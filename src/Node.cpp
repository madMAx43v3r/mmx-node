/*
 * Node.cpp
 *
 *  Created on: Dec 7, 2021
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/Challenge.hxx>
#include <mmx/FarmerClient.hxx>
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
	vnx::open_pipe(vnx_name, this, 1000);

	subscribe(input_blocks, 1000);
	subscribe(input_transactions, 1000);
	subscribe(input_proof_of_time, 1000);
	subscribe(input_proof_of_space, 1000);
}

void Node::main()
{
	params = get_params();
	{
		vdf_point_t point;
		point.output = hash_t(params->vdf_seed);
		point.recv_time = vnx::get_time_micros();
		verified_vdfs[0] = point;
	}
	vnx::File fork_line(storage_path + "fork_line.dat");

	vdf_chain = std::make_shared<vnx::File>(storage_path + "vdf_chain.dat");
	block_chain = std::make_shared<vnx::File>(storage_path + "block_chain.dat");

	if(vdf_chain->exists())
	{
		vdf_chain->open("rb+");
		int64_t last_pos = 0;
		while(true) {
			auto& in = vdf_chain->in;
			try {
				last_pos = in.get_input_pos();
				if(auto value = vnx::read(in)) {
					if(auto proof = std::dynamic_pointer_cast<ProofOfTime>(value)) {
						vdf_point_t point;
						point.recv_time = vnx::get_time_micros();
						point.output = proof->get_output();
						const auto vdf_iters = proof->start + proof->get_num_iters();
						verified_vdfs[vdf_iters] = point;
						vdf_index.emplace(vdf_iters, last_pos);
					}
				} else {
					break;
				}
			}
			catch(const std::exception& ex) {
				log(WARN) << "Failed to read VDF: " << ex.what();
				break;
			}
		}
		vdf_chain->seek_to(last_pos);
		log(INFO) << "Loaded " << verified_vdfs.size() << " VDFs from disk";
	} else {
		vdf_chain->open("ab");
	}

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

	if(state_hash == hash_t())
	{
		auto genesis = Block::create();
		genesis->time_diff = params->initial_time_diff;
		genesis->space_diff = params->initial_space_diff;
		genesis->finalize();

		apply(genesis);
		commit(genesis);
	}

	if(!verified_vdfs.empty())
	{
		const auto iter = --verified_vdfs.end();
		auto proof = ProofOfTime::create();
		proof->start = iter->first;
		proof->segments.resize(1);
		proof->segments[0].output = iter->second.output;
		publish(proof, output_verified_vdfs, BLOCKING);
	}

	set_timer_millis(update_interval_ms, std::bind(&Node::update, this));

	Super::main();

	fork_line.open("wb");
	for(const auto& fork : get_fork_line()) {
		vnx::write(fork_line.out, fork->block);
	}
	vnx::write(fork_line.out, nullptr);
	fork_line.close();

	vnx::write(vdf_chain->out, nullptr);
	vnx::write(block_chain->out, nullptr);

	vdf_chain->close();
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

hash_t Node::get_block_hash(const uint32_t& height) const
{
	auto iter = block_index.find(height);
	if(iter != block_index.end()) {
		return iter->second.second;
	}
	if(auto block = get_block_at(height)) {
		return block->hash;
	}
	throw std::logic_error("no such height");
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
			if(auto block = get_block_at(entry.first)) {
				if(entry.second == (typeof(entry.second))(-1)) {
					return block->tx_base;
				}
				return block->tx_list.at(entry.second);
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
	auto fork = std::make_shared<fork_t>();
	fork->recv_time = vnx::get_time_micros();
	fork->prev = find_fork(block->prev);
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
	try {
		validate(tx);
		tx_pool[tx->id] = tx;
	}
	catch(const std::exception& ex) {
		log(WARN) << "TX validation failed with: " << ex.what();
		throw;
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

std::vector<std::pair<utxo_key_t, tx_out_t>> Node::get_utxo_list(const std::vector<addr_t>& addresses) const
{
	std::vector<std::pair<utxo_key_t, tx_out_t>> res;
	for(const auto& addr : addresses) {
		const auto range = addr_map.equal_range(addr);
		for(auto iter = range.first; iter != range.second; ++iter) {
			auto iter2 = utxo_map.find(iter->second);
			if(iter2 != utxo_map.end()) {
				const auto& out = iter2->second;
				res.emplace_back(iter->second, out);
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
	auto root = get_root();
	if(vdf_iters <= root->vdf_iters) {
		return;
	}
	auto iter = verified_vdfs.find(proof->start);
	if(iter != verified_vdfs.end())
	{
		const auto& prev = iter->second;
		verify_vdf(proof, prev.output);

		vdf_point_t point;
		point.output = proof->get_output();
		point.recv_time = vnx_sample ? vnx_sample->recv_time : vnx::get_time_micros();
		verified_vdfs[vdf_iters] = point;

		if(!is_replay) {
			vdf_index.emplace(vdf_iters, vdf_chain->get_output_pos());
			vnx::write(vdf_chain->out, proof->compressed());
			vdf_chain->flush();
		}
		publish(proof, output_verified_vdfs, BLOCKING);

		log(INFO) << "Verified VDF at " << vdf_iters << " iterations, delta = "
				<< (point.recv_time - prev.recv_time) / 1e6 << " sec";

		update();
	}
}

void Node::handle(std::shared_ptr<const ProofResponse> value)
{
	if(!value->proof || !value->request || value->score >= params->score_threshold) {
		return;
	}
	const auto challenge = value->request->challenge;

	auto iter = proof_map.find(challenge);
	if(iter == proof_map.end() || value->score < iter->second->score)
	{
		try {
			const auto score = verify_proof(value->proof, challenge, value->request->space_diff);
			if(score == value->score) {
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

	for(auto iter = fork_tree.begin(); iter != fork_tree.end();)
	{
		const auto& fork = iter->second;
		const auto& block = fork->block;

		if(!fork->is_proof_verified)
		{
			hash_t vdf_output;
			if(find_vdf_output(block->vdf_iters, vdf_output))
			{
				if(block->vdf_output != vdf_output) {
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

// TODO: parallel is an issue
//#pragma omp parallel for
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
	const auto root = get_root();
	const auto prev_peak = find_header(state_hash);

	bool did_fork = false;
	std::shared_ptr<const BlockHeader> forked_at;

	// choose best fork
	while(true) {
		// purge disconnected forks
		purge_tree();

		const auto best_fork = find_best_fork();

		if(!best_fork || best_fork->block->hash == state_hash) {
			// no change
			break;
		}

		// we have a new winner
		const auto fork_line = get_fork_line(best_fork);

		// bring state back in line with new fork
		while(true) {
			bool found = false;
			for(const auto& fork : fork_line) {
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
				forked_at = root;
				break;
			}
		}

		// verify and apply new fork
		bool is_valid = true;
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
					is_valid = false;
					break;
				}
				publish(block, output_verified_blocks, BLOCKING);
			}
			apply(block);
		}
		if(is_valid) {
			break;
		}
		// try again
		did_fork = false;
		forked_at = nullptr;
	}

	const auto peak = find_header(state_hash);
	if(!peak) {
		log(WARN) << "Have no peak!";
		return;
	}

	if(!prev_peak || peak->hash != prev_peak->hash)
	{
		const auto fork = find_fork(peak->hash);
		log(INFO) << "New peak at height " << peak->height << " with score " << (fork ? std::to_string(fork->proof_score) : "?")
				<< (did_fork ? " (forked at " + std::to_string(forked_at ? forked_at->height : -1) + ")" : "");
	}

	// try to make a block
	bool made_block = false;
	for(uint32_t prev_height = root->height; prev_height <= peak->height; ++prev_height)
	{
		std::shared_ptr<const BlockHeader> prev;
		if(prev_height == root->height) {
			prev = root;
		} else if(auto fork = find_best_fork(&prev_height)) {
			prev = fork->block;
		} else {
			continue;
		}
		auto diff_block = find_prev_header(prev, params->finality_delay, true);
		if(!diff_block) {
			continue;
		}
		{
			// request proof of time
			auto request = IntervalRequest::create();
			request->begin = prev->vdf_iters;
			request->end = prev->vdf_iters + diff_block->time_diff * params->time_diff_constant;
			request->interval = (params->block_time * 1e6) / params->num_vdf_segments;

			auto iter = verified_vdfs.find(request->end);
			if(iter != verified_vdfs.end()) {
				// add dummy block in case no proof is found
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
			else {
				publish(request, output_interval_request);
			}
		}
		hash_t vdf_challenge;
		if(!find_vdf_challenge(prev, vdf_challenge, 1)) {
			continue;
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
		if(!made_block) {
			auto iter = proof_map.find(challenge);
			if(iter != proof_map.end()) {
				try {
					if(make_block(prev, iter->second)) {
						// update again right away
						add_task(std::bind(&Node::update, this));
						// only make one block at a time
						made_block = true;
						proof_map.erase(iter);
					}
				}
				catch(const std::exception& ex) {
					log(WARN) << "Failed to create a block: " << ex.what();
				}
			}
		}
	}

	// publish advance challenges
	for(uint32_t i = 2; i <= params->challenge_delay; ++i)
	{
		hash_t vdf_challenge;
		if(!find_vdf_challenge(peak, vdf_challenge, i)) {
			continue;
		}
		const auto challenge = get_challenge(peak, vdf_challenge, i);

		auto value = Challenge::create();
		value->height = peak->height + i;
		value->challenge = challenge;
		if(auto diff_block = find_prev_header(peak, (params->finality_delay + 1) - i, true)) {
			value->space_diff = diff_block->space_diff;
			publish(value, output_challenges);
		}
	}

	// commit mature blocks
	const auto now = vnx::get_time_micros();
	for(const auto& fork : get_fork_line()) {
		const auto elapsed = 1e-6 * (now - fork->recv_time);
		if(elapsed > params->block_time * params->finality_delay) {
			commit(fork->block);
		}
	}
}

bool Node::make_block(std::shared_ptr<const BlockHeader> prev, std::shared_ptr<const ProofResponse> response)
{
	auto block = Block::create();
	block->prev = prev->hash;
	block->height = prev->height + 1;
	block->time_diff = prev->time_diff;
	block->space_diff = prev->space_diff;

	const auto diff_block = get_diff_header(block);
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
	std::unordered_set<utxo_key_t> spent;
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

	block->tx_base = result.first;
	block->farmer_sig = result.second;
	block->finalize();

	add_block(block);

	log(INFO) << "Created block at height " << block->height << " with: ntx = " << block->tx_list.size()
			<< ", score = " << response->score << ", reward = " << final_reward / pow(10, params->decimals) << " MMX"
			<< ", nominal = " << block_reward / pow(10, params->decimals) << " MMX"
			<< ", fees = " << total_fees / pow(10, params->decimals) << " MMX";
	return true;
}

std::shared_ptr<Node::fork_t> Node::find_best_fork(const uint32_t* fork_height) const
{
	uint128_t max_weight = 0;
	std::shared_ptr<fork_t> best_fork;

	const auto root = get_root();
	for(const auto& entry : fork_tree)
	{
		const auto& fork = entry.second;
		const auto& block = fork->block;
		if(fork_height && block->height != *fork_height) {
			continue;
		}
		uint128_t weight = 0;
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

bool Node::calc_fork_weight(std::shared_ptr<const BlockHeader> root, std::shared_ptr<fork_t> fork, uint128_t& total_weight) const
{
	while(fork) {
		const auto& block = fork->block;
		if(!fork->diff_block) {
			fork->diff_block = find_prev_header(block, params->finality_delay + 1, true);
		}
		const auto& diff_block = fork->diff_block;
		if(fork->is_proof_verified && diff_block) {
			total_weight += uint128_t(2 * params->score_threshold - fork->proof_score) * diff_block->time_diff * diff_block->space_diff;
		} else {
			return false;
		}
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
		std::unordered_set<utxo_key_t> inputs;
		inputs.reserve(16 * 1024);
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
		throw failed_ex;
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
					simple.pubkey_hash = out.address;
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
		throw std::logic_error("invalid difficulty adjustment");
	}
	if(block < prev && prev - block > max_update) {
		throw std::logic_error("invalid difficulty adjustment");
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
	{
		std::vector<std::unordered_multimap<addr_t, utxo_key_t>::iterator> to_remove;
#pragma omp parallel for
		for(const auto& entry : log->utxo_removed)
		{
			const auto range = addr_map.equal_range(entry.second.address);
			for(auto iter = range.first; iter != range.second; ++iter) {
				if(iter->second == entry.first) {
#pragma omp critical
					to_remove.push_back(iter);
				}
			}
		}
		for(const auto& iter : to_remove) {
			// other iterators are not invalidated by calling erase()
			addr_map.erase(iter);
		}
	}
	for(const auto& entry : log->utxo_added) {
		addr_map.emplace(entry.second.address, entry.first);
	}
	for(const auto& txid : log->tx_added) {
		tx_map.erase(txid);
		tx_pool.erase(txid);
	}
	if(const auto& tx = block->tx_base) {
		tx_index[tx->id] = std::make_pair(block->height, -1);
	}
	for(size_t i = 0; i < block->tx_list.size(); ++i) {
		tx_index[block->tx_list[i]->id] = std::make_pair(block->height, i);
	}
	hash_index[block->hash] = block->height;
	history[block->height] = block->get_header();
	change_log.pop_front();

	// purge history
	while(history.size() > max_history)
	{
		const auto iter = history.begin();
		const auto& block = iter->second;
		verified_vdfs.erase(block->vdf_iters);
		history.erase(iter);
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
	publish(block, output_committed_blocks, BLOCKING);
}

size_t Node::purge_tree()
{
	const auto root = get_root();

	bool repeat = true;
	size_t num_purged = 0;
	while(repeat) {
		repeat = false;
		for(auto iter = fork_tree.begin(); iter != fork_tree.end();)
		{
			const auto& block = iter->second->block;
			if(block->prev != root->hash && !fork_tree.count(block->prev))
			{
				iter = fork_tree.erase(iter);
				repeat = true;
				num_purged++;
			} else {
				iter++;
			}
		}
	}
	return num_purged;
}

uint32_t Node::verify_proof(std::shared_ptr<const Block> block, const hash_t& vdf_challenge) const
{
	const auto prev = find_prev_header(block);
	if(!prev) {
		throw std::logic_error("invalid prev");
	}
	const auto diff_block = get_diff_header(block);

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

bool Node::verify_vdf(std::shared_ptr<const ProofOfTime> proof, const hash_t& begin) const
{
	const auto& segments = proof->segments;
	bool is_valid = !segments.empty();

#pragma omp parallel for
	for(size_t i = 0; i < segments.size(); ++i)
	{
		hash_t point;
		if(i > 0) {
			point = segments[i - 1].output;
		} else {
			point = begin;
		}
		for(size_t k = 0; k < segments[i].num_iters; ++k) {
			point = hash_t(point.bytes);
		}
		if(point != segments[i].output) {
			is_valid = false;
		}
	}
	return is_valid;
}

void Node::apply(std::shared_ptr<const Block> block) noexcept
{
	if(block->prev != state_hash) {
		return;
	}
	const auto log = std::make_shared<change_log_t>();
	log->prev_state = state_hash;

	if(const auto& tx = block->tx_base) {
		apply(block, tx, *log);
	}
	for(const auto& tx : block->tx_list) {
		apply(block, tx, *log);
	}
	state_hash = block->hash;
	change_log.push_back(log);
}

void Node::apply(std::shared_ptr<const Block> block, std::shared_ptr<const Transaction> tx, change_log_t& log) noexcept
{
	for(const auto& in : tx->inputs)
	{
		auto iter = utxo_map.find(in.prev);
		if(iter != utxo_map.end()) {
			log.utxo_removed.push_back(*iter);
			utxo_map.erase(iter);
		}
	}
	for(size_t i = 0; i < tx->outputs.size(); ++i)
	{
		utxo_key_t key;
		key.txid = tx->id;
		key.index = i;
		utxo_map[key] = tx->outputs[i];
		log.utxo_added.emplace_back(key, tx->outputs[i]);
	}
	tx_map[tx->id] = block->hash;
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
		utxo_map[entry.first] = entry.second;
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
															const size_t distance, bool clamp_to_genesis) const
{
	if(distance > params->finality_delay)
	{
		const auto height = block->height > distance ? block->height - distance : 0;
		auto iter = history.find(height);
		if(iter != history.end()) {
			return iter->second;
		}
	}
	for(size_t i = 0; block && i < distance; ++i)
	{
		if(clamp_to_genesis && block->height == 0) {
			break;
		}
		block = find_header(block->prev);
	}
	return block;
}

std::shared_ptr<const BlockHeader> Node::get_diff_header(std::shared_ptr<const BlockHeader> block, bool for_next) const
{
	if(auto header = find_prev_header(block, params->finality_delay + (for_next ? 0 : 1), true)) {
		return header;
	}
	throw std::logic_error("cannot find difficulty header");
}

hash_t Node::get_challenge(std::shared_ptr<const BlockHeader> block, const hash_t& vdf_challenge, uint32_t offset) const
{
	if(offset > params->challenge_delay) {
		throw std::logic_error("offset out of range");
	}
	uint32_t height = block->height + offset;
	height -= std::min(params->challenge_delay, height);
	height -= (height % params->challenge_interval);

	if(auto prev = find_prev_header(block, block->height - height)) {
		return hash_t(prev->hash + vdf_challenge);
	}
	return hash_t();
}

bool Node::find_vdf_challenge(std::shared_ptr<const BlockHeader> block, hash_t& vdf_challenge, uint32_t offset) const
{
	if(offset > params->challenge_delay) {
		throw std::logic_error("offset out of range");
	}
	if(auto vdf_block = find_prev_header(block, params->challenge_delay - offset, true)) {
		vdf_challenge = vdf_block->vdf_output;
		return true;
	}
	return false;
}

bool Node::find_vdf_output(const uint64_t vdf_iters, hash_t& vdf_output) const
{
	auto iter = verified_vdfs.find(vdf_iters);
	if(iter != verified_vdfs.end()) {
		vdf_output = iter->second.output;
		return true;
	}
	return false;
}

uint64_t Node::calc_block_reward(std::shared_ptr<const BlockHeader> block) const
{
	if(!block->proof) {
		return 0;
	}
	const auto diff_block = get_diff_header(block);
	return mmx::calc_block_reward(params, diff_block->space_diff);
}


} // mmx
