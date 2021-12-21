/*
 * Node.cpp
 *
 *  Created on: Dec 7, 2021
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/Challenge.hxx>
#include <mmx/FarmerClient.hxx>
#include <mmx/TimeInfusion.hxx>
#include <mmx/IntervalRequest.hxx>
#include <mmx/contract/PubKey.hxx>
#include <mmx/chiapos.h>
#include <mmx/utils.h>

#include <atomic>
#include <algorithm>


namespace mmx {

Node::Node(const std::string& _vnx_name)
	:	NodeBase(_vnx_name)
{
}

void Node::init()
{
	vnx::open_pipe(vnx_name, this, max_queue_ms);
}

void Node::main()
{
	params = get_params();
	{
		vdf_point_t point;
		point.output[0] = hash_t(params->vdf_seed);
		point.output[1] = hash_t(params->vdf_seed);
		point.recv_time = vnx::get_time_micros();
		verified_vdfs[0] = point;
	}
	router = std::make_shared<RouterAsyncClient>(router_name);
	add_async_client(router);

	block_chain = std::make_shared<vnx::File>(storage_path + "block_chain.dat");

	if(block_chain->exists())
	{
		block_chain->open("rb+");
		int64_t last_pos = 0;
		while(true) {
			auto& in = block_chain->in;
			try {
				last_pos = in.get_input_pos();
				if(auto value = vnx::read(in)) {
					if(auto block = std::dynamic_pointer_cast<Block>(value)) {
						apply(block);
						commit(block);
						block_index[block->height] = std::make_pair(last_pos, block->hash);
					}
				} else {
					break;
				}
			}
			catch(const std::exception& ex) {
				log(WARN) << "Failed to read block: " << ex.what();
				break;
			}
		}
		block_chain->seek_to(last_pos);
	} else {
		block_chain->open("ab");
	}

	vnx::File fork_line(storage_path + "fork_line.dat");
	if(fork_line.exists())
	{
		fork_line.open("rb");
		while(true) {
			auto& in = fork_line.in;
			try {
				if(auto value = vnx::read(in)) {
					if(auto block = std::dynamic_pointer_cast<Block>(value)) {
						add_block(block);
					}
				} else {
					break;
				}
			}
			catch(const std::exception& ex) {
				log(WARN) << "Failed to read block: " << ex.what();
				break;
			}
		}
	}

	if(auto block = find_header(state_hash)) {
		log(INFO) << "Loaded " << block->height + 1 << " blocks from disk";
	}
	is_replay = false;
	is_synced = !do_sync;

	if(state_hash == hash_t())
	{
		auto genesis = Block::create();
		genesis->time_diff = params->initial_time_diff;
		genesis->space_diff = params->initial_space_diff;
		genesis->vdf_output[0] = hash_t(params->vdf_seed);
		genesis->vdf_output[1] = hash_t(params->vdf_seed);
		genesis->finalize();

		apply(genesis);
		commit(genesis);
	}

	if(auto block = find_header(state_hash))
	{
		vdf_point_t point;
		point.output = block->vdf_output;
		point.recv_time = vnx::get_time_micros();
		verified_vdfs[block->vdf_iters] = point;
	}

	subscribe(input_vdfs, max_queue_ms);
	subscribe(input_blocks, max_queue_ms);
	subscribe(input_transactions, max_queue_ms);
	subscribe(input_timelord_vdfs, max_queue_ms);
	subscribe(input_harvester_proof, max_queue_ms);

	update_timer = set_timer_millis(update_interval_ms, std::bind(&Node::update, this));

	update();

	Super::main();

	fork_line.open("wb");
	for(const auto& fork : get_fork_line()) {
		vnx::write(fork_line.out, fork->block);
	}
	vnx::write(fork_line.out, nullptr);
	fork_line.close();

	vnx::write(block_chain->out, nullptr);

	block_chain->close();
}

uint32_t Node::get_height() const
{
	if(auto block = find_header(state_hash)) {
		return block->height;
	}
	throw std::logic_error("have no peak");
}

std::shared_ptr<const Block> Node::get_block(const hash_t& hash) const
{
	if(auto block = find_block(hash)) {
		return block;
	}
	auto iter = hash_index.find(hash);
	if(iter != hash_index.end()) {
		auto iter2 = block_index.find(iter->second);
		if(iter2 != block_index.end()) {
			const auto prev_pos = block_chain->get_output_pos();
			block_chain->seek_to(iter2->second.first);
			std::shared_ptr<const Block> block;
			try {
				block = std::dynamic_pointer_cast<const Block>(vnx::read(block_chain->in));
			} catch(...) {
				// ignore
			}
			block_chain->seek_to(prev_pos);
			return block;
		}
	}
	return nullptr;
}

std::shared_ptr<const Block> Node::get_block_at(const uint32_t& height) const
{
	auto iter = block_index.find(height);
	if(iter != block_index.end()) {
		return get_block(iter->second.second);
	}
	const auto line = get_fork_line();
	if(!line.empty()) {
		const auto offset = line[0]->block->height;
		if(height >= offset) {
			const auto index = height - offset;
			if(index < line.size()) {
				return line[index]->block;
			}
		}
	}
	return nullptr;
}

vnx::optional<hash_t> Node::get_block_hash(const uint32_t& height) const
{
	auto iter = block_index.find(height);
	if(iter != block_index.end()) {
		return iter->second.second;
	}
	if(auto block = get_block_at(height)) {
		return block->hash;
	}
	return nullptr;
}

vnx::optional<tx_key_t> Node::get_tx_key(const hash_t& id) const
{
	{
		auto iter = tx_map.find(id);
		if(iter != tx_map.end()) {
			return iter->second;
		}
	}
	{
		auto iter = tx_index.find(id);
		if(iter != tx_index.end()) {
			return iter->second;
		}
	}
	return nullptr;
}

txo_info_t Node::get_txo_info(const txio_key_t& key) const
{
	{
		auto iter = utxo_map.find(key);
		if(iter != utxo_map.end()) {
			txo_info_t info;
			info.created_at = iter->second.height;
			return info;
		}
	}
	{
		auto iter = stxo_index.find(key);
		if(iter != stxo_index.end()) {
			auto iter2 = tx_index.find(iter->second.txid);
			if(iter2 != tx_index.end()) {
				txo_info_t info;
				info.created_at = iter2->second.height;
				info.spent_on = iter->second;
				return info;
			}
		}
	}
	for(const auto& log : change_log) {
		auto iter = log->utxo_removed.find(key);
		if(iter != log->utxo_removed.end()) {
			txo_info_t info;
			info.created_at = iter->second.second.height;
			info.spent_on = iter->second.first;
			return info;
		}
	}
	throw std::logic_error("no such txo entry");
}

std::shared_ptr<const Transaction> Node::get_transaction(const hash_t& id) const
{
	{
		auto iter = tx_pool.find(id);
		if(iter != tx_pool.end()) {
			return iter->second;
		}
	}
	{
		auto iter = tx_index.find(id);
		if(iter != tx_index.end()) {
			const auto& entry = iter->second;
			if(auto block = get_block_at(entry.height)) {
				if(entry.index == 0) {
					return block->tx_base;
				}
				return block->tx_list.at(entry.index - 1);
			}
		}
	}
	return nullptr;
}

void Node::add_block(std::shared_ptr<const Block> block)
{
	if(fork_tree.count(block->hash)) {
		return;
	}
	auto root = get_root();
	if(block->height <= root->height) {
		return;
	}
	if(!block->is_valid()) {
		return;
	}
	if(is_replay || !is_synced) {
		vdf_point_t point;
		point.output = block->vdf_output;
		point.recv_time = vnx::get_time_micros();
		verified_vdfs[block->vdf_iters] = point;
		log(DEBUG) << "Added VDF at " << block->vdf_iters << " from block " << block->height;
	}
	auto fork = std::make_shared<fork_t>();
	fork->recv_time = vnx::get_time_micros();
	fork->block = block;
	fork_tree[block->hash] = fork;
}

void Node::add_transaction(std::shared_ptr<const Transaction> tx)
{
	if(tx_pool.count(tx->id)) {
		return;
	}
	if(!tx->is_valid()) {
		return;
	}
	tx_pool[tx->id] = tx;

	if(!vnx_sample) {
		publish(tx, output_transactions);
	}
}

uint64_t Node::get_balance(const addr_t& address, const addr_t& contract) const
{
	return get_total_balance({address}, contract);
}

uint64_t Node::get_total_balance(const std::vector<addr_t>& addresses, const addr_t& contract) const
{
	uint64_t total = 0;
	for(const auto& entry : get_utxo_list(addresses)) {
		const auto& out = entry.second;
		if(out.contract == contract) {
			total += out.amount;
		}
	}
	return total;
}

std::vector<std::pair<txio_key_t, utxo_t>> Node::get_utxo_list(const std::vector<addr_t>& addresses) const
{
	std::vector<std::pair<txio_key_t, utxo_t>> res;
	for(const auto& addr : addresses) {
		const auto begin = addr_map.lower_bound(std::make_pair(addr, txio_key_t()));
		const auto end   = addr_map.upper_bound(std::make_pair(addr, txio_key_t::create_ex(hash_t::ones(), -1)));
		for(auto iter = begin; iter != end; ++iter) {
			auto iter2 = utxo_map.find(iter->second);
			if(iter2 != utxo_map.end()) {
				res.emplace_back(iter->second, iter2->second);
			}
		}
	}
	return res;
}

void Node::handle(std::shared_ptr<const Block> block)
{
	add_block(block);
}

void Node::handle(std::shared_ptr<const Transaction> tx)
{
	add_transaction(tx);
}

void Node::handle(std::shared_ptr<const ProofOfTime> proof)
{
	const auto vdf_iters = proof->start + proof->get_num_iters();
	if(verified_vdfs.count(vdf_iters)) {
		return;
	}
	if(!requested_vdfs.count(std::make_pair(vdf_iters, proof->start))) {
		return;
	}
	auto iter = verified_vdfs.find(proof->start);
	if(iter != verified_vdfs.end())
	{
		const auto time_begin = vnx::get_wall_time_micros();
		const auto& prev = iter->second;
		try {
			if(!vnx_sample || vnx_sample->topic != input_timelord_vdfs) {
				verify_vdf(proof, prev);
			}
		}
		catch(const std::exception& ex) {
			log(WARN) << "VDF verification failed with: " << ex.what();
			return;
		}
		vdf_point_t point;
		point.output[0] = proof->get_output(0);
		point.output[1] = proof->get_output(1);
		point.recv_time = vnx_sample ? vnx_sample->recv_time : vnx::get_time_micros();

		verified_vdfs[vdf_iters] = point;
		requested_vdfs.erase(std::make_pair(vdf_iters, proof->start));

		publish(proof, output_verified_vdfs);

		log(INFO) << "Verified VDF at " << vdf_iters << " iterations, delta = "
				<< (point.recv_time - prev.recv_time) / 1e6 << " sec, took "
				<< (vnx::get_wall_time_micros() - time_begin) / 1e6 << " sec";

		update();
	}
	else {
		log(WARN) << "Cannot verify VDF for " << vdf_iters << " due to missing start at " << proof->start;
	}
}

void Node::handle(std::shared_ptr<const ProofResponse> value)
{
	if(!value->proof || !value->request || value->score >= params->score_threshold) {
		return;
	}
	const auto challenge = value->request->challenge;

	auto iter = proof_map.find(challenge);
	if(iter == proof_map.end() || value->score < iter->second->score) {
		try {
			const auto score = verify_proof(value->proof, challenge, value->request->space_diff);
			if(score == value->score) {
				if(iter == proof_map.end()) {
					challenge_map.emplace(value->request->height, challenge);
				}
				proof_map[challenge] = value;
			} else {
				throw std::logic_error("score mismatch");
			}
		} catch(const std::exception& ex) {
			log(WARN) << "Got invalid proof: " << ex.what();
		}
	}
}

void Node::update()
{
	// verify proof where possible
	std::vector<std::pair<std::shared_ptr<fork_t>, hash_t>> to_verify;
	{
		const auto root = get_root();
		for(auto iter = fork_tree.begin(); iter != fork_tree.end();)
		{
			const auto& fork = iter->second;
			const auto& block = fork->block;

			bool has_prev = true;
			if(!fork->prev.lock()) {
				if(auto prev = find_fork(block->prev)) {
					fork->prev = prev;
				} else {
					has_prev = false;
				}
			}
			if(!fork->diff_block) {
				fork->diff_block = find_diff_header(block);
			}
			if(!fork->is_proof_verified && fork->diff_block && (has_prev || block->prev == root->hash))
			{
				auto iter2 = verified_vdfs.find(block->vdf_iters);
				if(iter2 != verified_vdfs.end()) {
					if(block->vdf_output != iter2->second.output) {
						log(WARN) << "VDF verification failed for a block at height " << block->height;
						iter = fork_tree.erase(iter);
						continue;
					}
					hash_t vdf_challenge;
					if(find_vdf_challenge(block, vdf_challenge)) {
						to_verify.emplace_back(fork, vdf_challenge);
					}
				}
			}
			iter++;
		}
	}

#pragma omp parallel for
	for(const auto& entry : to_verify)
	{
		const auto& fork = entry.first;
		const auto& block = fork->block;
		try {
			fork->proof_score = verify_proof(block, entry.second);
			fork->is_proof_verified = true;
		}
		catch(const std::exception& ex) {
#pragma omp critical
			fork_tree.erase(block->hash);
			log(WARN) << "Proof verification failed for a block at height " << block->height << " with: " << ex.what();
		}
	}
	const auto prev_peak = find_header(state_hash);

	std::shared_ptr<const BlockHeader> forked_at;

	// choose best fork
	while(true) {
		forked_at = nullptr;

		// purge disconnected forks
		purge_tree();

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

		// commit to history
		const auto fork_line = get_fork_line();
		for(size_t i = 0; i + params->commit_delay < fork_line.size(); ++i) {
			commit(fork_line[i]->block);
		}
		break;
	}

	const auto peak = find_header(state_hash);
	if(!peak) {
		log(WARN) << "Have no peak!";
		return;
	}
	const auto root = get_root();

	if(!prev_peak || peak->hash != prev_peak->hash)
	{
		const auto fork = find_fork(peak->hash);
		log(INFO) << "New peak at height " << peak->height << " with score " << (fork ? std::to_string(fork->proof_score) : "?")
				<< (forked_at ? " (forked at " + std::to_string(forked_at->height) + ")" : "");
	}

	if(!is_synced && sync_pos >= sync_peak && sync_pending.empty())
	{
		if(sync_retry < num_sync_retries) {
			sync_pos = sync_peak;
			sync_peak = -1;
			sync_retry++;
		} else {
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
				request->interval = (params->block_time * 1e6) / params->num_vdf_segments;
				requested_vdfs.emplace(vdf_iters, request->begin);
				publish(request, output_interval_request);
			}
		}
	}

	// try to make a block
	{
		auto prev = peak;
		bool made_block = false;
		for(uint32_t i = 0; prev && i <= params->finality_delay; ++i)
		{
			if(prev->height < root->height) {
				break;
			}
			if(auto base = find_prev_header(prev)) {
				// find best block at this height to build on
				const auto prev_height = prev->height;
				if(auto fork = find_best_fork(base, &prev_height)) {
					prev = fork->block;
				}
			}
			auto diff_block = find_diff_header(prev, 1);
			if(!diff_block) {
				break;
			}
			{
				// add dummy block in case no proof is found
				const auto vdf_iters = prev->vdf_iters + diff_block->time_diff * params->time_diff_constant;
				auto iter = verified_vdfs.find(vdf_iters);
				if(iter != verified_vdfs.end()) {
					auto block = Block::create();
					block->prev = prev->hash;
					block->height = prev->height + 1;
					block->time_diff = prev->time_diff;
					block->space_diff = prev->space_diff;
					block->vdf_iters = iter->first;
					block->vdf_output = iter->second.output;
					block->finalize();
					add_block(block);
				}
			}
			hash_t vdf_challenge;
			if(!find_vdf_challenge(prev, vdf_challenge, 1)) {
				break;
			}
			const auto challenge = get_challenge(prev, vdf_challenge, 1);
			{
				// publish challenge
				auto value = Challenge::create();
				value->height = prev->height + 1;
				value->challenge = challenge;
				value->space_diff = diff_block->space_diff;
				publish(value, output_challenges);
			}
			auto iter = proof_map.find(challenge);
			if(iter != proof_map.end()) {
				const auto& proof = iter->second;
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
				}
			}
			prev = find_prev_header(prev);
		}
		if(made_block) {
			// revert back to peak
			if(auto fork = find_fork(peak->hash)) {
				fork_to(fork);
			}
			// update again right away
			add_task(std::bind(&Node::update, this));
		}
	}

	// publish advance challenges
	for(uint32_t i = 2; i <= params->challenge_delay; ++i)
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
	if(auto fork = find_fork(prev->hash)) {
		fork_to(fork);
	} else {
		throw std::logic_error("no such fork");
	}
	auto block = Block::create();
	block->prev = prev->hash;
	block->height = prev->height + 1;
	block->time_diff = prev->time_diff;
	block->space_diff = prev->space_diff;

	const auto diff_block = find_diff_header(block);
	if(!diff_block) {
		return false;
	}
	block->vdf_iters = prev->vdf_iters + diff_block->time_diff * params->time_diff_constant;

	vdf_point_t vdf_point;
	{
		auto iter = verified_vdfs.find(block->vdf_iters);
		if(iter != verified_vdfs.end()) {
			vdf_point = iter->second;
		} else {
			return false;
		}
	}
	block->vdf_output = vdf_point.output;
	{
		// set new time difficulty
		auto iter = verified_vdfs.find(prev->vdf_iters);
		if(iter != verified_vdfs.end()) {
			const int64_t time_delta = vdf_point.recv_time - iter->second.recv_time;
			if(time_delta > 0) {
				const double gain = 0.1;
				double new_diff = params->block_time * ((block->vdf_iters - prev->vdf_iters) / params->time_diff_constant) / (time_delta * 1e-6);
				new_diff = prev->time_diff * (1 - gain) + new_diff * gain;
				block->time_diff = std::max<int64_t>(new_diff + 0.5, 1);
			}
		}
	}
	{
		// set new space difficulty
		double delta = prev->space_diff;
		if(response->score < params->target_score) {
			delta *= (params->target_score - response->score);
		} else {
			delta *= -1 * double(response->score - params->target_score);
		}
		delta /= params->target_score;
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

	std::unordered_set<hash_t> invalid;
	std::unordered_set<hash_t> postpone;
	std::unordered_set<txio_key_t> spent;
	std::vector<std::shared_ptr<const Transaction>> tx_list;

	for(const auto& entry : tx_pool)
	{
		if(tx_map.find(entry.first) != tx_map.end()) {
			// already included in a previous block
			continue;
		}
		const auto& tx = entry.second;
		try {
			for(const auto& in : tx->inputs) {
				if(!spent.insert(in.prev).second) {
					throw std::logic_error("double spend");
				}
			}
			tx_list.push_back(tx);
		}
		catch(const std::exception& ex) {
			invalid.insert(entry.first);
			log(WARN) << "TX validation failed with: " << ex.what();
		}
	}
	std::vector<uint64_t> tx_fees(tx_list.size());

#pragma omp parallel for
	for(size_t i = 0; i < tx_list.size(); ++i)
	{
		const auto& tx = tx_list[i];
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
			tx_fees[i] = validate(tx);
		}
		catch(const std::exception& ex) {
#pragma omp critical
			invalid.insert(tx->id);
			log(WARN) << "TX validation failed with: " << ex.what();
		}
	}

	uint64_t total_fees = 0;
	for(size_t i = 0; i < tx_list.size(); ++i)
	{
		const auto& tx = tx_list[i];
		if(!invalid.count(tx->id) && !postpone.count(tx->id))
		{
			if(total_fees + tx_fees[i] < params->max_block_cost) {
				block->tx_list.push_back(tx);
				total_fees += tx_fees[i];
			} else {
				break;
			}
		}
	}
	for(const auto& id : invalid) {
		tx_pool.erase(id);
	}
	block->finalize();

	FarmerClient farmer(response->farmer_addr);
	const auto block_reward = calc_block_reward(block);
	const auto final_reward = std::max(std::max(block_reward, params->min_reward), uint64_t(total_fees));
	const auto result = farmer.sign_block(block, final_reward);

	if(!result) {
		throw std::logic_error("farmer refused");
	}
	block->tx_base = result->tx_base;
	block->pool_sig = result->pool_sig;
	block->farmer_sig = result->farmer_sig;
	block->finalize();

	add_block(block);

	log(INFO) << "Created block at height " << block->height << " with: ntx = " << block->tx_list.size()
			<< ", score = " << response->score << ", reward = " << final_reward / pow(10, params->decimals) << " MMX"
			<< ", nominal = " << block_reward / pow(10, params->decimals) << " MMX"
			<< ", fees = " << total_fees / pow(10, params->decimals) << " MMX";
	return true;
}

void Node::start_sync()
{
	sync_pos = 0;
	sync_peak = -1;
	sync_retry = 0;
	is_synced = false;
	sync_more();
}

void Node::sync_more()
{
	if(is_synced) {
		return;
	}
	if(!sync_pos) {
		sync_pos = get_root()->height + 1;
		sync_update = sync_pos;
		log(INFO) << "Starting sync at height " << sync_pos;
	}
	while(sync_pending.size() < params->finality_delay)
	{
		if(sync_pos >= sync_peak) {
			break;
		}
		const auto height = sync_pos++;
		router->get_blocks_at(height, std::bind(&Node::sync_result, this, height, std::placeholders::_1));
		sync_pending.insert(height);
	}
}

void Node::sync_result(uint32_t height, const std::vector<std::shared_ptr<const Block>>& blocks)
{
	sync_pending.erase(height);

	for(auto block : blocks) {
		if(block) {
			add_block(block);
		}
	}
	if(blocks.empty()) {
		if(sync_peak == uint32_t(-1)) {
			sync_peak = height;
			log(INFO) << "Reached sync peak at height " << height - 1;
		}
	}
	sync_more();

	if(sync_pos - sync_update >= 32) {
		sync_update = sync_pos;
		add_task(std::bind(&Node::update, this));
	}
}

std::shared_ptr<const BlockHeader> Node::fork_to(std::shared_ptr<fork_t> fork_head)
{
	const auto prev_state = find_fork(state_hash);
	const auto fork_line = get_fork_line(fork_head);

	bool did_fork = false;
	std::shared_ptr<const BlockHeader> forked_at;

	// bring state back in line
	while(true) {
		bool found = false;
		for(const auto& fork : fork_line)
		{
			const auto& block = fork->block;
			if(block->hash == state_hash) {
				found = true;
				forked_at = block;
				break;
			}
		}
		if(found) {
			break;
		}
		did_fork = true;

		if(!revert()) {
			forked_at = get_root();
			break;
		}
	}

	// verify and apply
	for(const auto& fork : fork_line)
	{
		const auto& block = fork->block;
		if(block->prev != state_hash) {
			// already verified and applied
			continue;
		}
		if(!fork->is_verified) {
			try {
				validate(block);
				fork->is_verified = true;
			}
			catch(const std::exception& ex) {
				log(WARN) << "Block verification failed for height " << block->height << " with: " << ex.what();
				fork_tree.erase(block->hash);
				fork_to(prev_state);
				throw;
			}
			if(!is_replay && is_synced) {
				publish(block, output_verified_blocks);
			}
		}
		apply(block);
	}
	return did_fork ? forked_at : nullptr;
}

std::shared_ptr<Node::fork_t> Node::find_best_fork(std::shared_ptr<const BlockHeader> root, const uint32_t* at_height) const
{
	uint64_t max_weight = 0;
	std::shared_ptr<fork_t> best_fork;

	if(!root) {
		root = get_root();
	}
	for(const auto& entry : fork_tree)
	{
		const auto& fork = entry.second;
		const auto& block = fork->block;
		if(block->height <= root->height) {
			continue;
		}
		if(at_height && block->height != *at_height) {
			continue;
		}
		uint64_t weight = 0;
		if(calc_fork_weight(root, fork, weight))
		{
			if(!best_fork || weight > max_weight || (weight == max_weight && block->hash < best_fork->block->hash))
			{
				best_fork = fork;
				max_weight = weight;
			}
		}
	}
	return best_fork;
}

std::vector<std::shared_ptr<Node::fork_t>> Node::get_fork_line(std::shared_ptr<fork_t> fork_head) const
{
	std::vector<std::shared_ptr<fork_t>> line;
	auto fork = fork_head ? fork_head : find_fork(state_hash);
	while(fork) {
		line.push_back(fork);
		fork = fork->prev.lock();
	}
	std::reverse(line.begin(), line.end());
	return line;
}

bool Node::calc_fork_weight(std::shared_ptr<const BlockHeader> root, std::shared_ptr<fork_t> fork, uint64_t& total_weight) const
{
	while(fork) {
		const auto& block = fork->block;
		if(!fork->is_proof_verified || fork->proof_score > params->score_threshold) {
			return false;
		}
		total_weight += 2 * params->score_threshold - fork->proof_score;

		if(block->prev == root->hash) {
			return true;
		}
		fork = fork->prev.lock();
	}
	return false;
}

void Node::validate(std::shared_ptr<const Block> block) const
{
	const auto prev = find_prev_header(block);
	if(!prev) {
		throw std::logic_error("invalid prev");
	}
	if(prev->hash != state_hash) {
		throw std::logic_error("state mismatch");
	}
	if(block->height != prev->height + 1) {
		throw std::logic_error("invalid height");
	}
	if(block->time_diff == 0 || block->space_diff == 0) {
		throw std::logic_error("invalid difficulty");
	}
	if(auto proof = block->proof) {
		if(!block->pool_sig || !block->pool_sig->verify(proof->pool_key, block->hash)) {
			throw std::logic_error("invalid pool signature");
		}
		if(!block->farmer_sig || !block->farmer_sig->verify(proof->farmer_key, block->hash)) {
			throw std::logic_error("invalid farmer signature");
		}
		validate_diff_adjust(block->time_diff, prev->time_diff);
		validate_diff_adjust(block->space_diff, prev->space_diff);
	} else {
		if(block->tx_base || !block->tx_list.empty()) {
			throw std::logic_error("transactions not allowed");
		}
		if(block->time_diff != prev->time_diff || block->space_diff != prev->space_diff) {
			throw std::logic_error("invalid difficulty adjustment");
		}
	}
	uint64_t base_spent = 0;
	if(const auto& tx = block->tx_base) {
		base_spent = validate(tx, block);
	}
	{
		std::unordered_set<txio_key_t> inputs;
		for(const auto& tx : block->tx_list) {
			for(const auto& in : tx->inputs) {
				if(!inputs.insert(in.prev).second) {
					throw std::logic_error("double spend");
				}
			}
		}
	}
	std::exception_ptr failed_ex;
	std::atomic<uint64_t> total_fees {0};

#pragma omp parallel for
	for(const auto& tx : block->tx_list) {
		try {
			total_fees += validate(tx);
		} catch(...) {
#pragma omp critical
			failed_ex = std::current_exception();
		}
	}
	if(failed_ex) {
		std::rethrow_exception(failed_ex);
	}
	if(total_fees > params->max_block_cost) {
		throw std::logic_error("block cost too high");
	}

	const auto base_reward = calc_block_reward(block);
	const auto base_allowed = std::max(std::max(base_reward, params->min_reward), uint64_t(total_fees));
	if(base_spent > base_allowed) {
		throw std::logic_error("coin base over-spend");
	}
}

uint64_t Node::validate(std::shared_ptr<const Transaction> tx, std::shared_ptr<const Block> block) const
{
	if(tx->id != tx->calc_hash()) {
		throw std::logic_error("invalid tx id");
	}
	if(block) {
		if(!tx->execute.empty()) {
			throw std::logic_error("coin base cannot have operations");
		}
		if(tx->inputs.size() != 1) {
			throw std::logic_error("coin base must have one input");
		}
		const auto& in = tx->inputs[0];
		if(in.prev.txid != hash_t(block->prev) || in.prev.index != 0) {
			throw std::logic_error("invalid coin base input");
		}
	} else {
		if(tx->inputs.empty()) {
			throw std::logic_error("tx without input");
		}
	}
	if(tx->inputs.size() > params->max_tx_inputs) {
		throw std::logic_error("too many tx inputs");
	}
	if(tx->outputs.size() > params->max_tx_outputs) {
		throw std::logic_error("too many tx outputs");
	}

	uint64_t base_amount = 0;
	std::unordered_map<hash_t, uint64_t> amounts;

	if(!block) {
		for(const auto& in : tx->inputs)
		{
			auto iter = utxo_map.find(in.prev);
			if(iter == utxo_map.end()) {
				throw std::logic_error("utxo not found");
			}
			const auto& out = iter->second;

			// verify signature
			const auto solution = tx->get_solution(in.solution);
			if(!solution) {
				throw std::logic_error("missing solution");
			}
			{
				auto iter = contracts.find(out.address);
				if(iter != contracts.end()) {
					if(!iter->second->validate(solution, tx->id)) {
						throw std::logic_error("invalid solution");
					}
				}
				else {
					contract::PubKey simple;
					simple.address = out.address;
					if(!simple.validate(solution, tx->id)) {
						throw std::logic_error("invalid solution");
					}
				}
			}
			amounts[out.contract] += out.amount;
		}
		for(const auto& op : tx->execute)
		{
			// TODO
		}
	}
	for(const auto& out : tx->outputs)
	{
		if(out.amount == 0) {
			throw std::logic_error("zero tx output");
		}
		if(block) {
			if(out.contract != hash_t()) {
				throw std::logic_error("invalid coin base output");
			}
			base_amount += out.amount;
		}
		else {
			auto& value = amounts[out.contract];
			if(out.amount > value) {
				throw std::logic_error("tx over-spend");
			}
			value -= out.amount;
		}
	}
	if(block) {
		return base_amount;
	}
	const auto fee_amount = amounts[hash_t()];
	const auto fee_needed = tx->calc_min_fee(params);
	if(fee_amount < fee_needed) {
		throw std::logic_error("insufficient fee");
	}
	return fee_amount;
}

void Node::validate_diff_adjust(const uint64_t& block, const uint64_t& prev) const
{
	const auto max_update = std::max<uint64_t>(prev >> params->max_diff_adjust, 1);
	if(block > prev && block - prev > max_update) {
		throw std::logic_error("invalid difficulty adjustment upwards");
	}
	if(block < prev && prev - block > max_update) {
		throw std::logic_error("invalid difficulty adjustment downwards");
	}
}

void Node::commit(std::shared_ptr<const Block> block) noexcept
{
	const auto root = get_root();
	if(root && block->prev != root->hash) {
		return;
	}
	if(change_log.empty()) {
		return;
	}
	const auto log = change_log.front();

	for(const auto& entry : log->utxo_removed) {
		addr_map.erase(std::make_pair(entry.second.second.address, entry.first));
	}
	for(const auto& entry : log->utxo_added) {
		addr_map.emplace(entry.second.address, entry.first);
	}
	for(const auto& txid : log->tx_added) {
		auto iter = tx_map.find(txid);
		if(iter != tx_map.end()) {
			tx_index[txid] = iter->second;
			tx_map.erase(iter);
		}
		tx_pool.erase(txid);
	}
	{
		const auto range = challenge_map.equal_range(block->height);
		for(auto iter = range.first; iter != range.second; ++iter) {
			proof_map.erase(iter->second);
		}
		challenge_map.erase(range.first, range.second);
	}
	hash_index[block->hash] = block->height;
	history[block->height] = block->get_header();
	change_log.pop_front();

	// purge history
	while(history.size() > max_history) {
		history.erase(history.begin());
	}
	if(!history.empty()) {
		const auto begin = history.begin()->second->vdf_iters;
		verified_vdfs.erase(verified_vdfs.begin(), verified_vdfs.lower_bound(begin));
	}
	if(!is_replay) {
		const auto fork = find_fork(block->hash);
		Node::log(INFO) << "Committed height " << block->height << " with: ntx = " << block->tx_list.size()
				<< ", score = " << (fork ? fork->proof_score : 0) << ", k = " << (block->proof ? block->proof->ksize : 0)
				<< ", tdiff = " << block->time_diff << ", sdiff = " << block->space_diff;
	}
	fork_tree.erase(block->hash);
	purge_tree();

	if(!is_replay) {
		block_index[block->height] = std::make_pair(block_chain->get_output_pos(), block->hash);
		vnx::write(block_chain->out, block);
		block_chain->flush();
	}
	publish(block, output_committed_blocks);
}

void Node::purge_tree()
{
	const auto root = get_root();
	for(auto iter = fork_tree.begin(); iter != fork_tree.end();)
	{
		const auto& block = iter->second->block;
		if(block->height <= root->height) {
			iter = fork_tree.erase(iter);
		} else {
			iter++;
		}
	}
}

uint32_t Node::verify_proof(std::shared_ptr<const Block> block, const hash_t& vdf_challenge) const
{
	const auto prev = find_prev_header(block);
	if(!prev) {
		throw std::logic_error("invalid prev");
	}
	const auto diff_block = find_diff_header(block);
	if(!diff_block) {
		throw std::logic_error("cannot verify");
	}
	if(block->vdf_iters != prev->vdf_iters + diff_block->time_diff * params->time_diff_constant) {
		throw std::logic_error("invalid vdf_iters");
	}
	// Note: vdf_output already verified to match vdf_iters

	if(!block->proof) {
		return params->score_threshold;
	}
	const auto challenge = get_challenge(block, vdf_challenge);

	return verify_proof(block->proof, challenge, diff_block->space_diff);
}

uint32_t Node::verify_proof(std::shared_ptr<const ProofOfSpace> proof, const hash_t& challenge, const uint64_t space_diff) const
{
	if(proof->ksize < params->min_ksize) {
		throw std::logic_error("ksize too small");
	}
	if(proof->ksize > params->max_ksize) {
		throw std::logic_error("ksize too big");
	}
	const auto plot_key = proof->local_key.to_bls() + proof->farmer_key.to_bls();

	if(hash_t(proof->pool_key + bls_pubkey_t(plot_key)) != proof->plot_id) {
		throw std::logic_error("invalid proof keys");
	}
	if(!proof->local_sig.verify(proof->local_key, proof->calc_hash())) {
		throw std::logic_error("invalid proof signature");
	}
	if(!check_plot_filter(params, challenge, proof->plot_id)) {
		throw std::logic_error("plot filter failed");
	}
	const auto quality = hash_t::from_bytes(chiapos::verify(
			proof->ksize, proof->plot_id.bytes, challenge.bytes, proof->proof_bytes.data(), proof->proof_bytes.size()));

	const auto score = calc_proof_score(params, proof->ksize, quality, space_diff);
	if(score >= params->score_threshold) {
		throw std::logic_error("invalid score");
	}
	return score;
}

void Node::verify_vdf(std::shared_ptr<const ProofOfTime> proof, const vdf_point_t& prev) const
{
	// check proper infusions
	if(proof->start > 0) {
		if(proof->infuse[0].size() != 1) {
			throw std::logic_error("missing infusion on chain 0");
		}
		const auto infused = *proof->infuse[0].begin();
		if(infused.first != proof->start) {
			throw std::logic_error("invalid infusion point on chain 0: must be at start");
		}
		const auto infused_block = find_header(infused.second);
		if(!infused_block) {
			throw std::logic_error("invalid infusion value on chain 0");
		}
		uint64_t target_iters = infused_block->vdf_iters;
		for(size_t i = 0; i < params->finality_delay; ++i) {
			if(auto diff_block = find_diff_header(infused_block, i + 1)) {
				target_iters += diff_block->time_diff * params->time_diff_constant;
				if(infused_block->height == 0 && infused.first == target_iters) {
					break;	// genesis case
				}
			} else {
				throw std::logic_error("cannot verify");
			}
		}
		if(infused.first != target_iters) {
			throw std::logic_error("invalid infusion point on chain 0: " + std::to_string(infused.first) + " != " + std::to_string(target_iters));
		}
		const bool need_second = infused_block->height >= params->challenge_interval
				&& infused_block->height % params->challenge_interval == 0;

		if(proof->infuse[1].size() != (need_second ? 1 : 0)) {
			throw std::logic_error("wrong number of infusions on chain 1: " + std::to_string(proof->infuse[1].size()));
		}
		if(need_second) {
			if(*proof->infuse[1].begin() != infused) {
				throw std::logic_error("invalid infusion on chain 1");
			}
		}
	}
	verify_vdf(proof, 0, prev.output[0]);
	verify_vdf(proof, 1, prev.output[1]);
}

void Node::verify_vdf(std::shared_ptr<const ProofOfTime> proof, const uint32_t chain, const hash_t& begin) const
{
	const auto& segments = proof->segments;
	bool is_valid = !segments.empty();
	size_t invalid_segment = -1;

	std::vector<uint64_t> start_iters(segments.size());
	for(size_t i = 0; i < segments.size(); ++i) {
		if(i == 0) {
			start_iters[i] = proof->start;
		} else {
			start_iters[i] = start_iters[i - 1] + segments[i - 1].num_iters;
		}
	}

#pragma omp parallel for
	for(size_t i = 0; i < segments.size(); ++i)
	{
		hash_t point;
		if(i > 0) {
			point = segments[i - 1].output[chain];
		} else {
			point = begin;
		}
		{
			auto iter = proof->infuse[chain].find(start_iters[i]);
			if(iter != proof->infuse[chain].end()) {
				point = hash_t(point + iter->second);
			}
		}
		for(size_t k = 0; k < segments[i].num_iters; ++k) {
			point = hash_t(point.bytes);
		}
		if(point != segments[i].output[chain]) {
			is_valid = false;
			invalid_segment = i;
		}
	}
	if(!is_valid) {
		throw std::logic_error("invalid output at segment " + std::to_string(invalid_segment));
	}
}

void Node::apply(std::shared_ptr<const Block> block) noexcept
{
	if(block->prev != state_hash) {
		return;
	}
	const auto log = std::make_shared<change_log_t>();
	log->prev_state = state_hash;

	if(const auto& tx = block->tx_base) {
		apply(block, tx, 0, *log);
	}
	for(size_t i = 0; i < block->tx_list.size(); ++i) {
		apply(block, block->tx_list[i], 1 + i, *log);
	}
	state_hash = block->hash;
	change_log.push_back(log);
}

void Node::apply(std::shared_ptr<const Block> block, std::shared_ptr<const Transaction> tx, size_t index, change_log_t& log) noexcept
{
	for(size_t i = 0; i < tx->inputs.size(); ++i)
	{
		auto iter = utxo_map.find(tx->inputs[i].prev);
		if(iter != utxo_map.end()) {
			const auto key = txio_key_t::create_ex(tx->id, i);
			log.utxo_removed.emplace(iter->first, std::make_pair(key, iter->second));
			utxo_map.erase(iter);
		}
	}
	for(size_t i = 0; i < tx->outputs.size(); ++i)
	{
		const auto key = txio_key_t::create_ex(tx->id, i);
		const auto value = utxo_t::create_ex(tx->outputs[i], block->height);
		utxo_map[key] = value;
		log.utxo_added.emplace(key, value);
	}
	tx_key_t info;
	info.height = block->height;
	info.index = index;
	tx_map[tx->id] = info;
	log.tx_added.push_back(tx->id);
}

bool Node::revert() noexcept
{
	if(change_log.empty()) {
		return false;
	}
	const auto log = change_log.back();

	for(const auto& entry : log->utxo_added) {
		utxo_map.erase(entry.first);
	}
	for(const auto& entry : log->utxo_removed) {
		utxo_map.emplace(entry.first, entry.second.second);
	}
	for(const auto& txid : log->tx_added) {
		tx_map.erase(txid);
	}
	change_log.pop_back();
	state_hash = log->prev_state;
	return true;
}

std::shared_ptr<const BlockHeader> Node::get_root() const
{
	if(history.empty()) {
		return nullptr;
	}
	return (--history.end())->second;
}

std::shared_ptr<Node::fork_t> Node::find_fork(const hash_t& hash) const
{
	auto iter = fork_tree.find(hash);
	if(iter != fork_tree.end()) {
		return iter->second;
	}
	return nullptr;
}

std::shared_ptr<const Block> Node::find_block(const hash_t& hash) const
{
	if(auto fork = find_fork(hash)) {
		return fork->block;
	}
	return nullptr;
}

std::shared_ptr<const BlockHeader> Node::find_header(const hash_t& hash) const
{
	if(auto block = find_block(hash)) {
		return block;
	}
	auto iter = hash_index.find(hash);
	if(iter != hash_index.end()) {
		auto iter2 = history.find(iter->second);
		if(iter2 != history.end()) {
			return iter2->second;
		}
	}
	return nullptr;
}

std::shared_ptr<const BlockHeader> Node::find_prev_header(	std::shared_ptr<const BlockHeader> block,
															const size_t distance, bool clamped) const
{
	if(distance > params->finality_delay
		&& (block->height >= distance || clamped))
	{
		const auto height = block->height > distance ? block->height - distance : 0;
		auto iter = history.find(height);
		if(iter != history.end()) {
			return iter->second;
		}
	}
	for(size_t i = 0; block && i < distance; ++i)
	{
		if(clamped && block->height == 0) {
			break;
		}
		block = find_header(block->prev);
	}
	return block;
}

std::shared_ptr<const BlockHeader> Node::find_diff_header(std::shared_ptr<const BlockHeader> block, uint32_t offset) const
{
	if(offset > params->challenge_interval) {
		throw std::logic_error("offset out of range");
	}
	uint32_t height = block->height + offset;
	height -= (height % params->challenge_interval);
	if(auto prev = find_prev_header(block, (block->height + params->challenge_interval) - height, true)) {
		return prev;
	}
	return nullptr;
}

hash_t Node::get_challenge(std::shared_ptr<const BlockHeader> block, const hash_t& vdf_challenge, uint32_t offset) const
{
	if(auto diff_block = find_diff_header(block, offset)) {
		return hash_t(diff_block->hash + vdf_challenge);
	}
	return hash_t();
}

bool Node::find_vdf_challenge(std::shared_ptr<const BlockHeader> block, hash_t& vdf_challenge, uint32_t offset) const
{
	if(offset > params->challenge_delay) {
		throw std::logic_error("offset out of range");
	}
	if(auto vdf_block = find_prev_header(block, params->challenge_delay - offset, true)) {
		vdf_challenge = vdf_block->vdf_output[1];
		return true;
	}
	return false;
}

uint64_t Node::calc_block_reward(std::shared_ptr<const BlockHeader> block) const
{
	if(!block->proof) {
		return 0;
	}
	if(auto diff_block = find_diff_header(block)) {
		return mmx::calc_block_reward(params, diff_block->space_diff);
	}
	return 0;
}


} // mmx
