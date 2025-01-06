/*
 * Node_tests.cpp
 *
 *  Created on: Jan 5, 2025
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/ProofOfSpaceOG.hxx>

#include <algorithm>


namespace mmx {

std::shared_ptr<Block> Node::create_test_block(std::shared_ptr<const BlockHeader> prev, const bool valid)
{
	const skey_t farmer_sk(hash_t("test"));
	const pubkey_t farmer_key(farmer_sk);

	const auto proof = ProofOfSpaceOG::create();
	proof->ksize = 32;
	proof->score = 256;
	proof->farmer_key = farmer_key;
	proof->challenge = get_challenge(prev, 1, proof->difficulty);
	proof->proof_xs.resize(256);
	std::generate(proof->proof_xs.begin(), proof->proof_xs.end(),
		[]() { return vnx::rand64(); });

	proof_data_t proof_data;
	proof_data.hash = proof->calc_proof_hash();
	proof_data.proof = proof;

	proof_map[proof->challenge] = {proof_data};

	auto point = VDF_Point::create();
	point->vdf_height = prev->vdf_height + 1;
	point->start = prev->vdf_iters;
	point->input = prev->vdf_output;
	point->output = proof->challenge;
	point->prev = get_infusion(prev, 0, point->num_iters);
	point->content_hash = point->calc_hash();

	vdf_tree.emplace(point->output, point);
	vdf_index.emplace(point->start + point->num_iters, point);

	const auto prev_state = state_hash;
	try {
		auto out = vnx::clone(make_block(prev, {point}, proof->challenge));
		out->nonce = 1337;
		out->reward_addr = hash_t("test");
		if(!valid) {
			out->reward_vote = 2;
		}
		out->hash = out->calc_hash();
		out->farmer_sig = signature_t::sign(farmer_sk, out->hash);
		out->content_hash = out->calc_content_hash();
		fork_to(prev_state);
		return out;
	} catch(...) {
		fork_to(prev_state);
		throw;
	}
}

std::shared_ptr<Node::fork_t> Node::create_test_fork(std::shared_ptr<const BlockHeader> prev, const bool valid)
{
	auto fork = std::make_shared<fork_t>();
	fork->block = create_test_block(prev, valid);
	fork->is_vdf_verified = true;
	fork->is_proof_verified = true;
	return fork;
}

void Node::test_all()
{
	const auto old_peak = get_peak();
	const auto old_root = get_root();

	log(INFO) << "Running tests ...";
	log(INFO) << "Root is at height " << old_root->height << " hash " << old_root->hash;
	log(INFO) << "Peak is at height " << old_peak->height << " hash " << old_peak->hash;

	// normal forking
	{
		const int depth = 5;
		const int length = 10;

		auto block = old_peak;
		for(int i = 0; i < depth && block && block->height > root->height; ++i) {
			block = find_prev(block);
		}
		if(!block) {
			throw std::logic_error("cannot walk back from peak");
		}
		log(INFO) << "Starting new fork at height " << block->height << " hash " << block->hash;

		std::shared_ptr<fork_t> start;

		for(int i = 0; i < length; ++i) {
			auto fork = create_test_fork(block);
			if(!start) {
				start = fork;
			}
			add_fork(fork);
			block = fork->block;
		}
		log(INFO) << "Created new fork at peak height " << block->height << " hash " << block->hash;

		update();

		if(get_peak()->hash != block->hash) {
			throw std::logic_error("normal forking failed");
		}

		start->is_invalid = true;
		update();

		if(get_peak()->hash != old_peak->hash) {
			throw std::logic_error("failed to revert invalid fork");
		}
		log(INFO) << "(1/4) passed";
	}

	// failed normal forking
	{
		const int depth = 4;
		const int length = 6;

		auto block = old_peak;
		for(int i = 0; i < depth && block && block->height > root->height; ++i) {
			block = find_prev(block);
		}
		if(!block) {
			throw std::logic_error("cannot walk back from peak");
		}
		log(INFO) << "Starting new fork at height " << block->height << " hash " << block->hash;

		std::shared_ptr<fork_t> start;

		for(int i = 0; i < length; ++i) {
			auto fork = create_test_fork(block, i > 0);
			fork->is_validated = true;
			if(!start) {
				start = fork;
			}
			add_fork(fork);
			block = fork->block;
		}
		log(INFO) << "Created new fork at peak height " << block->height << " hash " << block->hash;

		start->is_validated = false;
		update();

		if(get_peak()->hash != old_peak->hash) {
			throw std::logic_error("normal forking did not fail");
		}
		log(INFO) << "(2/4) passed";
	}

	// deep forking
	{
		const int depth = params->commit_delay;
		const int length = 30;

		auto block = old_peak;
		for(int i = 0; i < depth && block && block->height > root->height; ++i) {
			block = find_prev(block);
		}
		if(!block) {
			throw std::logic_error("cannot walk back from peak");
		}
		log(INFO) << "Starting new fork at height " << block->height << " hash " << block->hash;

		for(int i = 0; i < length; ++i) {
			auto fork = create_test_fork(block);
			add_fork(fork);
			block = fork->block;
		}
		log(INFO) << "Created new fork at peak height " << block->height << " hash " << block->hash;

		update();

		if(get_peak()->hash != block->hash) {
			throw std::logic_error("advancing failed");
		}

		for(auto fork : get_fork_line()) {
			fork->is_invalid = true;
		}
		update();

		log(INFO) << "New peak is at height " << get_peak()->height << ", is_invalid = " << find_fork(state_hash)->is_invalid;

		if(get_peak()->hash != old_peak->hash) {
			log(INFO) << get_peak()->hash << " != " << old_peak->hash;
			throw std::logic_error("deep forking failed");
		}
		log(INFO) << "(3/4) passed";
	}

	// failed deep forking
}














} // mmx
