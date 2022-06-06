/*
 * Node_update.cpp
 *
 *  Created on: Jan 20, 2022
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/FarmerClient.hxx>
#include <mmx/TimeInfusion.hxx>
#include <mmx/IntervalRequest.hxx>
#include <mmx/operation/Mutate.hxx>
#include <mmx/operation/Revoke.hxx>
#include <mmx/operation/Execute.hxx>
#include <mmx/utils.h>

#include <vnx/vnx.h>


namespace mmx {

void Node::verify_vdfs()
{
	for(auto iter = pending_vdfs.begin(); iter != pending_vdfs.end();) {
		const auto& proof = iter->second;
		if(!vdf_verify_pending.count(proof->height)) {
			add_task([this, proof]() {
				handle(proof);
			});
			iter = pending_vdfs.erase(iter);
		} else {
			iter++;
		}
	}
}

void Node::add_dummy_blocks(std::shared_ptr<const BlockHeader> prev)
{
	if(auto vdf_point = find_next_vdf_point(prev))
	{
		std::vector<std::shared_ptr<const ProofOfSpace>> proofs = {nullptr};
		{
			hash_t vdf_challenge;
			if(find_vdf_challenge(prev, vdf_challenge, 1)) {
				const auto challenge = get_challenge(prev, vdf_challenge, 1);
				for(const auto& response : find_proof(challenge)) {
					proofs.push_back(response->proof);
				}
			}
		}
		const auto diff_block = get_diff_header(prev, 1);

		for(const auto& proof : proofs) {
			auto block = Block::create();
			block->version = 0;
			block->prev = prev->hash;
			block->height = prev->height + 1;
			block->proof = proof;
			block->time_diff = prev->time_diff;
			block->space_diff = calc_new_space_diff(params, prev->space_diff, proof ? proof->score : params->score_threshold);
			block->vdf_iters = vdf_point->vdf_iters;
			block->vdf_output = vdf_point->output;
			block->weight = calc_block_weight(params, diff_block, block, false);
			block->total_weight = prev->total_weight + block->weight;
			block->finalize();
			add_block(block);
		}
	}
}

void Node::update()
{
	const auto time_begin = vnx::get_wall_time_micros();

	verify_vdfs();

	// verify proof responses
	for(auto iter = pending_proofs.begin(); iter != pending_proofs.end();) {
		if(verify(*iter)) {
			iter = pending_proofs.erase(iter);
		} else {
			iter++;
		}
	}

	// pre-validate new blocks
#pragma omp parallel for if(!is_synced)
	for(int i = 0; i < int(pending_forks.size()); ++i)
	{
		const auto& fork = pending_forks[i];
		const auto& block = fork->block;
		try {
			if(!block->is_valid()) {
				throw std::logic_error("invalid block");
			}
			block->validate();
		}
		catch(const std::exception& ex) {
			fork->is_invalid = true;
			log(WARN) << "Pre-validation failed for a block at height " << block->height << " with: " << ex.what();
		}
	}
	for(const auto& fork : pending_forks) {
		if(!fork->is_invalid) {
			add_fork(fork);
		}
	}
	pending_forks.clear();

	// verify proof where possible
	std::vector<std::shared_ptr<fork_t>> to_verify;
	{
		const auto root = get_root();
		for(const auto& entry : fork_index) {
			const auto& fork = entry.second;
			if(fork->is_proof_verified) {
				continue;
			}
			const auto& block = fork->block;
			if(!fork->prev.lock()) {
				if(auto prev = find_fork(block->prev)) {
					fork->prev = prev;
				} else if(is_synced) {
					// fetch missing previous
					const auto hash = block->prev;
					const auto height = block->height - 1;
					if(!fetch_pending.count(hash) && height > root->height) {
						fetch_block(hash);
						log(WARN) << "Fetching missed block at height " << height << " with hash " << hash;
					}
				}
			}
			if(!fork->diff_block) {
				fork->diff_block = find_diff_header(block);
			}
			if(auto prev = fork->prev.lock()) {
				fork->is_invalid = prev->is_invalid;
				fork->is_connected = prev->is_connected;
			} else if(block->prev == root->hash) {
				fork->is_connected = true;
			}
			const auto prev = find_prev_header(block);
			if(!prev || fork->is_invalid || !fork->diff_block || !fork->is_connected) {
				continue;
			}
			if(auto point = find_vdf_point(block->height, prev->vdf_iters, block->vdf_iters, prev->vdf_output, block->vdf_output)) {
				fork->vdf_point = point;
				fork->is_vdf_verified = true;
			}
			if(fork->vdf_point || !is_synced) {
				to_verify.push_back(fork);
			}
		}
	}

#pragma omp parallel for if(!is_synced)
	for(int i = 0; i < int(to_verify.size()); ++i)
	{
		const auto& fork = to_verify[i];
		const auto& block = fork->block;
		try {
			hash_t vdf_challenge;
			if(find_vdf_challenge(block, vdf_challenge)) {
				verify_proof(fork, vdf_challenge);
			}
		}
		catch(const std::exception& ex) {
			fork->is_invalid = true;
			log(WARN) << "Proof verification failed for a block at height " << block->height << " with: " << ex.what();
		}
	}
	const auto prev_peak = get_peak();

	std::shared_ptr<const BlockHeader> forked_at;

	// choose best fork
	while(true) {
		forked_at = nullptr;

		const auto best_fork = find_best_fork();

		if(!best_fork || best_fork->block->hash == state_hash) {
			break;	// no change
		}

		// verify and apply new fork
		try {
			forked_at = fork_to(best_fork);
		}
		catch(...) {
			continue;	// try again
		}
		purge_tree();

		const auto peak = best_fork->block;
		const auto fork_line = get_fork_line();

		// commit to disk
		const auto commit_delay = is_synced || sync_retry ? params->commit_delay : max_sync_ahead;
		for(size_t i = 0; i + commit_delay < fork_line.size(); ++i)
		{
			const auto& fork = fork_line[i];
			const auto& block = fork->block;
			if(!fork->is_vdf_verified) {
				break;	// wait for VDF verify
			}
			if(!is_synced && fork_line.size() < max_fork_length) {
				// check if there is a competing fork at this height
				if(		std::distance(fork_index.lower_bound(block->height), fork_index.upper_bound(block->height)) > 1
					&&	std::distance(fork_index.lower_bound(peak->height), fork_index.upper_bound(peak->height)) > 1)
				{
					break;
				}
			}
			commit(block);
		}
		break;
	}

	const auto peak = get_peak();
	if(!peak) {
		log(WARN) << "Have no peak!";
		return;
	}
	const auto root = get_root();
	const auto elapsed = (vnx::get_wall_time_micros() - time_begin) / 1e6;

	if(!prev_peak || peak->hash != prev_peak->hash)
	{
		stuck_timer->reset();
		if(auto fork = find_fork(peak->hash))
		{
			auto proof = fork->block->proof;
			auto vdf_point = fork->vdf_point;
			log(INFO) << "New peak at height " << peak->height
					<< " with score " << (proof ? proof->score : params->score_threshold) << (peak->farmer_sig ? "" : " (dummy)")
					<< (is_synced && forked_at ? ", forked at " + std::to_string(forked_at->height) : "")
					<< (is_synced && vdf_point ? ", delay " + std::to_string((fork->recv_time - vdf_point->recv_time) / 1e6) + " sec" : "")
					<< ", took " << elapsed << " sec";
		}
	}

	if(!is_synced && sync_peak && sync_pending.empty())
	{
		if(sync_retry < num_sync_retries)
		{
			log(INFO) << "Reached sync peak at height " << *sync_peak - 1;
			sync_pos = *sync_peak;
			sync_peak = nullptr;
			sync_retry++;
		}
		else {
			log(INFO) << "Finished sync at height " << peak->height;
			is_synced = true;
		}
	}
	if(!is_synced) {
		sync_more();
		update_timer->reset();
		return;
	}
	{
		// publish time infusions for VDF 0
		auto infuse = TimeInfusion::create();
		infuse->chain = 0;
		auto vdf_iters = peak->vdf_iters;
		for(uint32_t i = 0; i <= params->infuse_delay; ++i)
		{
			if(auto prev = find_prev_header(peak, params->infuse_delay - i))
			{
				infuse->values[vdf_iters] = prev->hash;
			}
			auto diff_block = get_diff_header(peak, i + 1);
			vdf_iters += diff_block->time_diff * params->time_diff_constant;
		}
		publish(infuse, output_timelord_infuse);
	}
	{
		// publish next time infusion for VDF 1
		uint32_t height = peak->height;
		height -= (height % params->challenge_interval);
		if(auto prev = find_prev_header(peak, peak->height - height))
		{
			if(prev->height >= params->challenge_interval)
			{
				if(auto diff_block = find_prev_header(prev, params->challenge_interval))
				{
					auto infuse = TimeInfusion::create();
					infuse->chain = 1;
					const auto vdf_iters = prev->vdf_iters + diff_block->time_diff * params->time_diff_constant * params->infuse_delay;
					infuse->values[vdf_iters] = prev->hash;
					publish(infuse, output_timelord_infuse);
				}
			}
		}
	}
	{
		// request next VDF proofs
		auto vdf_iters = peak->vdf_iters;
		for(uint32_t i = 0; i < params->infuse_delay; ++i)
		{
			auto request = IntervalRequest::create();
			request->begin = vdf_iters;
			if(i == 0) {
				request->has_start = true;
				request->start_values = peak->vdf_output;
			}
			auto diff_block = get_diff_header(peak, i + 1);
			vdf_iters += diff_block->time_diff * params->time_diff_constant;
			request->end = vdf_iters;
			request->height = peak->height + i + 1;
			request->num_segments = params->num_vdf_segments;
			publish(request, output_interval_request);
		}
	}

	// try to make a block
	bool made_block = false;
	for(uint32_t i = 0; i < params->infuse_delay && i <= peak->height; ++i)
	{
		const auto height = peak->height - i;
		if(height < root->height) {
			break;
		}
		if(auto fork = find_best_fork(height)) {
			const auto& prev = fork->block;
			hash_t vdf_challenge;
			if(find_vdf_challenge(prev, vdf_challenge, 1)) {
				const auto challenge = get_challenge(prev, vdf_challenge, 1);
				for(const auto& response : find_proof(challenge)) {
					// check if it's our proof
					if(vnx::get_pipe(response->farmer_addr)) {
						const auto key = std::make_pair(prev->hash, response->proof->calc_hash());
						if(!created_blocks.count(key)) {
							try {
								if(auto block = make_block(prev, response)) {
									add_block(block);
									made_block = true;
									created_blocks[key] = block->hash;
								}
							}
							catch(const std::exception& ex) {
								log(WARN) << "Failed to create a block: " << ex.what();
							}
							// revert back to peak
							fork_to(peak->hash);
						}
					}
				}
			}
		}
	}
	if(made_block) {
		// update again right away
		add_task(std::bind(&Node::update, this));
	}

	// publish challenges
	for(uint32_t i = 0; i <= params->challenge_delay; ++i)
	{
		if(auto vdf_block = find_prev_header(peak, params->challenge_delay - i))
		{
			auto value = Challenge::create();
			value->height = peak->height + i;
			value->challenge = get_challenge(peak, vdf_block->vdf_output[1], i);
			value->space_diff = get_diff_header(peak, i)->space_diff;
			publish(value, output_challenges);
		}
	}
}

void Node::validate_pool()
{
	for(const auto& entry : validate_pending(params->max_block_cost, params->max_block_cost, true)) {
		publish(entry.tx, output_verified_transactions);
	}
}

std::vector<Node::tx_pool_t> Node::validate_pending(const uint64_t verify_limit, const uint64_t select_limit, const bool only_new)
{
	const auto peak = get_peak();
	if(!peak) {
		return {};
	}
	const auto time_begin = vnx::get_wall_time_micros();

	std::vector<tx_pool_t> all_tx;
	for(const auto& entry : tx_pool) {
		all_tx.push_back(entry.second);
	}
	for(const auto& entry : pending_transactions) {
		if(const auto& tx = entry.second) {
			tx_pool_t out;
			out.tx = tx;
			out.cost = tx->calc_cost(params);
			out.full_hash = entry.first;
			all_tx.push_back(out);
		}
	}

	// sort transactions by fee ratio
	std::sort(all_tx.begin(), all_tx.end(),
		[](const tx_pool_t& lhs, const tx_pool_t& rhs) -> bool {
			return lhs.tx->fee_ratio > rhs.tx->fee_ratio;
		});

	size_t num_purged = 0;
	uint128_t total_pool_cost = 0;
	uint128_t total_verify_cost = 0;
	std::vector<tx_pool_t> tx_list;

	// purge transactions from pool if overflowing
	{
		vnx::optional<size_t> cutoff;
		for(size_t i = 0; i < all_tx.size(); ++i) {
			const auto& entry = all_tx[i];
			total_pool_cost += entry.cost;
			if(total_pool_cost > uint128_t(tx_pool_limit) * params->max_block_cost) {
				if(!cutoff) {
					*cutoff = i;
				}
				tx_pool.erase(entry.tx->id);
				num_purged++;
			}
		}
		if(cutoff) {
			all_tx.resize(*cutoff);
		}
	}
	auto context = new_exec_context();
	{
		auto base = Context::create();
		base->height = peak->height + 1;
		context->block = base;
	}

	// select transactions to verify
	for(const auto& entry : all_tx) {
		if(only_new && entry.is_valid && entry.last_check + tx_verify_interval > peak->height) {
			continue;
		}
		if(uint32_t(entry.tx->id.to_uint256() & 0x1) != (context->block->height & 0x1)) {
			continue;
		}
		if(total_verify_cost + entry.cost <= verify_limit) {
			tx_list.push_back(entry);
			total_verify_cost += entry.cost;
		}
	}

	// prepare synchronization
	for(const auto& entry : tx_list) {
		setup_context(context, entry.tx);
	}

	// verify transactions in parallel
#pragma omp parallel for
	for(int i = 0; i < int(tx_list.size()); ++i)
	{
		auto& entry = tx_list[i];
		auto& tx = entry.tx;
		context->wait(tx->id);
		try {
			if(auto new_tx = validate(tx, context, nullptr, entry.fee, entry.cost)) {
				tx = new_tx;
			}
			entry.is_valid = true;
		}
		catch(const std::exception& ex) {
			if(show_warnings) {
				log(WARN) << "TX validation failed with: " << ex.what() << " (" << tx->id << ")";
			}
			entry.is_valid = false;
		}
		entry.last_check = peak->height;
		context->signal(tx->id);
	}

	// update tx_pool
	size_t num_invalid = 0;
	for(const auto& entry : tx_list) {
		const auto& tx = entry.tx;
		if(entry.is_valid) {
			tx_pool[tx->id] = entry;
		} else {
			num_invalid++;
		}
		// erase from pool if it didn't came from pending map (and is invalid)
		if(!pending_transactions.erase(entry.full_hash) && !entry.is_valid) {
			tx_pool.erase(tx->id);
		}
	}

	uint128_t total_cost = 0;
	std::vector<tx_pool_t> result;
	std::unordered_set<addr_t> tx_set;
	std::multiset<std::pair<hash_t, addr_t>> revoked;
	balance_cache_t balance_cache(&balance_map);

	// select final set of transactions
	for(auto& entry : tx_list)
	{
		if(!entry.is_valid || total_cost + entry.cost > select_limit) {
			continue;
		}
		bool passed = true;
		std::unordered_set<addr_t> tmp_set;
		std::multiset<std::pair<hash_t, addr_t>> tmp_revoke;
		balance_cache_t tmp_cache(&balance_cache);
		{
			auto tx = entry.tx;
			while(tx) {
				if(tx_set.count(tx->id)) {
					passed = false;
				}
				tmp_set.insert(tx->id);

				if(tx->sender) {
					const auto key = std::make_pair(tx->id, *tx->sender);
					if(revoked.count(key) || tmp_revoke.count(key)) {
						passed = false;
					}
				}
				for(const auto& in : tx->get_inputs()) {
					const auto balance = tmp_cache.get(in.address, in.contract);
					if(balance && in.amount <= *balance) {
						*balance -= in.amount;
					} else {
						passed = false;
					}
				}
				for(const auto& op : tx->execute) {
					if(auto revoke = std::dynamic_pointer_cast<const operation::Revoke>(op)) {
						if(tx_set.count(revoke->txid) || tmp_set.count(revoke->txid)) {
							passed = false;
						}
						tmp_revoke.emplace(revoke->txid, revoke->address);
					}
				}
				tx = tx->parent;
			}
		}
		if(!passed) {
			continue;
		}
		tx_set.insert(tmp_set.begin(), tmp_set.end());
		revoked.insert(tmp_revoke.begin(), tmp_revoke.end());
		balance_cache.apply(tmp_cache);

		total_cost += entry.cost;
		result.push_back(entry);
	}

	if(!only_new || !tx_list.empty()) {
		const auto elapsed = (vnx::get_wall_time_micros() - time_begin) / 1e6;
		log(INFO) << "Validated" << (only_new ? " new" : "") << " transactions: " << result.size() << " valid, "
				<< num_invalid << " invalid, " << num_purged << " purged, took " << elapsed << " sec";
	}
	return result;
}

std::shared_ptr<const Block> Node::make_block(std::shared_ptr<const BlockHeader> prev, std::shared_ptr<const ProofResponse> response)
{
	const auto time_begin = vnx::get_wall_time_micros();

	// find VDF output
	const auto vdf_point = find_next_vdf_point(prev);
	if(!vdf_point) {
		return nullptr;
	}

	// reset state to previous block
	fork_to(prev->hash);

	auto block = Block::create();
	block->prev = prev->hash;
	block->height = prev->height + 1;
	block->time_diff = prev->time_diff;
	block->space_diff = prev->space_diff;
	block->vdf_iters = vdf_point->vdf_iters;
	block->vdf_output = vdf_point->output;
	block->proof = response->proof;

	const auto tx_list = validate_pending(2 * params->max_block_cost, params->max_block_cost, false);

	// select transactions
	uint64_t total_fees = 0;
	for(const auto& entry : tx_list) {
		block->tx_list.push_back(entry.tx);
		total_fees += entry.fee;
	}

	const auto prev_fork = find_fork(prev->hash);

	// set new time difficulty
	if(auto fork = find_prev_fork(prev_fork, params->infuse_delay))
	{
		if(auto point = fork->vdf_point) {
			const int64_t time_delta = (vdf_point->recv_time - point->recv_time) / (params->infuse_delay + 1);
			if(time_delta > 0) {
				const double gain = 0.1;
				if(auto diff_block = fork->diff_block) {
					double new_diff = params->block_time * diff_block->time_diff / (time_delta * 1e-6);
					new_diff = prev->time_diff * (1 - gain) + new_diff * gain;
					block->time_diff = std::max<uint64_t>(std::max<int64_t>(new_diff + 0.5, 1), params->min_time_diff);
				}
			}
		}
	}
	{
		// limit time diff update
		const auto max_update = std::max<uint64_t>(prev->time_diff >> params->max_diff_adjust, 1);
		block->time_diff = std::min(block->time_diff, prev->time_diff + max_update);
		block->time_diff = std::max(block->time_diff, prev->time_diff - max_update);
	}
	{
		// set new space difficulty
		block->space_diff = calc_new_space_diff(params, prev->space_diff, response->proof->score);
	}
	const auto diff_block = get_diff_header(prev, 1);
	block->weight = calc_block_weight(params, diff_block, block, true);
	block->total_weight = prev->total_weight + block->weight;
	block->finalize();

	FarmerClient farmer(response->farmer_addr);
	const auto block_reward = calc_block_reward(block);
	const auto final_reward = calc_final_block_reward(params, block_reward, total_fees);
	const auto result = farmer.sign_block(block, final_reward);

	if(!result) {
		throw std::logic_error("farmer refused to sign block");
	}
	block->BlockHeader::operator=(*result);

	const auto elapsed = (vnx::get_wall_time_micros() - time_begin) / 1e6;
	log(INFO) << "Created block at height " << block->height << " with: ntx = " << block->tx_list.size()
			<< ", score = " << response->proof->score << ", reward = " << final_reward / pow(10, params->decimals) << " MMX"
			<< ", nominal = " << block_reward / pow(10, params->decimals) << " MMX"
			<< ", fees = " << total_fees / pow(10, params->decimals) << " MMX"
			<< ", took " << elapsed << " sec";
	return block;
}


} // mmx
