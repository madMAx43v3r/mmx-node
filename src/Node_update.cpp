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
#include <mmx/utils.h>

#include <vnx/vnx.h>


namespace mmx {

void Node::check_vdfs()
{
	for(auto iter = pending_vdfs.begin(); iter != pending_vdfs.end();) {
		const auto& proof = iter->second;
		if(!vdf_verify_pending.count(proof->height)) {
			add_task([this, proof]() {
				handle(proof);
			});
			iter = pending_vdfs.erase(iter);
			continue;
		}
		iter++;
	}
}

void Node::add_fork(std::shared_ptr<fork_t> fork)
{
	const auto& block = fork->block;
	if(fork_tree.emplace(block->hash, fork).second) {
		fork_index.emplace(block->height, fork);
	}
	if(add_dummy_block(block)) {
		fork->has_dummy_block = true;
	}
}

bool Node::add_dummy_block(std::shared_ptr<const BlockHeader> prev)
{
	if(auto vdf_point = find_next_vdf_point(prev))
	{
		auto block = Block::create();
		block->version = 0;
		block->prev = prev->hash;
		block->height = prev->height + 1;
		block->time_diff = prev->time_diff;
		block->space_diff = calc_new_space_diff(params, prev->space_diff, params->score_threshold);
		block->vdf_iters = vdf_point->vdf_iters;
		block->vdf_output = vdf_point->output;

		hash_t vdf_challenge;
		if(find_vdf_challenge(prev, vdf_challenge, 1)) {
			const auto challenge = get_challenge(prev, vdf_challenge, 1);
			if(auto response = find_proof(challenge)) {
				block->proof = response->proof;
			}
		}
		block->finalize();
		add_block(block);
		return true;
	}
	return false;
}

void Node::update()
{
	const auto time_begin = vnx::get_wall_time_micros();

	check_vdfs();

	// add dummy blocks
	{
		std::vector<std::shared_ptr<const Block>> blocks;
		for(const auto& entry : fork_index) {
			const auto& fork = entry.second;
			if(!fork->has_dummy_block) {
				blocks.push_back(fork->block);
			}
		}
		for(const auto& block : blocks) {
			add_dummy_block(block);
		}
	}

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
					if(prev->is_invalid) {
						fork->is_invalid = true;
					}
					fork->prev = prev;
				}
				else if(is_synced) {
					// fetch missing previous
					const auto hash = block->prev;
					const auto height = block->height - 1;
					if(!fetch_pending.count(hash) && height > root->height)
					{
						fetch_pending.insert(hash);
						router->fetch_block(hash, nullptr,
								std::bind(&Node::fetch_result, this, hash, std::placeholders::_1),
								[this, hash](const vnx::exception&) {
									fetch_pending.erase(hash);
								});
						log(WARN) << "Fetching missed block at height " << height << " with hash " << hash;
					}
				}
			}
			if(!fork->diff_block) {
				fork->diff_block = find_diff_header(block);
			}
			if(fork->is_invalid || !fork->diff_block) {
				continue;
			}
			const auto prev = find_prev_header(block);
			if(!prev) {
				continue;	// wait for previous block
			}
			bool vdf_passed = false;
			if(auto point = find_vdf_point(block->height, prev->vdf_iters, block->vdf_iters, prev->vdf_output, block->vdf_output)) {
				vdf_passed = true;
				fork->is_vdf_verified = true;
				fork->vdf_point = point;
			}
			if(vdf_passed || !is_synced) {
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
		for(size_t i = 0; i + params->commit_delay < fork_line.size(); ++i)
		{
			const auto& fork = fork_line[i];
			const auto& block = fork->block;
			if(!fork->is_vdf_verified) {
				break;	// wait for VDF verify
			}
			if(!is_synced && fork_line.size() < max_fork_length) {
				// check if there is a competing fork at this height
				const auto finalized_height = peak->height - std::min(params->infuse_delay + 1, peak->height);
				if(		std::distance(fork_index.lower_bound(block->height), fork_index.upper_bound(block->height)) > 1
					&&	std::distance(fork_index.lower_bound(finalized_height), fork_index.upper_bound(finalized_height)) > 1)
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
			auto vdf_point = fork->vdf_point;
			log(INFO) << "New peak at height " << peak->height << " with score " << std::to_string(fork->proof_score)
					<< (is_synced && forked_at ? ", forked at " + std::to_string(forked_at->height) : "")
					<< (is_synced && vdf_point ? ", delay " + std::to_string((fork->recv_time - vdf_point->recv_time) / 1e6) + " sec" : "")
					<< ", took " << elapsed << " sec";
		}
	}

	if(!is_synced && sync_peak && sync_pending.empty())
	{
		if(sync_retry < num_sync_retries)
		{
			bool done = true;
			for(const auto& fork : get_fork_line()) {
				if(!fork->is_vdf_verified) {
					done = false;
					break;
				}
			}
			if(done) {
				log(INFO) << "Reached sync peak at height " << *sync_peak - 1;
				sync_pos = *sync_peak;
				sync_peak = nullptr;
				sync_retry++;
			}
		}
		else if(peak->height + params->commit_delay + 1 < *sync_peak)
		{
			const auto replay_height = peak->height - std::min<uint32_t>(1000, peak->height);
			vnx::write_config("Node.replay_height", replay_height);
			log(ERROR) << "Sync failed, it appears we have forked from the network a while ago, restarting with --Node.replay_height " << replay_height;
			exit();
			return;
		}
		else {
			is_synced = true;
			log(INFO) << "Finished sync at height " << peak->height;
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
			if(auto diff_block = find_diff_header(peak, i + 1))
			{
				if(auto prev = find_prev_header(peak, params->infuse_delay - i, true))
				{
					if(vdf_iters > 0) {
						infuse->values[vdf_iters] = prev->hash;
					}
				}
				vdf_iters += diff_block->time_diff * params->time_diff_constant;
			}
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
				if(auto diff_block = find_prev_header(prev, params->challenge_interval, true))
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
			if(auto diff_block = find_diff_header(peak, i + 1))
			{
				auto request = IntervalRequest::create();
				request->begin = vdf_iters;
				if(i == 0) {
					request->has_start = true;
					request->start_values = peak->vdf_output;
				}
				vdf_iters += diff_block->time_diff * params->time_diff_constant;
				request->end = vdf_iters;
				request->height = peak->height + i + 1;
				request->num_segments = params->num_vdf_segments;
				publish(request, output_interval_request);
			}
		}
	}

	// try to make a block
	{
		auto prev = peak;
		bool made_block = false;
		for(uint32_t i = 0; prev && i <= 1; ++i)
		{
			if(prev->height < root->height) {
				break;
			}
			hash_t vdf_challenge;
			if(find_vdf_challenge(prev, vdf_challenge, 1))
			{
				const auto challenge = get_challenge(prev, vdf_challenge, 1);
				if(auto response = find_proof(challenge)) {
					// check if it's our proof
					if(vnx::get_pipe(response->farmer_addr)) {
						const auto key = std::make_pair(prev->height + 1, prev->hash);
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
			prev = find_prev_header(prev);
		}
		if(made_block) {
			// update again right away
			add_task(std::bind(&Node::update, this));
		}
	}

	// publish challenges
	for(uint32_t i = 0; i <= params->challenge_delay; ++i)
	{
		hash_t vdf_challenge;
		if(find_vdf_challenge(peak, vdf_challenge, i))
		{
			const auto challenge = get_challenge(peak, vdf_challenge, i);

			auto value = Challenge::create();
			value->height = peak->height + i;
			value->challenge = challenge;
			if(auto diff_block = find_diff_header(peak, i)) {
				value->space_diff = diff_block->space_diff;
				publish(value, output_challenges);
			}
		}
	}
}

void Node::validate_pool()
{
	for(const auto& entry : validate_pending(params->max_block_cost, params->max_block_cost, true)) {
		publish(entry.tx, output_verified_transactions);
	}
}

std::vector<Node::tx_data_t> Node::validate_pending(const uint64_t verify_limit, const uint64_t select_limit, const bool only_new)
{
	const auto peak = get_peak();
	if(!peak) {
		return {};
	}
	const auto time_begin = vnx::get_wall_time_micros();

	std::vector<tx_data_t> all_tx;

	for(const auto& iter : tx_pool) {
		tx_data_t tmp;
		tmp.tx_pool_t::operator=(iter.second);
		all_tx.push_back(tmp);
	}

	// check for duplicate transactions
#pragma omp parallel for
	for(int i = 0; i < int(all_tx.size()); ++i)
	{
		auto& entry = all_tx[i];
		if(tx_map.count(entry.tx->id) || tx_index.find(entry.tx->id)) {
			entry.duplicate = true;
		}
	}

	// purge duplicates from pool first to avoid detecting false dependency
	size_t num_duplicate = 0;
	for(const auto& entry : all_tx) {
		if(entry.duplicate) {
			num_duplicate += tx_pool.erase(entry.tx->id);
		}
	}

	// check for dependency between transactions
#pragma omp parallel for
	for(int i = 0; i < int(all_tx.size()); ++i)
	{
		auto& entry = all_tx[i];
		if(entry.duplicate) {
			continue;
		}
		for(const auto& in : entry.tx->inputs) {
			const auto& prev = in.prev.txid;
			// check if tx depends on another one which is not in a block yet
			if(tx_pool.count(prev)) {
				entry.depends.push_back(prev);
			}
		}
		entry.cost = entry.tx->calc_cost(params);
	}

	// sort transactions by fee ratio known from previous iterations
	std::sort(all_tx.begin(), all_tx.end(),
		[](const tx_data_t& lhs, const tx_data_t& rhs) -> bool {
			return lhs.fee_ratio > rhs.fee_ratio;
		});

	size_t num_purged = 0;
	size_t num_dependent = 0;
	uint64_t total_verify_cost = 0;
	std::vector<tx_data_t> tx_list;
	std::unordered_multimap<hash_t, hash_t> dependency;

	{
		// purge transactions from pool if overflowing
		uint64_t total_pool_cost = 0;
		for(const auto& entry : all_tx) {
			if(entry.did_validate) {
				total_pool_cost += entry.cost;
			}
		}
		for(auto iter = all_tx.rbegin(); iter != all_tx.rend(); ++iter)
		{
			if(total_pool_cost <= tx_pool_limit * params->max_block_cost) {
				break;
			}
			if(iter->did_validate) {
				// only purge transactions where fee ratio is known already
				num_purged += tx_pool.erase(iter->tx->id);
				total_pool_cost -= iter->cost;
				iter->purged = true;
			}
		}
	}

	// select transactions to verify
	for(const auto& entry : all_tx) {
		if(entry.duplicate || entry.purged) {
			continue;
		}
		for(const auto& prev : entry.depends) {
			dependency.emplace(prev, entry.tx->id);
		}
		if(only_new && entry.did_validate) {
			continue;
		}
		if(entry.depends.empty()) {
			if(total_verify_cost + entry.cost <= verify_limit) {
				tx_list.push_back(entry);
				total_verify_cost += entry.cost;
			}
		} else {
			num_dependent++;
		}
	}
	auto context = Context::create();
	context->height = peak->height + 1;

	// verify transactions in parallel
#pragma omp parallel for
	for(int i = 0; i < int(tx_list.size()); ++i)
	{
		auto& entry = tx_list[i];
		auto& tx = entry.tx;
		try {
			if(!tx->exec_outputs.empty()) {
				auto copy = vnx::clone(tx);
				copy->exec_outputs.clear();
				tx = copy;
			}
			if(auto new_tx = validate(tx, context, nullptr, entry.fees)) {
				tx = new_tx;
			}
			entry.fee_ratio = entry.fees / double(entry.cost);
			{
				auto iter = tx_pool.find(tx->id);
				if(iter != tx_pool.end()) {
					auto& info = iter->second;
					info.did_validate = true;
					info.fee_ratio = entry.fee_ratio;
				}
			}
		}
		catch(const std::exception& ex) {
			if(show_warnings) {
				log(WARN) << "TX validation failed with: " << ex.what();
			}
			entry.invalid = true;
		}
	}

	// sort transactions by fee ratio
	std::sort(tx_list.begin(), tx_list.end(),
		[](const tx_data_t& lhs, const tx_data_t& rhs) -> bool {
			return lhs.fee_ratio > rhs.fee_ratio;
		});

	uint64_t total_cost = 0;
	std::vector<tx_data_t> result;
	std::unordered_set<addr_t> mutated;
	std::unordered_set<txio_key_t> spent;

	// select final set of transactions
	for(auto& entry : tx_list)
	{
		if(entry.invalid) {
			continue;
		}
		if(total_cost + entry.cost <= select_limit)
		{
			bool passed = true;
			for(const auto& in : entry.tx->inputs) {
				// prevent double spending
				if(!spent.insert(in.prev).second) {
					entry.invalid = true;
					passed = false;
				}
			}
			{
				std::unordered_set<addr_t> addr_set;
				for(const auto& op : entry.tx->execute) {
					if(std::dynamic_pointer_cast<const operation::Mutate>(op)) {
						addr_set.insert(op->address);
					}
				}
				for(const auto& addr : addr_set) {
					// prevent concurrent mutation
					if(!mutated.insert(addr).second) {
						passed = false;
					}
				}
			}
			if(passed) {
				result.push_back(entry);
				total_cost += entry.cost;
			}
		}
	}

	// purge invalid transactions
	size_t num_invalid = 0;
	{
		std::vector<hash_t> invalid;
		for(const auto& entry : tx_list) {
			if(entry.invalid) {
				invalid.push_back(entry.tx->id);
			}
		}
		while(!invalid.empty()) {
			std::vector<hash_t> more;
			for(const auto& id : invalid) {
				const auto range = dependency.equal_range(id);
				for(auto iter = range.first; iter != range.second; ++iter) {
					more.push_back(iter->second);
				}
				num_invalid += tx_pool.erase(id);
			}
			invalid = more;
		}
	}

	if(!only_new || !tx_list.empty()) {
		const auto elapsed = (vnx::get_wall_time_micros() - time_begin) / 1e6;
		log(INFO) << "Validated" << (only_new ? " new" : "") << " transactions: " << result.size() << " valid, "
				<< num_invalid << " invalid, " << num_duplicate << " duplicate, " << num_dependent << " dependent, "
				<< num_purged << " purged, took " << elapsed << " sec";
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
		total_fees += entry.fees;
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
