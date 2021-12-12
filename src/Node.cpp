/*
 * Node.cpp
 *
 *  Created on: Dec 7, 2021
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/TimePoint.hxx>
#include <mmx/Challenge.hxx>
#include <mmx/FarmerClient.hxx>
#include <mmx/IntervalRequest.hxx>
#include <mmx/contract/PubKey.hxx>
#include <mmx/chiapos.h>
#include <mmx/utils.h>

#include <atomic>


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

	auto genesis = Block::create();
	genesis->time_diff = params->initial_time_diff;
	genesis->space_diff = params->initial_space_diff;
	genesis->finalize();

	apply(genesis);
	commit(genesis);

	verified_vdfs[0] = hash_t(params->vdf_seed);

	set_timer_millis(update_interval_ms, std::bind(&Node::update, this));

	Super::main();
}

void Node::add_transaction(std::shared_ptr<const Transaction> tx)
{
	validate(tx);
	handle(tx);
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
	fork->recv_time = vnx_sample ? vnx_sample->recv_time : vnx::get_time_micros();
	fork->prev = find_fork(block->prev);
	fork->block = block;
	fork_tree[block->hash] = fork;
}

void Node::handle(std::shared_ptr<const Transaction> tx)
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
	}
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
		verify_vdf(proof, iter->second);
		verified_vdfs[vdf_iters] = proof->get_output();
		{
			auto point = TimePoint::create();
			point->num_iters = vdf_iters;
			point->output = proof->get_output();
			publish(point, output_verified_points, BLOCKING);
		}
		log(INFO) << "Verified VDF at " << vdf_iters << " iterations";

		update();
	}
}

void Node::handle(std::shared_ptr<const ProofResponse> value)
{
	if(!value->proof) {
		return;
	}
	auto& prev = proof_map[value->challenge];
	if(!prev || value->score < prev->score)
	{
		auto iter = challange_diff.find(value->challenge);
		if(iter != challange_diff.end()) {
			try {
				const auto score = verify_proof(value->proof, value->challenge, iter->second);
				if(score == value->score) {
					prev = value;
				} else {
					throw std::logic_error("score mismatch");
				}
			} catch(const std::exception& ex) {
				log(WARN) << "Got invalid proof: " << ex.what();
			}
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

	bool did_fork = false;
	std::shared_ptr<const BlockHeader> forked_at;

	// choose best fork
	while(true) {
		// purge disconnected forks
		purge_tree();

		uint64_t max_weight = 0;
		std::shared_ptr<fork_t> best_fork;

		// find best fork
		const auto root = get_root();
		for(const auto& entry : fork_tree)
		{
			const auto& fork = entry.second;
			const auto& block = fork->block;

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
		if(!best_fork || best_fork->block->hash == state_hash) {
			// no change
			break;
		}

		// we have a new winner
		std::list<std::shared_ptr<fork_t>> fork_line;
		{
			auto fork = best_fork;
			while(fork) {
				fork_line.push_front(fork);
				fork = fork->prev.lock();
			}
		}

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
			// commit to history
			if(fork_line.size() > params->finality_delay) {
				const size_t num_blocks = fork_line.size() - params->finality_delay;

				auto iter = fork_line.begin();
				for(size_t i = 0; i < num_blocks; ++i, ++iter) {
					commit((*iter)->block);
				}
			}
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

	// request next time points and check for making a new block
	std::pair<uint64_t, hash_t> next_vdf;
	std::shared_ptr<const BlockHeader> next_prev;
	{
		auto vdf_iters = peak->vdf_iters;
		for(uint32_t i = 0; i <= params->finality_delay; ++i)
		{
			if(auto diff_block = find_prev_header(peak, params->finality_delay - i, true))
			{
				auto request = IntervalRequest::create();
				request->begin = vdf_iters;
				request->end = vdf_iters + diff_block->time_diff * params->time_diff_constant;

				auto iter = verified_vdfs.find(request->end);
				if(iter != verified_vdfs.end()) {
					// we can make a block here
					if(auto prev = find_prev_header(peak, i)) {
						try {
							make_block(prev, *iter);
						} catch(const std::exception& ex) {
							log(WARN) << "Failed to create a block: " << ex.what();
						}
					}
				} else {
					request->interval = (params->block_time * 1e6) / params->num_vdf_segments;
					publish(request, output_interval_request);
				}
				vdf_iters = request->end;
			}
		}
	}

	// add dummy block in case no proof is found
	{
		const auto diff_block = get_diff_header(peak, true);
		const auto vdf_iters = peak->vdf_iters + diff_block->time_diff * params->time_diff_constant;

		hash_t vdf_output;
		if(find_vdf_output(vdf_iters, vdf_output))
		{
			auto block = Block::create();
			block->prev = peak->hash;
			block->height = peak->height + 1;
			block->time_diff = peak->time_diff;
			block->space_diff = peak->space_diff;
			block->vdf_iters = vdf_iters;
			block->vdf_output = vdf_output;
			block->finalize();
			handle(block);
		}
	}

	// publish challenges for next blocks
	{
		auto block = peak;
		for(uint32_t i = 0; block && i < params->challenge_delay; ++i)
		{
			auto value = Challenge::create();
			value->height = block->height + params->challenge_delay;
			{
				const auto height = value->height - (value->height % params->challenge_interval);
				if(auto prev = find_prev_header(block, block->height - height)) {
					value->challenge = hash_t(prev->hash + block->vdf_output);
				} else {
					continue;
				}
			}
			if(auto diff_block = find_prev_header(block, params->finality_delay - params->challenge_delay, true)) {
				value->space_diff = diff_block->space_diff;
			} else {
				continue;
			}
			challange_diff[value->challenge] = value->space_diff;

			publish(value, output_challanges);

			block = find_prev_header(block);
		}
	}

	if(!prev_peak || peak->hash != prev_peak->hash)
	{
		const auto fork = find_fork(peak->hash);
		log(INFO) << "New peak at height " << peak->height << " with score " << (fork ? std::to_string(fork->proof_score) : "?")
				<< (did_fork ? " (forked at " + std::to_string(forked_at ? forked_at->height : -1) + ")" : "");
	}
}

void Node::make_block(std::shared_ptr<const BlockHeader> prev, const std::pair<uint64_t, hash_t>& vdf_point)
{
	auto block = Block::create();
	block->prev = prev->hash;
	block->height = prev->height + 1;
	block->time_diff = prev->time_diff;
	block->space_diff = prev->space_diff;
	block->vdf_iters = vdf_point.first;
	block->vdf_output = vdf_point.second;

	hash_t vdf_challenge;
	if(!find_vdf_challenge(block, vdf_challenge)) {
		return;
	}
	const auto challenge = get_challenge(block, vdf_challenge);

	auto iter = proof_map.find(challenge);
	if(iter == proof_map.end()) {
		return;
	}
	const auto response = iter->second;
	block->proof = response->proof;

	uint64_t total_fees = 0;
	// TODO: transactions

	block->finalize();

	FarmerClient farmer(response->farmer_addr);
	const auto total_reward = std::max(calc_block_reward(block), total_fees);
	const auto result = farmer.sign_block(block, total_reward);

	block->tx_base = result.first;
	block->farmer_sig = result.second;
	block->finalize();

	handle(block);
	proof_map.erase(iter);

	log(INFO) << "Created block at height " << block->height << " with: score = " << response->score
			<< ", reward = " << total_reward / pow(10, params->decimals) << " MMX";
}

bool Node::calc_fork_weight(std::shared_ptr<const BlockHeader> root, std::shared_ptr<const fork_t> fork, uint64_t& total_weight)
{
	while(fork) {
		const auto& block = fork->block;
		if(fork->is_proof_verified) {
			total_weight += 2 * params->score_threshold - fork->proof_score;
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
	if(auto proof = block->proof) {
		if(!block->farmer_sig || !block->farmer_sig->verify(proof->farmer_key, block->hash)) {
			throw std::logic_error("invalid farmer signature");
		}
		// TODO: check time_diff + space_diff deltas
	} else {
		if(block->tx_base || !block->tx_list.empty()) {
			throw std::logic_error("transactions not allowed");
		}
		if(block->time_diff != prev->time_diff || block->space_diff != prev->space_diff) {
			throw std::logic_error("invalid difficulty");
		}
	}
	if(block->tx_list.size() >= params->max_tx_count) {
		throw std::logic_error("too many transactions");
	}
	uint64_t base_spent = 0;
	if(const auto& tx = block->tx_base) {
		base_spent = validate(tx, true);
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
	std::atomic<uint64_t> total_fee {0};

#pragma omp parallel for
	for(const auto& tx : block->tx_list) {
		total_fee += validate(tx);
	}

	const auto base_allowed = std::max(calc_block_reward(block), uint64_t(total_fee));
	if(base_spent > base_allowed) {
		throw std::logic_error("coin base over-spend");
	}
}

uint64_t Node::validate(std::shared_ptr<const Transaction> tx, bool is_base) const
{
	if(is_base) {
		if(!tx->inputs.empty()) {
			throw std::logic_error("coin base cannot have inputs");
		}
		if(!tx->execute.empty()) {
			throw std::logic_error("coin base cannot have operations");
		}
	} else {
		if(tx->inputs.empty()) {
			throw std::logic_error("tx without input");
		}
	}
	if(tx->outputs.size() >= params->max_tx_outputs) {
		throw std::logic_error("too many tx outputs");
	}

	uint64_t base_amount = 0;
	std::unordered_map<hash_t, uint64_t> amounts;

	for(const auto& in : tx->inputs)
	{
		auto iter = utxo_map.find(in.prev);
		if(iter == utxo_map.end()) {
			throw std::logic_error("utxo not found");
		}
		const auto& out = iter->second;

		// verify signature
		if(!in.solution) {
			throw std::logic_error("missing solution");
		}
		{
			auto iter = contracts.find(out.address);
			if(iter != contracts.end()) {
				if(!iter->second->validate(in.solution, tx->id)) {
					throw std::logic_error("invalid solution");
				}
			}
			else {
				contract::PubKey simple;
				simple.pubkey_hash = out.address;
				if(!simple.validate(in.solution, tx->id)) {
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
	for(const auto& out : tx->outputs)
	{
		if(out.amount == 0) {
			throw std::logic_error("zero tx output");
		}
		if(is_base) {
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
	if(is_base) {
		return base_amount;
	}
	const auto fee_amount = amounts[hash_t()];
	const auto fee_needed = tx->calc_min_fee(params);
	if(fee_amount < fee_needed) {
		throw std::logic_error("insufficient fee");
	}
	return fee_amount;
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

	finalized[block->hash] = block->height;
	history[block->height] = block->get_header();
	change_log.pop_front();

	const auto fork = find_fork(block->hash);
	Node::log(INFO) << "Committed height " << block->height << " with: ntx = " << block->tx_list.size()
			<< ", score = " << (fork ? fork->proof_score : 0) << ", k = " << (block->proof ? block->proof->ksize : 0);

	fork_tree.erase(block->hash);
	purge_tree();

	publish(block, output_committed_block, BLOCKING);
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
			if(block->prev != root->hash && fork_tree.find(block->prev) == fork_tree.end())
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
	tx_map[tx->id] = block->height;
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
	auto iter = finalized.find(hash);
	if(iter != finalized.end()) {
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

hash_t Node::get_challenge(std::shared_ptr<const BlockHeader> block, const hash_t& vdf_challenge) const
{
	uint32_t height = block->height > params->challenge_delay ? block->height - params->challenge_delay : 0;
	height -= (height % params->challenge_interval);

	if(auto prev = find_prev_header(block, block->height - height)) {
		return hash_t(prev->hash + vdf_challenge);
	}
	return hash_t();
}

bool Node::find_vdf_output(const uint64_t vdf_iters, hash_t& vdf_output) const
{
	auto iter = verified_vdfs.find(vdf_iters);
	if(iter != verified_vdfs.end()) {
		vdf_output = iter->second;
		return true;
	}
	return false;
}

bool Node::find_vdf_challenge(std::shared_ptr<const BlockHeader> block, hash_t& vdf_challenge) const
{
	if(auto vdf_block = find_prev_header(block, params->challenge_delay, true))
	{
		return find_vdf_output(vdf_block->vdf_iters, vdf_challenge);
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
