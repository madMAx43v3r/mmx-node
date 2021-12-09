/*
 * Node.cpp
 *
 *  Created on: Dec 7, 2021
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/chiapos.h>

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
		verify(proof, iter->second);
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
		if(auto prev = find_prev_header(block, params->challenge_delay, true))
		{
			auto iter2 = verified_vdfs.find(prev->vdf_iters + block->deficit * prev->time_diff);
			if(iter2 != verified_vdfs.end()) {
				vdf_challenge = iter2->second;
			} else {
				iter++;
				continue;
			}
		}
		try {
			fork->proof_score = verify_proof(block, vdf_challenge);
		}
		catch(const std::exception& ex) {
			log(WARN) << "Proof verification failed for a block at height " << block->height << " with: " << ex.what();
			iter = fork_tree.erase(iter);
			continue;
		}
		iter++;
	}
	bool did_fork = false;
	bool did_update = false;
	std::shared_ptr<const BlockHeader> forked_at;

	// choose best fork
	while(true) {
		const auto root = get_root();

		// purge disconnected forks
		purge_tree();

		uint64_t max_weight = 0;
		std::shared_ptr<fork_t> best_fork;

		// find best fork
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

		// bring state back to the new fork
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
					verify(block);
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
		if(!is_valid) {
			// try again
			continue;
		}
		did_update = true;

		// commit to history
		if(fork_line.size() > params->finality_delay) {
			commit(fork_line.front()->block);
		}
		break;
	}

	if(did_update) {
		if(auto peak = find_header(state_hash)) {
			log(INFO) << "New peak at height " << peak->height
					<< (did_fork && forked_at ? " (forked at " + std::to_string(forked_at->height) + ")" : "");
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

void Node::verify(std::shared_ptr<const Block> block) const
{
	auto prev = find_prev_block(block);
	if(!prev) {
		throw std::logic_error("invalid prev");
	}
	if(state_hash != prev->hash) {
		throw std::logic_error("state mismatch");
	}
	if(block->height != prev->height + 1) {
		throw std::logic_error("invalid height");
	}
	// TODO
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
		// TODO: check plot filter

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

uint128_t Node::calc_proof_score(const uint8_t ksize, const hash_t& quality, const uint64_t difficulty) const
{
	uint128_t modulo = uint128_t(difficulty) * params->space_diff_constant;
	modulo /= (2 * ksize) + 1;
	modulo >>= ksize - 1;
	return quality.to_uint256() % modulo;
}


} // mmx
