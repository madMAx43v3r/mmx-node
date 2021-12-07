/*
 * Node.cpp
 *
 *  Created on: Dec 7, 2021
 *      Author: mad
 */

#include <mmx/Node.h>

#include <vnx/Config.h>


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
	auto root = get_root();
	if(block->height <= root->height) {
		return;
	}
	if(!block->is_valid()) {
		return;
	}
	fork_tree[block->hash] = block;

	check_forks();
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

		check_forks();
	}
}

void Node::check_forks()
{
	std::unordered_map<hash_t, std::vector<std::shared_ptr<const Block>>> forward_map;

	for(const auto& entry : fork_tree) {
		const auto& block = entry.second;
		forward_map[block->prev].push_back(block);
	}
	auto root = get_root();

	// TODO
}

void Node::verify(std::shared_ptr<const Block> block) const
{
	auto prev = find_prev_block(block, 1);
	if(!prev) {
		throw std::logic_error("invalid prev");
	}
	if(block->height != prev->height + 1) {
		throw std::logic_error("invalid height");
	}
	if(!block->proof) {
		throw std::logic_error("missing proof");
	}
	auto diff_block = find_prev_header(block, params->finality_delay);
	if(!diff_block) {
		diff_block = history.begin()->second;
	}
	if(block->vdf_iters != prev->vdf_iters + diff_block->time_diff * (1 + block->deficit)) {
		throw std::logic_error("invalid vdf_iters");
	}
	// TODO
}

void Node::verify(	std::shared_ptr<const ProofOfSpace> proof,
					const hash_t& challenge, const uint64_t difficulty, uint64_t& quality) const
{
	// TODO
	quality = 0;
}

void Node::verify(std::shared_ptr<const ProofOfTime> proof, const hash_t& begin) const
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
	if(!is_valid) {
		throw std::logic_error("invalid proof of time");
	}
}

void Node::apply(std::shared_ptr<const Block> block) noexcept
{
	if(block->prev != state_hash) {
		return;
	}
	const auto log = std::make_shared<DiffLog>();

	if(auto tx = block->tx_base) {
		apply(*log, tx);
	}
	for(auto tx : block->tx_list) {
		apply(*log, tx);
	}
	state_hash = block->hash;
	diff_log[block->hash] = log;
}

void Node::apply(DiffLog& log, std::shared_ptr<const Transaction> tx) noexcept
{
	// TODO
}

bool Node::revert() noexcept
{
	if(diff_log.empty()) {
		return false;
	}
	const auto log = diff_log.back();

	// TODO

	diff_log.pop_back();
	state_hash = log->prev_hash;
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
	for(size_t i = 0; block && i < distance; ++i) {
		block = find_block(block->prev);
	}
	return block;
}

std::shared_ptr<const BlockHeader> Node::find_prev_header(std::shared_ptr<const BlockHeader> block, const size_t distance) const
{
	for(size_t i = 0; block && i < distance; ++i) {
		block = find_header(block->prev);
	}
	return block;
}


} // mmx
