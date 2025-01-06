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

std::shared_ptr<Block> Node::create_test_block(std::shared_ptr<const BlockHeader> prev)
{
	const skey_t farmer_sk(hash_t("test"));
	const pubkey_t farmer_key(farmer_sk);

	const auto proof = ProofOfSpaceOG::create();
	proof->ksize = 32;
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

std::shared_ptr<Node::fork_t> Node::create_test_fork(std::shared_ptr<const BlockHeader> prev)
{
	auto fork = std::make_shared<fork_t>();
	fork->block = create_test_block(prev);
	fork->is_vdf_verified = true;
	fork->is_proof_verified = true;
	fork->is_validated = true;
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
			log(INFO) << "Created new block at height " << fork->block->height << " hash " << fork->block->hash;
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
		log(INFO) << "(1/1) passed";
	}

	// failed normal forking

	// deep forking

	// failed deep forking

	// running ahead with longer chain then stopping to make it switch back to main
}



} // mmx
