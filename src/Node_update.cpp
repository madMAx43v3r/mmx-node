/*
 * Node_update.cpp
 *
 *  Created on: Jan 20, 2022
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/FarmerClient.hxx>
#include <mmx/Challenge.hxx>
#include <mmx/IntervalRequest.hxx>
#include <mmx/ProofOfSpaceOG.hxx>
#include <mmx/ProofOfSpaceNFT.hxx>
#include <mmx/ProofOfStake.hxx>
#include <mmx/contract/VirtualPlot.hxx>
#include <mmx/operation/Execute.hxx>
#include <mmx/operation/Deposit.hxx>
#include <mmx/utils.h>

#include <vnx/vnx.h>
#include <random>


namespace mmx {

void Node::verify_vdfs()
{
	if(vdf_queue.empty() || vdf_verify_pending.size() >= vdf_verify_max_pending) {
		return;
	}
	const auto time_now = vnx::get_wall_time_millis();
	const auto vdf_timeout = 10 * params->block_interval_ms;
	const auto root = get_root();

	struct vdf_fork_t {
		int64_t trust = 0;
		std::shared_ptr<const ProofOfTime> proof;
	};
	std::vector<std::shared_ptr<vdf_fork_t>> try_now;
	std::vector<std::pair<std::shared_ptr<const ProofOfTime>, int64_t>> try_again;

	for(const auto& entry : vdf_queue)
	{
		const auto& proof = entry.first;
		if(proof->start < root->vdf_iters) {
			continue;		// too old
		}
		if(time_now - entry.second > vdf_timeout) {
			continue;		// timeout
		}
		if(find_vdf_point(proof->input, proof->get_output())) {
			continue;		// already verified
		}
		const auto prev = get_header(proof->prev);
		const auto prev_fork = find_fork(proof->prev);
		if(!prev || (prev_fork && !prev_fork->is_all_proof_verified)) {
			try_again.emplace_back(proof, entry.second);	// wait for previous blocks
			continue;
		}
		auto out = std::make_shared<vdf_fork_t>();
		{
			auto iter = timelord_trust.find(proof->timelord_key);
			if(iter != timelord_trust.end()) {
				out->trust = iter->second;
			}
		}
		out->proof = proof;
		try_now.emplace_back(out);
	}

	std::sort(try_now.begin(), try_now.end(),
		[]( const std::shared_ptr<vdf_fork_t>& L, const std::shared_ptr<vdf_fork_t>& R) -> bool {
			return L->trust > R->trust;
		});

	for(const auto& fork : try_now)
	{
		const auto& proof = fork->proof;
		if(vdf_verify_pending.count(proof->hash)) {
			continue;		// duplicate
		}
		if(vdf_verify_pending.size() >= vdf_verify_max_pending) {
			try_again.emplace_back(proof, time_now);
			continue;
		}
		try {
			verify_vdf(proof);
		} catch(const std::exception& ex) {
			log(WARN) << "VDF static verification for height " << proof->vdf_height << " failed with: " << ex.what();
		}
	}
	vdf_queue = std::move(try_again);
}

void Node::verify_proofs()
{
	const auto time_now = vnx::get_wall_time_millis();
	const auto proof_timeout = 20 * params->block_interval_ms;

	std::vector<std::pair<std::shared_ptr<const ProofResponse>, int64_t>> try_again;

	for(const auto& entry : proof_queue) {
		if(time_now - entry.second > proof_timeout) {
			continue;		// timeout
		}
		const auto& response = entry.first;
		try {
			if(!verify(response)) {
				try_again.emplace_back(response, entry.second);
			}
		} catch(const std::exception& ex) {
			log(WARN) << "Got invalid proof for VDF height " << response->vdf_height << ": " << ex.what();
		} catch(...) {
			// ignore
		}
	}
	proof_queue = std::move(try_again);
}

void Node::verify_block_proofs()
{
	std::mutex mutex;
	const auto root = get_root();

	for(const auto& entry : fork_index)
	{
		const auto& fork = entry.second;
		if(fork->is_proof_verified) {
			continue;
		}
		const auto& block = fork->block;
		if(!fork->prev.lock()) {
			fork->prev = find_fork(block->prev);
		}
		if(auto prev = fork->prev.lock()) {
			if(prev->is_invalid) {
				fork->is_invalid = true;
			}
			fork->is_connected = prev->is_connected;
		} else if(block->prev == root->hash) {
			fork->is_connected = true;
		}
		const auto prev = find_prev_header(block);
		if(!prev || fork->is_invalid || !fork->is_connected) {
			continue;
		}
		if(!fork->is_vdf_verified) {
			const auto vdf_points = find_vdf_points(block);
			if(vdf_points.size()) {
				fork->vdf_points = vdf_points;
				fork->is_vdf_verified = true;
			}
		}
		// we don't verify VDFs here if still syncing
		// instead it's done later when needed (competing forks)
		if(fork->is_vdf_verified || !is_synced)
		{
			threads->add_task([this, fork, &mutex]() {
				const auto& block = fork->block;
				try {
					verify_proof(fork);

					if(auto proof = block->proof) {
						std::lock_guard<std::mutex> lock(mutex);
						add_proof(proof, block->vdf_height, vnx::Hash64());
					}
				} catch(const std::exception& ex) {
					fork->is_invalid = true;
					log(WARN) << "Proof verification failed for a block at height " << block->height << ": " << ex.what();
				}
			});
		}
	}
	threads->sync();
}

void Node::update()
{
	std::unique_lock lock(db_mutex);

	const auto time_begin = vnx::get_wall_time_millis();
	update_pending = false;

	verify_vdfs();

	verify_proofs();

	verify_block_proofs();

	const auto prev_peak = get_peak();
	std::shared_ptr<const BlockHeader> forked_at;

	// choose best fork
	while(vnx::do_run())
	{
		forked_at = nullptr;
		const auto best_fork = find_best_fork();

		if(!best_fork || best_fork->block->hash == state_hash) {
			break;	// no change
		}

		// verify and apply new fork
		try {
			forked_at = fork_to(best_fork);
		} catch(const std::exception& ex) {
			best_fork->is_invalid = true;
			log(WARN) << "Forking to height " << best_fork->block->height << " failed with: " << ex.what();
			continue;	// try again
		}
		break;
	}

	const auto peak = get_peak();
	if(!peak) {
		log(WARN) << "Have no peak!";
		return;
	}
	{
		// commit to disk
		const auto fork_line = get_fork_line();

		for(size_t i = 0; i + params->commit_delay < fork_line.size(); ++i)
		{
			const auto& fork = fork_line[i];
			const auto& block = fork->block;
			if(!fork->is_vdf_verified) {
				break;	// wait for VDF verify
			}
			commit(block);
		}
	}
	const auto root = get_root();
	const auto now_ms = vnx::get_wall_time_millis();
	const auto elapsed = (now_ms - time_begin) / 1e3;

	if(!prev_peak || peak->hash != prev_peak->hash)
	{
		if(auto fork = find_fork(peak->hash))
		{
			std::stringstream msg;
			msg << u8"\U0001F4BE New peak at height " << peak->height << " / " << peak->vdf_height << " with score ";
			if(peak->proof) {
				msg << peak->proof->score;
			} else {
				msg << "N/A";
			}
			if(is_synced) {
				if(forked_at) {
					msg << ", forked at " << forked_at->height;
				}
				if(fork->vdf_points.size()) {
					msg << ", delay " << (fork->recv_time - fork->vdf_points.back()->recv_time) / 1e3 << " sec";
				}
			} else {
				msg << ", " << sync_pending.size() << " pending";
				if(auto count = vdf_threads->get_num_pending_total()) {
					msg << ", " << count << " vdf checks";
				}
			}
			msg << ", took " << elapsed << " sec";
			log(INFO) << msg.str();
		}
		stuck_timer->reset();
	}

	if(!is_synced && sync_peak && sync_pending.empty())
	{
		if(sync_retry < num_sync_retries) {
			if(now_ms - sync_finish_ms > params->block_interval_ms / 2) {
				sync_pos = *sync_peak - 1;
				sync_peak = nullptr;
				sync_finish_ms = now_ms;
				sync_retry++;
				log(INFO) << "Reached sync peak at height " << sync_pos;
			}
		} else {
			is_synced = true;
			on_sync_done(peak->height);
		}
	}
	if(!is_synced) {
		sync_more();
		update_timer->reset();
		return;
	}

	{
		const auto vdf_points = find_next_vdf_points(peak);
		const auto vdf_advance = std::min<uint32_t>(
				vdf_points.size() + params->infuse_delay, params->max_vdf_count);

		// request next VDF proofs
		auto vdf_iters = peak->vdf_iters;
		for(uint32_t i = 0; i <= vdf_advance; ++i)
		{
			uint64_t num_iters = 0;
			const auto infuse = get_infusion(peak, i, num_iters);

			auto request = IntervalRequest::create();
			request->vdf_height = peak->vdf_height + i + 1;
			request->start = vdf_iters;
			request->end = vdf_iters + num_iters;
			if(i == 0) {
				request->input = peak->vdf_output;
			}
			request->infuse = infuse;

			publish(request, output_interval_request);

			vdf_iters += num_iters;
		}
		const auto challenge_advance = std::min<uint32_t>(
				vdf_points.size() + params->challenge_delay, params->max_vdf_count);

		// publish challenges
		for(uint32_t i = 1; i <= challenge_advance; ++i)
		{
			uint64_t space_diff = 0;
			const auto challenge = get_challenge(peak, i, space_diff);

			auto value = Challenge::create();
			value->vdf_height = peak->vdf_height + i;
			value->challenge = challenge;
			value->difficulty = space_diff;

			publish(value, output_challenges);
		}
	}

	// try to replace current peak
	// in case another farmer made a block when they shouldn't
	{
		hash_t challenge;
		uint64_t space_diff = 0;
		if(find_challenge(peak, 0, challenge, space_diff))
		{
			if(auto proof = find_best_proof(challenge))
			{
				if(vnx::get_pipe(proof->farmer_mac))
				{
					if(!created_blocks.count(proof->hash))
					{
						if(auto prev = find_prev_header(peak))
						{
							const auto vdf_points = find_next_vdf_points(prev);
							if(vdf_points.size()) {
								try {
									if(auto block = make_block(prev, vdf_points, *proof)) {
										add_block(block);
									}
								} catch(const std::exception& ex) {
									log(WARN) << "Failed to create block at height " << peak->height << ": " << ex.what();
								}
								fork_to(peak->hash);
							}
						}
					}
				}
			}
		}
	}

	// try to extend peak
	{
		const auto vdf_points = find_next_vdf_points(peak);
		if(vdf_points.size()) {
			hash_t challenge;
			uint64_t space_diff = 0;
			if(find_challenge(peak, vdf_points.size(), challenge, space_diff))
			{
				if(auto proof = find_best_proof(challenge))
				{
					if(vnx::get_pipe(proof->farmer_mac))
					{
						if(!created_blocks.count(proof->hash)) {
							try {
								if(auto block = make_block(peak, vdf_points, *proof)) {
									add_block(block);
								}
							} catch(const std::exception& ex) {
								log(WARN) << "Failed to create block at height " << peak->height + 1 << ": " << ex.what();
							}
							fork_to(peak->hash);
						}
					}
				}
			}
		}
	}
}

void Node::on_sync_done(const uint32_t height)
{
	log(INFO) << "Finished sync at height " << height;
	synced_since = height;
	update_control();
}

bool Node::tx_pool_update(const tx_pool_t& entry, const bool force_add)
{
	if(entry.is_skipped) {
		throw std::logic_error("tx_pool_update(): entry is_skipped");
	}
	if(const auto& tx = entry.tx) {
		if(tx->sender) {
			const auto& sender = *tx->sender;
			const auto iter = tx_pool.find(tx->id);
			const auto fees = tx_pool_fees.find(sender);
			auto new_total = (fees != tx_pool_fees.end()) ? fees->second : 0;
			if(iter != tx_pool.end()) {
				new_total -= iter->second.fee;
			}
			new_total += entry.fee;

			if(!force_add && new_total > get_balance(sender, addr_t())) {
				return false;
			}
			if(fees != tx_pool_fees.end()) {
				fees->second = new_total;
			} else {
				tx_pool_fees[sender] = new_total;
			}
			if(iter != tx_pool.end()) {
				iter->second = entry;
			} else {
				std::lock_guard<std::mutex> lock(mutex);

				for(const auto& in : tx->get_inputs()) {
					tx_pool_index.emplace(std::make_pair(in.address, tx->id), tx);
				}
				for(const auto& out : tx->get_outputs()) {
					tx_pool_index.emplace(std::make_pair(out.address, tx->id), tx);
					if(out.memo) {
						tx_pool_index.emplace(std::make_pair(hash_t(out.address + (*out.memo)), tx->id), tx);
					}
				}
				tx_pool[tx->id] = entry;
			}
			return true;
		}
	}
	return false;
}

void Node::tx_pool_erase(const hash_t& txid)
{
	const auto iter = tx_pool.find(txid);
	if(iter != tx_pool.end()) {
		if(const auto& tx = iter->second.tx) {
			if(const auto& sender = tx->sender) {
				const auto iter2 = tx_pool_fees.find(*sender);
				if(iter2 != tx_pool_fees.end()) {
					if((iter2->second -= iter->second.fee) == 0) {
						tx_pool_fees.erase(iter2);
					}
				}
			}
			std::lock_guard<std::mutex> lock(mutex);

			for(const auto& in : tx->get_inputs()) {
				tx_pool_index.erase(std::make_pair(in.address, tx->id));
			}
			for(const auto& out : tx->get_outputs()) {
				tx_pool_index.erase(std::make_pair(out.address, tx->id));
				if(out.memo) {
					tx_pool_index.erase(std::make_pair(hash_t(out.address + (*out.memo)), tx->id));
				}
			}
		}
		tx_pool.erase(iter);
	}
}

void Node::purge_tx_pool()
{
	const auto time_begin = vnx::get_wall_time_millis();

	std::vector<tx_pool_t> all_tx;
	all_tx.reserve(tx_pool.size());
	for(const auto& entry : tx_pool) {
		all_tx.push_back(entry.second);
	}

	// sort transactions by fee ratio
	std::sort(all_tx.begin(), all_tx.end(),
		[](const tx_pool_t& lhs, const tx_pool_t& rhs) -> bool {
			const auto L = lhs.tx->fee_ratio;
			const auto R = rhs.tx->fee_ratio;
			return (L == R) ? lhs.luck < rhs.luck : L > R;
		});

	size_t num_purged = 0;
	uint64_t total_pool_size = 0;
	std::unordered_map<addr_t, std::pair<uint64_t, uint64_t>> sender_map;	// [sender => [balance, total fee]]

	const uint64_t max_pool_size = uint64_t(max_tx_pool) * params->max_block_size;

	// purge transactions from pool if overflowing
	for(const auto& entry : all_tx) {
		const auto& tx = entry.tx;
		bool fee_overspend = false;
		if(tx->sender) {
			const auto sender = *tx->sender;
			const auto iter = sender_map.emplace(sender, std::make_pair(0, 0));
			auto& balance = iter.first->second.first;
			auto& total_fee = iter.first->second.second;
			if(iter.second) {
				balance = get_balance(sender, addr_t());
			}
			total_fee += entry.fee;
			fee_overspend = total_fee > balance;
		}
		if(!fee_overspend) {
			total_pool_size += tx->static_cost;
		}
		if(total_pool_size > max_pool_size || fee_overspend) {
			tx_pool_erase(tx->id);
			num_purged++;
		} else {
			min_pool_fee_ratio = tx->fee_ratio;
		}
	}
	if(total_pool_size < 9 * max_pool_size / 10) {
		min_pool_fee_ratio = 0;
	}
	if(total_pool_size || num_purged) {
		log(INFO) << uint64_t((total_pool_size * 10000) / max_pool_size) / 100. << " % mem pool, "
				<< min_pool_fee_ratio / 1024. << " min fee ratio, " << num_purged << " purged, took "
				<< (vnx::get_wall_time_millis() - time_begin) / 1e3 << " sec";
	}
}

void Node::validate_new()
{
	const auto peak = get_peak();
	if(!peak || !is_synced) {
		return;
	}
	std::default_random_engine luck_gen(vnx::rand64());

	// select non-overlapping set
	std::vector<tx_pool_t> tx_list;
	std::unordered_set<hash_t> tx_set;
	tx_list.reserve(max_tx_queue);
	tx_set.reserve(max_tx_queue);
	for(const auto& entry : tx_queue) {
		if(const auto& tx = entry.second) {
			if(tx_set.insert(tx->id).second) {
				tx_pool_t tmp;
				tmp.tx = tx;
				tmp.luck = luck_gen();
				tx_list.push_back(tmp);
			}
		}
	}
	auto context = new_exec_context(peak->height + 1);

	// prepare synchronization
	for(const auto& entry : tx_list) {
		prepare_context(context, entry.tx);
	}
	const auto deadline_ms = vnx::get_wall_time_millis() + validate_interval_ms;

	// verify transactions in parallel
	for(auto& entry : tx_list) {
		threads->add_task([this, &entry, context, deadline_ms]() {
			if(vnx::get_wall_time_millis() > deadline_ms) {
				entry.is_skipped = true;
				return;
			}
			entry.is_valid = false;

			auto& tx = entry.tx;
			if(tx->exec_result) {
				auto tmp = vnx::clone(tx);
				tmp->reset(params);
				tx = tmp;
			}
			context->wait(tx->id);
			try {
				if(auto result = validate(tx, context)) {
					entry.cost = result->total_cost;
					entry.fee = result->total_fee;
					entry.is_valid = true;
				}
			} catch(const std::exception& ex) {
				if(show_warnings) {
					log(WARN) << "TX validation failed with: " << ex.what() << " (" << tx->id << ")";
				}
			} catch(...) {
				// ignore
			}
			context->signal(tx->id);
		});
	}
	threads->sync();

	// update tx pool
	for(const auto& entry : tx_list) {
		if(!entry.is_skipped) {
			const auto& tx = entry.tx;
			if(entry.is_valid) {
				if(tx_pool_update(entry)) {
					publish(tx, output_verified_transactions);
				}
			}
			tx_queue.erase(tx->content_hash);
		}
	}
}

std::vector<Node::tx_pool_t> Node::validate_for_block(const int64_t deadline_ms)
{
	const auto peak = get_peak();
	if(!peak) {
		return {};
	}
	auto context = new_exec_context(peak->height + 1);

	std::vector<tx_pool_t> all_tx;
	all_tx.reserve(tx_pool.size());
	for(const auto& entry : tx_pool) {
		const auto& pool = entry.second;
		if(check_tx_inclusion(pool.tx->id, context->height)) {
			all_tx.push_back(pool);
		}
	}

	// sort transactions by fee ratio
	std::sort(all_tx.begin(), all_tx.end(),
		[](const tx_pool_t& lhs, const tx_pool_t& rhs) -> bool {
			return lhs.tx->fee_ratio > rhs.tx->fee_ratio;
		});

	std::vector<tx_pool_t> tx_list;
	uint64_t total_verify_cost = 0;

	// select transactions to verify
	for(const auto& entry : all_tx) {
		if(total_verify_cost + entry.cost <= params->max_block_cost) {
			tx_list.push_back(entry);
			total_verify_cost += entry.cost;
		}
	}

	// prepare synchronization
	for(const auto& entry : tx_list) {
		prepare_context(context, entry.tx);
	}

	// verify transactions in parallel
	for(auto& entry : tx_list) {
		threads->add_task([this, &entry, context, deadline_ms]() {
			if(vnx::get_wall_time_millis() > deadline_ms) {
				entry.is_skipped = true;
				return;
			}
			entry.is_valid = false;

			auto& tx = entry.tx;
			if(tx->exec_result) {
				auto tmp = vnx::clone(tx);
				tmp->reset(params);
				tx = tmp;
			}
			context->wait(tx->id);
			try {
				auto result = validate(tx, context);
				if(!result) {
					throw std::logic_error("!result");
				}
				auto tmp = vnx::clone(tx);
				tmp->update(*result, params);
				tx = tmp;
				entry.cost = result->total_cost;
				entry.fee = result->total_fee;
				entry.is_valid = true;
			} catch(const std::exception& ex) {
				if(show_warnings) {
					log(WARN) << "TX validation failed with: " << ex.what() << " (" << tx->id << ")";
				}
			} catch(...) {
				// ignore
			}
			context->signal(tx->id);
		});
	}
	threads->sync();

	uint32_t num_skipped = 0;
	uint64_t total_cost = 0;
	uint64_t static_cost = 0;
	std::vector<tx_pool_t> result;
	balance_cache_t balance_cache(&balance_table);

	// select final set of transactions
	for(auto& entry : tx_list)
	{
		if(entry.is_skipped) {
			num_skipped++;
			continue;
		}
		if(!entry.is_valid) {
			tx_pool_erase(entry.tx->id);
			continue;
		}
		const auto tx = entry.tx;

		if(static_cost + tx->static_cost > params->max_block_size || total_cost + entry.cost > params->max_block_cost) {
			continue;
		}
		bool passed = true;
		balance_cache_t tmp_cache(&balance_cache);
		{
			const auto balance = tmp_cache.find(*tx->sender, addr_t());
			if(balance && entry.fee <= *balance) {
				*balance -= entry.fee;
			} else {
				passed = false;
			}
		}
		if(!tx->exec_result->did_fail) {
			for(const auto& in : tx->inputs) {
				const auto balance = tmp_cache.find(in.address, in.contract);
				if(balance && in.amount <= *balance) {
					*balance -= in.amount;
				} else {
					passed = false;
				}
			}
		}
		if(!passed) {
			continue;
		}
		balance_cache.apply(tmp_cache);

		total_cost += entry.cost;
		static_cost += tx->static_cost;
		result.push_back(entry);
	}
	if(num_skipped) {
		log(WARN) << "Skipped " << num_skipped << " transactions due to block creation deadline";
	}

	const uint32_t N = params->min_fee_ratio.size();
	if(N == 0) {
		return result;
	}
	std::vector<uint64_t> band_avail(N);
	for(uint64_t i = 0; i < N; ++i) {
		band_avail[i] = ((i + 1) * params->max_block_size) / N - (i * params->max_block_size) / N;
	}
	uint32_t i = N - 1;
	std::vector<tx_pool_t> out;
	for(const auto& entry : result) {
		const auto& tx = entry.tx;
		while(i && (!band_avail[i] || tx->fee_ratio < params->min_fee_ratio[i] * 1024)) {
			i--;
		}
		if(tx->static_cost > band_avail[i]) {
			if(i) {
				// we assume band size is always >= max_tx_cost (so band_avail never goes negative here)
				band_avail[i - 1] -= tx->static_cost - band_avail[i];
				band_avail[i] = 0;
			} else {
				continue;	// no room left for this tx
			}
		} else {
			band_avail[i] -= tx->static_cost;
		}
		out.push_back(entry);
	}
	return out;
}

std::shared_ptr<const Block> Node::make_block(
		std::shared_ptr<const BlockHeader> prev, std::vector<std::shared_ptr<const VDF_Point>> vdf_points, const proof_data_t& proof)
{
	if(vdf_points.empty()) {
		return nullptr;
	}
	const auto vdf_point = vdf_points.back();
	const auto time_begin = vnx::get_wall_time_millis();

	// reset state to previous block
	fork_to(prev->hash);

	auto block = Block::create();
	block->prev = prev->hash;
	block->height = prev->height + 1;
	block->time_diff = prev->time_diff;
	block->proof = proof.proof;
	block->proof_hash = proof.hash;
	block->vdf_count = vdf_points.size();
	block->vdf_height = prev->vdf_height + block->vdf_count;
	block->vdf_output = vdf_point->output;
	block->vdf_reward_addr = vdf_point->reward_addr;
	block->reward_vote = reward_vote;
	block->txfee_buffer = calc_new_txfee_buffer(params, prev);

	block->vdf_iters = prev->vdf_iters;
	for(auto point : vdf_points) {
		block->vdf_iters += point->num_iters;
	}
	block->challenge = calc_next_challenge(params, prev->challenge, block->vdf_count, block->proof_hash, block->is_space_fork);

	if(block->height % params->vdf_reward_interval == 0) {
		block->vdf_reward_payout = get_vdf_reward_winner(block);
	}

	if(auto nft = std::dynamic_pointer_cast<const ProofOfSpaceNFT>(block->proof)) {
		block->reward_contract = nft->contract;
	}

	if(auto contract = block->reward_contract) {
		const addr_t any = hash_t::random();
		const auto address = get_plot_nft_target(*contract, any);
		if(address != any) {
			block->reward_addr = address;
		}
	}

	if(auto ref = find_prev_header(prev, 100))
	{
		// set new time diff
		const auto delta_ms = prev->time_stamp - ref->time_stamp;
		const auto factor = double(params->block_interval_ms * 100) / delta_ms;
		block->time_diff = std::max<int64_t>(prev->time_diff * factor + 0.5, params->time_diff_divider);

		// limit time diff update
		const auto max_update = std::max<uint64_t>(prev->time_diff >> params->max_diff_adjust, 1);
		if(prev->time_diff > max_update) {
			block->time_diff = std::max(block->time_diff, prev->time_diff - max_update);
		}
		block->time_diff = std::min(block->time_diff, prev->time_diff + max_update);
	}
	{
		// set time stamp
		auto delta_ms = time_begin - prev->time_stamp;
		delta_ms = std::min(delta_ms, block->vdf_count * params->block_interval_ms * 2);
		delta_ms = std::max(delta_ms, block->vdf_count * params->block_interval_ms / 10);
		block->time_stamp = prev->time_stamp + delta_ms;
	}

	uint64_t total_fees = 0;
	if(block->height >= params->transaction_activation)
	{
		const auto deadline = vdf_point->recv_time + params->block_interval_ms;
		const auto tx_list = validate_for_block(deadline);
		// select transactions
		for(const auto& entry : tx_list) {
			block->tx_list.push_back(entry.tx);
			total_fees += entry.fee;
		}
	}

	block->weight = calc_block_weight(params, block, prev);
	block->total_weight = prev->total_weight + block->weight;
	block->reward_amount = calc_block_reward(block, total_fees);
	block->set_space_diff(params, prev);
	block->set_base_reward(params, prev);
	block->finalize();

	FarmerClient farmer(proof.farmer_mac);

	const auto result = farmer.sign_block(block);
	if(!result) {
		log(WARN) << "Farmer refused to sign block at height " << block->height;
		return nullptr;
	}
	block->BlockHeader::operator=(*result);

	created_blocks[proof.hash] = block->hash;

	const auto elapsed = (vnx::get_wall_time_millis() - time_begin) / 1e3;
	log(INFO) << u8"\U0001F911 Created block at height " << block->height << " with: ntx = " << block->tx_count
			<< ", score = " << block->proof->score << ", reward = " << to_value(block->reward_amount, params) << " MMX"
			<< ", fees = " << to_value(total_fees, params) << " MMX" << ", took " << elapsed << " sec";
	return block;
}


} // mmx
