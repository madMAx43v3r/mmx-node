/*
 * Node.cpp
 *
 *  Created on: Dec 7, 2021
 *      Author: mad
 */

#include <mmx/Node.h>

#include <vnx/vnx.h>


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

	Super::main();
}

void Node::handle(std::shared_ptr<const Block> block)
{
	if(fork_tree.count(block->hash)) {
		return;
	}
	if(!block->proof) {
		return;
	}
	if(!find_prev_block(block)) {
		return;
	}
	auto root = get_root();
	if(block->height <= root->height) {
		return;
	}
	if(!block->is_valid()) {
		return;
	}
	fork_tree[block->hash] = block;

	update();
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
	if(proof->start < root->vdf_iters) {
		return;
	}
	auto iter = verified_vdfs.find(proof->start);
	if(iter != verified_vdfs.end())
	{
		verify(proof, iter->second);
		verified_vdfs[vdf_iters] = proof->get_output();

		log(INFO) << "Verified VDF at " << vdf_iters << " iterations";

		update();
	}
}

void Node::update()
{
	// verify proofs where possible
	for(auto iter = fork_tree.begin(); iter != fork_tree.end();)
	{
		const auto& block = iter->second;
		if(verified_proofs.count(block->hash)) {
			continue;
		}
		{
			auto iter2 = verified_vdfs.find(block->vdf_iters);
			if(iter2 != verified_vdfs.end()) {
				if(block->vdf_output != iter2->second) {
					log(WARN) << "VDF verification failed for a block at height " << block->height;
					iter = fork_tree.erase(iter);
					continue;
				}
			} else {
				iter++;
				continue;
			}
		}
		hash_t vdf_challenge;
		{
			auto prev = find_prev_header(block, params->challenge_delay, true);
			auto iter2 = verified_vdfs.find(prev->vdf_iters + block->deficit * prev->time_diff);
			if(iter2 != verified_vdfs.end()) {
				vdf_challenge = iter2->second;
			} else {
				iter++;
				continue;
			}
		}
		try {
			const auto score = verify_proof(block, vdf_challenge);
			verified_proofs[block->hash] = score;
		}
		catch(const std::exception& ex) {
			log(WARN) << "Proof verification failed for a block at height " << block->height << " with: " << ex.what();
			iter = fork_tree.erase(iter);
			continue;
		}
		iter++;
	}

	std::unordered_map<hash_t, size_t> node_degree;
	for(const auto& entry : fork_tree) {
		const auto& block = entry.second;
		node_degree[block->prev]++;
	}

	// find all blocks which don't have a child (those are fork heads)
	std::vector<std::shared_ptr<const Block>> forks;
	for(const auto& entry : fork_tree) {
		const auto& block = entry.second;
		if(!node_degree.count(block->hash)) {
			forks.push_back(block);
		}
	}
	uint64_t max_weight = 0;
	std::shared_ptr<const Block> best_fork;

	// find best fork
	for(auto fork : forks) {
		uint64_t weight = 0;
		if(calc_fork_weight(fork, weight))
		{
			if(!best_fork || weight > max_weight || (weight == max_weight && fork->hash < best_fork->hash))
			{
				best_fork = fork;
				max_weight = weight;
			}
		}
	}

	if(best_fork && best_fork->hash != state_hash)
	{
		// we have a new winner
		// TODO
	}
}

bool Node::calc_fork_weight(std::shared_ptr<const Block> block, uint64_t& total_weight)
{
	while(block) {
		auto iter = verified_proofs.find(block->hash);
		if(iter != verified_proofs.end()) {
			total_weight += params->score_threshold - iter->second;
		} else {
			return false;
		}
		block = find_prev_block(block);
	}
	return true;
}

void Node::verify(std::shared_ptr<const Block> block) const
{
	auto prev = find_prev_block(block);
	if(!prev) {
		throw std::logic_error("invalid prev");
	}
	if(block->height != prev->height + 1) {
		throw std::logic_error("invalid height");
	}
	// TODO
}

uint64_t Node::verify_proof(std::shared_ptr<const Block> block, const hash_t& vdf_challenge) const
{
	auto prev = find_prev_block(block);
	if(!prev) {
		throw std::logic_error("invalid prev");
	}
	auto diff_block = find_prev_header(block, params->finality_delay, true);

	if(block->vdf_iters != prev->vdf_iters + diff_block->time_diff * (1 + block->deficit)) {
		throw std::logic_error("invalid vdf_iters");
	}
	// Note: vdf_output already verified to match vdf_iters

	const hash_t block_challenge = get_challenge(block);
	const hash_t challenge(block_challenge + vdf_challenge);

	if(auto proof = block->proof)
	{
		if(proof->ksize < params->min_ksize) {
			throw std::logic_error("ksize too small");
		}
		if(proof->ksize > params->max_ksize) {
			throw std::logic_error("ksize too big");
		}
		uint256_t quality = 0;
		// TODO
		uint128_t modulo = uint128_t(diff_block->space_diff) * params->space_diff_constant;
		modulo /= (2 * proof->ksize) + 1;
		modulo >>= proof->ksize - 1;

		const uint128_t score = quality % modulo;
		if(score >= params->score_threshold) {
			throw std::logic_error("invalid score");
		}
		return score;
	}
	else {
		throw std::logic_error("missing proof");
	}
}

bool Node::verify(std::shared_ptr<const ProofOfTime> proof, const hash_t& begin) const
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
	const auto log = std::make_shared<ChangeLog>();
	log->prev_state_hash = state_hash;

	if(auto tx = block->tx_base) {
		apply(*log, tx);
	}
	for(auto tx : block->tx_list) {
		apply(*log, tx);
	}
	state_hash = block->hash;
	change_log.push_back(log);
}

void Node::apply(ChangeLog& log, std::shared_ptr<const Transaction> tx) noexcept
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
	state_hash = log->prev_state_hash;
	return true;
}

std::shared_ptr<const BlockHeader> Node::get_root() const
{
	if(history.empty()) {
		return nullptr;
	}
	return (--history.end())->second;
}

std::shared_ptr<const Block> Node::find_block(const hash_t& hash) const
{
	auto iter = fork_tree.find(hash);
	if(iter != fork_tree.end()) {
		return iter->second;
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

std::shared_ptr<const Block> Node::find_prev_block(std::shared_ptr<const Block> block, const size_t distance) const
{
	for(size_t i = 0; block && i < distance; ++i)
	{
		block = find_block(block->prev);
	}
	return block;
}

std::shared_ptr<const BlockHeader> Node::find_prev_header(	std::shared_ptr<const BlockHeader> block,
															const size_t distance, bool clamp_to_genesis) const
{
	for(size_t i = 0; block && i < distance; ++i)
	{
		if(clamp_to_genesis && block->height == 0) {
			break;
		}
		block = find_header(block->prev);
	}
	return block;
}

hash_t Node::get_challenge(std::shared_ptr<const BlockHeader> block) const
{
	uint32_t height = block->height > params->challenge_delay ? block->height - params->challenge_delay : 0;
	height -= (height % params->challenge_interval);

	if(auto prev = find_prev_header(block, block->height - height)) {
		return prev->hash;
	}
	return hash_t();
}


} // mmx
