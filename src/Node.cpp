/*
 * Node.cpp
 *
 *  Created on: Dec 7, 2021
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/contract/PubKey.hxx>
#include <mmx/chiapos.h>

#include <vnx/vnx.h>
#include <atomic>


namespace mmx {

Node::Node(const std::string& _vnx_name)
	:	NodeBase(_vnx_name)
{
}

void Node::init()
{
	subscribe(input_blocks);
	subscribe(input_transactions);
	subscribe(input_proof_of_time);
}

void Node::main()
{
	{
		auto tmp = ChainParams::create();
		vnx::read_config("chain.params", tmp);
		params = tmp;
	}
	auto genesis = Block::create();
	genesis->time_ms = vnx::get_time_millis();
	genesis->time_diff = params->initial_time_diff;
	genesis->space_diff = params->initial_space_diff;
	genesis->finalize();

	apply(genesis);

	history[genesis->height] = genesis;
	verified_vdfs[0] = hash_t(params->vdf_seed);

	set_timer_millis(update_interval_ms, std::bind(&Node::update, this));

	Super::main();
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
	fork->block = block;
	fork->prev = find_fork(block->prev);
	fork_tree[block->hash] = fork;
}

void Node::handle(std::shared_ptr<const Transaction> tx)
{
	// TODO
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
		log(INFO) << "Verified VDF at " << vdf_iters << " iterations";
	}
}

void Node::update()
{
	// verify proof where possible
	for(auto iter = fork_tree.begin(); iter != fork_tree.end();)
	{
		const auto& fork = iter->second;
		if(fork->is_proof_verified) {
			continue;
		}
		const auto& block = fork->block;

		hash_t vdf_output;
		if(find_vdf_output(block->vdf_iters, vdf_output))
		{
			if(block->vdf_output != vdf_output) {
				log(WARN) << "VDF verification failed for a block at height " << block->height;
				iter = fork_tree.erase(iter);
				continue;
			}
			hash_t vdf_challenge;
			if(find_vdf_challenge(block, vdf_challenge))
			{
				try {
					fork->proof_score = verify_proof(block, vdf_challenge);
					fork->is_proof_verified = true;
				}
				catch(const std::exception& ex) {
					log(WARN) << "Proof verification failed for a block at height " << block->height << " with: " << ex.what();
					iter = fork_tree.erase(iter);
					continue;
				}
			}
		}
		iter++;
	}
	bool did_fork = false;
	bool did_update = false;
	std::shared_ptr<const BlockHeader> forked_at;

	// choose best fork
	while(true) {
		// purge disconnected forks
		purge_tree();

		uint64_t max_weight = 0;
		std::shared_ptr<fork_t> best_fork;

		// find best fork
		const auto root = get_root();
		for(const auto& entry : fork_tree) {
			const auto& fork = entry.second;

			uint64_t weight = 0;
			if(calc_fork_weight(root, fork, weight))
			{
				if(!best_fork || weight > max_weight || (weight == max_weight && fork->block->hash < best_fork->block->hash))
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
					log(WARN) << "Block verification failed for height " << block->height << " with:" << ex.what();
					fork_tree.erase(block->hash);
					is_valid = false;
					break;
				}
				publish(block, output_blocks);
			}
			apply(block);
		}
		if(is_valid) {
			// commit to history
			if(fork_line.size() > params->finality_delay) {
				commit(fork_line.front()->block);
			}
			did_update = true;
			break;
		}
		// try again
		did_fork = false;
		forked_at = nullptr;
	}

	if(did_update) {
		if(auto peak = find_header(state_hash)) {
			log(INFO) << "New peak at height " << peak->height
					<< (did_fork ? " (forked at " + std::to_string(forked_at ? forked_at->height : -1) + ")" : "");
		}
	}
}

bool Node::calc_fork_weight(std::shared_ptr<const BlockHeader> root, std::shared_ptr<const fork_t> fork, uint64_t& total_weight)
{
	while(fork) {
		const auto& block = fork->block;
		if(fork->is_proof_verified && fork->proof_score < params->score_threshold) {
			total_weight += params->score_threshold;
			total_weight += params->score_threshold - fork->proof_score;
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
	auto prev = find_prev_header(block);
	if(!prev) {
		throw std::logic_error("invalid prev");
	}
	if(state_hash != prev->hash) {
		throw std::logic_error("state mismatch");
	}
	if(block->height != prev->height + 1) {
		throw std::logic_error("invalid height");
	}
	// TODO: check time_diff + space_diff deltas

	uint64_t base_spent = 0;
	if(auto tx = block->tx_base) {
		base_spent = validate(tx, true);
	}
	{
		std::unordered_set<utxo_key_t> inputs;
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
	const auto diff_block = get_diff_header(block);
	const auto block_reward = (diff_block->space_diff * params->reward_factor.value) / params->reward_factor.inverse;
	const auto base_allowed = std::max(std::max(block_reward, params->min_reward), uint64_t(total_fee));
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
	}
	const auto txid = tx->calc_hash();

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
				if(!iter->second->validate(in.solution, txid)) {
					throw std::logic_error("invalid solution");
				}
			}
			else {
				contract::PubKey simple;
				simple.pubkey_hash = out.address;
				if(!simple.validate(in.solution, txid)) {
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
		if(is_base) {
			if(out.contract != hash_t()) {
				throw std::logic_error("invalid coin base output");
			}
			base_amount += out.amount;
		}
		else {
			auto& value = amounts[out.contract];
			if(out.amount > value) {
				throw std::logic_error("output > input");
			}
			value -= out.amount;
		}
	}
	if(is_base) {
		return base_amount;
	}
	const auto fee_amount = amounts[hash_t()];
	const auto fee_needed =
			(tx->inputs.size() + tx->outputs.size()) * params->min_txfee_inout
			+ tx->execute.size() * params->min_txfee_exec;
	if(fee_amount < fee_needed) {
		throw std::logic_error("insufficient txfee");
	}
	return fee_amount;
}

void Node::commit(std::shared_ptr<const Block> block) noexcept
{
	const auto root = get_root();
	if(block->prev != root->hash) {
		return;
	}
	finalized[block->hash] = block->height;
	history[block->height] = block->get_header();

	fork_tree.erase(block->hash);
	purge_tree();
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
			if(block->prev != root->hash && fork_tree.find(block->prev) == fork_tree.end()) {
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
	auto prev = find_prev_header(block);
	if(!prev) {
		throw std::logic_error("invalid prev");
	}
	auto diff_block = get_diff_header(block);

	if(block->vdf_iters != prev->vdf_iters + diff_block->time_diff * (1 + block->deficit)) {
		throw std::logic_error("invalid vdf_iters");
	}
	// Note: vdf_output already verified to match vdf_iters

	if(auto proof = block->proof)
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

		const auto challenge = get_challenge(block, vdf_challenge);
		if(!check_plot_filter(challenge, proof->plot_id)) {
			throw std::logic_error("plot filter failed");
		}
		const auto quality = hash_t::from_bytes(chiapos::verify(
				proof->ksize, proof->plot_id.bytes, challenge.bytes, proof->proof_bytes.data(), proof->proof_bytes.size()));

		const auto score = calc_proof_score(proof->ksize, quality, diff_block->space_diff);
		if(score >= params->score_threshold) {
			throw std::logic_error("invalid score");
		}
		return score;
	}
	else {
		throw std::logic_error("missing proof");
	}
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

	if(auto tx = block->tx_base) {
		apply(*log, tx);
	}
	for(auto tx : block->tx_list) {
		apply(*log, tx);
	}
	state_hash = block->hash;
	change_log.push_back(log);
}

void Node::apply(change_log_t& log, std::shared_ptr<const Transaction> tx) noexcept
{
	// TODO
}

bool Node::revert() noexcept
{
	if(change_log.empty()) {
		return false;
	}
	const auto log = change_log.back();

	// TODO

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

std::shared_ptr<const BlockHeader> Node::get_diff_header(std::shared_ptr<const BlockHeader> block) const
{
	if(auto header = find_prev_header(block, params->finality_delay + 1, true)) {
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
		return find_vdf_output(vdf_block->vdf_iters + block->deficit * vdf_block->time_diff, vdf_challenge);
	}
	return false;
}

bool Node::check_plot_filter(const hash_t& challenge, const hash_t& plot_id) const
{
	return hash_t(challenge + plot_id).to_uint256() >> (256 - params->plot_filter);
}

uint128_t Node::calc_proof_score(const uint8_t ksize, const hash_t& quality, const uint64_t difficulty) const
{
	uint128_t modulo = uint128_t(difficulty) * params->space_diff_constant;
	modulo /= (2 * ksize) + 1;
	modulo >>= ksize - 1;
	return quality.to_uint256() % modulo;
}


} // mmx
