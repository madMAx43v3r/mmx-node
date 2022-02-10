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
		if(verified_vdfs.count(proof->height - 1)) {
			if(!vdf_verify_pending.count(proof->height)) {
				add_task([this, proof]() {
					handle(proof);
				});
				iter = pending_vdfs.erase(iter);
				continue;
			}
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
		block->prev = prev->hash;
		block->height = prev->height + 1;
		block->time_diff = prev->time_diff;
		block->space_diff = prev->space_diff;
		block->vdf_iters = vdf_point->vdf_iters;
		block->vdf_output = vdf_point->output;
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
		for(const auto& entry : fork_index)
		{
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

#pragma omp parallel for
	for(size_t i = 0; i < to_verify.size(); ++i)
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

		// show finalized blocks
		for(const auto& fork : fork_line) {
			const auto& block = fork->block;
			if(block->height + params->finality_delay + 1 < peak->height) {
				if(!fork->is_finalized) {
					fork->is_finalized = true;
					if(!do_sync || (sync_peak && block->height >= *sync_peak)) {
						log(INFO) << "Finalized height " << block->height << " with: ntx = " << block->tx_list.size()
								<< ", k = " << (block->proof ? block->proof->ksize : 0) << ", score = " << fork->proof_score
								<< ", tdiff = " << block->time_diff << ", sdiff = " << block->space_diff
								<< (fork->has_weak_proof ? ", weak proof" : "");
					}
				}
			}
		}

		// commit to history
		for(size_t i = 0; i + params->commit_delay < fork_line.size(); ++i)
		{
			const auto& fork = fork_line[i];
			const auto& block = fork->block;
			if(!fork->is_vdf_verified) {
				break;	// wait for VDF verify
			}
			if(!is_synced && fork_line.size() < max_fork_length) {
				// check if there is a competing fork at this height
				const auto finalized_height = peak->height - std::min(params->finality_delay + 1, peak->height);
				if(std::distance(fork_index.lower_bound(block->height), fork_index.upper_bound(block->height)) > 1
					&& std::distance(fork_index.lower_bound(finalized_height), fork_index.upper_bound(finalized_height)) > 1)
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
		if(auto fork = find_fork(peak->hash)) {
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
			log(INFO) << "Reached sync peak at height " << *sync_peak - 1;
			sync_pos = *sync_peak;
			sync_peak = nullptr;
			sync_retry++;
		}
		else if(peak->height + params->commit_delay + 1 < *sync_peak)
		{
			log(ERROR) << "Sync failed, it appears we have forked from the network a while ago.";
			const auto replay_height = peak->height - std::min<uint32_t>(1000, peak->height);
			vnx::write_config("Node.replay_height", replay_height);
			log(INFO) << "Restarting with --Node.replay_height " << replay_height;
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
	if(light_mode) {
		return;
	}
	{
		// publish time infusions for VDF 0
		auto infuse = TimeInfusion::create();
		infuse->chain = 0;
		auto vdf_iters = peak->vdf_iters;
		for(uint32_t i = 0; i <= params->finality_delay; ++i)
		{
			if(auto diff_block = find_diff_header(peak, i + 1))
			{
				if(auto prev = find_prev_header(peak, params->finality_delay - i, true))
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
					const auto vdf_iters = prev->vdf_iters + diff_block->time_diff * params->time_diff_constant * params->finality_delay;
					infuse->values[vdf_iters] = prev->hash;
					publish(infuse, output_timelord_infuse);
				}
			}
		}
	}
	{
		// request next VDF proofs
		auto vdf_iters = peak->vdf_iters;
		for(uint32_t i = 0; i < params->finality_delay; ++i)
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
			if(!find_vdf_challenge(prev, vdf_challenge, 1)) {
				break;
			}
			const auto challenge = get_challenge(prev, vdf_challenge, 1);

			auto iter = proof_map.find(challenge);
			if(iter != proof_map.end()) {
				const auto proof = iter->second;
				// check if it's our proof
				if(vnx::get_pipe(proof->farmer_addr))
				{
					const auto next_height = prev->height + 1;
					const auto best_fork = find_best_fork(prev, &next_height);
					// check if we have a better proof
					if(!best_fork || proof->score < best_fork->proof_score) {
						try {
							if(make_block(prev, proof)) {
								made_block = true;
							}
						}
						catch(const std::exception& ex) {
							log(WARN) << "Failed to create a block: " << ex.what();
						}
						// revert back to peak
						if(auto fork = find_fork(peak->hash)) {
							fork_to(fork);
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

bool Node::make_block(std::shared_ptr<const BlockHeader> prev, std::shared_ptr<const ProofResponse> response)
{
	const auto time_begin = vnx::get_wall_time_micros();

	const auto vdf_point = find_next_vdf_point(prev);
	if(!vdf_point) {
		return false;
	}
	const auto prev_fork = find_fork(prev->hash);

	// reset state to previous block
	fork_to(prev->hash);

	auto block = Block::create();
	block->prev = prev->hash;
	block->height = prev->height + 1;
	block->time_diff = prev->time_diff;
	block->space_diff = prev->space_diff;
	block->vdf_iters = vdf_point->vdf_iters;
	block->vdf_output = vdf_point->output;

	// set new time difficulty
	if(auto fork = find_prev_fork(prev_fork, params->finality_delay)) {
		if(auto point = fork->vdf_point) {
			const int64_t time_delta = (vdf_point->recv_time - point->recv_time) / (params->finality_delay + 1);
			if(time_delta > 0) {
				const double gain = 0.1;
				if(auto diff_block = fork->diff_block) {
					double new_diff = params->block_time * diff_block->time_diff / (time_delta * 1e-6);
					new_diff = prev->time_diff * (1 - gain) + new_diff * gain;
					block->time_diff = std::max<int64_t>(new_diff + 0.5, 1);
				}
			}
		}
	}
	{
		// set new space difficulty
		double avg_score = response->score;
		{
			uint32_t counter = 1;
			auto fork = prev_fork;
			for(uint32_t i = 1; i < params->finality_delay && fork; ++i) {
				avg_score += fork->proof_score;
				fork = fork->prev.lock();
				counter++;
			}
			avg_score /= counter;
		}
		double delta = prev->space_diff * (params->score_target - avg_score);
		delta /= params->score_target;
		delta /= (1 << params->max_diff_adjust);

		int64_t update = 0;
		if(delta > 0 && delta < 1) {
			update = 1;
		} else if(delta < 0 && delta > -1) {
			update = -1;
		} else {
			update = delta + 0.5;
		}
		const int64_t new_diff = prev->space_diff + update;
		block->space_diff = std::max<int64_t>(new_diff, 1);
	}
	{
		const auto max_update = std::max<uint64_t>(prev->time_diff >> params->max_diff_adjust, 1);
		block->time_diff = std::min(block->time_diff, prev->time_diff + max_update);
		block->time_diff = std::max(block->time_diff, prev->time_diff - max_update);
	}
	{
		const auto max_update = std::max<uint64_t>(prev->space_diff >> params->max_diff_adjust, 1);
		block->space_diff = std::min(block->space_diff, prev->space_diff + max_update);
		block->space_diff = std::max(block->space_diff, prev->space_diff - max_update);
	}
	block->proof = response->proof;

	struct tx_entry_t {
		uint64_t fees = 0;
		uint64_t cost = 0;
		double fee_ratio = 0;
		std::shared_ptr<const Transaction> tx;
	};

	std::vector<tx_entry_t> tx_list;
	std::unordered_set<hash_t> invalid;
	std::unordered_set<hash_t> postpone;

	for(const auto& entry : tx_pool) {
		if(!tx_map.count(entry.first)) {
			tx_entry_t tmp;
			tmp.tx = entry.second;
			tx_list.push_back(tmp);
		}
	}
	auto context = Context::create();
	context->height = block->height;

#pragma omp parallel for
	for(size_t i = 0; i < tx_list.size(); ++i)
	{
		auto& entry = tx_list[i];
		auto& tx = entry.tx;
		// check if tx depends on another one which is not in a block yet
		bool depends = false;
		for(const auto& in : tx->inputs) {
			if(tx_pool.count(in.prev.txid) && !tx_map.count(in.prev.txid)) {
				depends = true;
			}
		}
		if(depends) {
#pragma omp critical
			postpone.insert(tx->id);
			continue;
		}
		try {
			const auto cost = tx->calc_cost(params);
			if(cost > params->max_block_cost) {
				throw std::logic_error("tx cost > max_block_cost");
			}
			if(!tx->exec_outputs.empty()) {
				auto copy = vnx::clone(tx);
				copy->exec_outputs.clear();
				tx = copy;
			}
			if(auto new_tx = validate(tx, context, nullptr, entry.fees)) {
				tx = new_tx;
			}
			entry.cost = cost;
			entry.fee_ratio = entry.fees / double(cost);
		}
		catch(const std::exception& ex) {
#pragma omp critical
			invalid.insert(tx->id);
			log(WARN) << "TX validation failed with: " << ex.what();
		}
	}

	// sort by fee ratio
	std::sort(tx_list.begin(), tx_list.end(),
		[](const tx_entry_t& lhs, const tx_entry_t& rhs) -> bool {
			return lhs.fee_ratio > rhs.fee_ratio;
		});

	uint64_t total_fees = 0;
	uint64_t total_cost = 0;
	std::unordered_set<addr_t> mutated;
	std::unordered_set<txio_key_t> spent;

	for(size_t i = 0; i < tx_list.size(); ++i)
	{
		const auto& entry = tx_list[i];
		const auto& tx = entry.tx;
		if(!invalid.count(tx->id) && !postpone.count(tx->id))
		{
			if(total_cost + entry.cost < params->max_block_cost)
			{
				bool passed = true;
				for(const auto& in : tx->inputs) {
					if(!spent.insert(in.prev).second) {
						passed = false;
					}
				}
				for(const auto& op : tx->execute) {
					if(std::dynamic_pointer_cast<const operation::Mutate>(op)) {
						if(!mutated.insert(op->address).second) {
							passed = false;
						}
					}
				}
				if(passed) {
					block->tx_list.push_back(tx);
					total_fees += entry.fees;
					total_cost += entry.cost;
				}
			}
		}
	}
	for(const auto& id : invalid) {
		tx_pool.erase(id);
	}
	block->finalize();

	FarmerClient farmer(response->farmer_addr);
	const auto block_reward = calc_block_reward(block);
	const auto final_reward = calc_final_block_reward(params, block_reward, total_fees);
	const auto result = farmer.sign_block(block, final_reward);

	if(!result) {
		throw std::logic_error("farmer refused");
	}
	block->tx_base = result->tx_base;
	block->farmer_sig = result->farmer_sig;
	block->finalize();

	add_block(block);

	const auto elapsed = (vnx::get_wall_time_micros() - time_begin) / 1e6;
	log(INFO) << "Created block at height " << block->height << " with: ntx = " << block->tx_list.size()
			<< ", score = " << response->score << ", reward = " << final_reward / pow(10, params->decimals) << " MMX"
			<< ", nominal = " << block_reward / pow(10, params->decimals) << " MMX"
			<< ", fees = " << total_fees / pow(10, params->decimals) << " MMX"
			<< ", took " << elapsed << " sec";
	return true;
}


} // mmx
