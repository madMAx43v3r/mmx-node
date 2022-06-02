/*
 * Node_verify.cpp
 *
 *  Created on: Jan 20, 2022
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/ProofOfSpaceOG.hxx>
#include <mmx/ProofOfStake.hxx>
#include <mmx/contract/VirtualPlot.hxx>
#include <mmx/chiapos.h>
#include <mmx/utils.h>

#include <vnx/vnx.h>


namespace mmx {

bool Node::verify(std::shared_ptr<const ProofResponse> value)
{
	const auto request = value->request;
	const auto vdf_block = get_header_at(request->height - params->challenge_delay);
	if(!vdf_block) {
		log(DEBUG) << "Missing vdf_block for proof at height " << request->height << " with score " << value->proof->score;
		return false;
	}
	try {
		const auto diff_block = get_diff_header(vdf_block, params->challenge_delay);
		const auto challenge = hash_t(diff_block->hash + vdf_block->vdf_output[1]);
		if(request->challenge != challenge) {
			throw std::logic_error("invalid challenge");
		}
		if(request->space_diff != diff_block->space_diff) {
			throw std::logic_error("invalid space_diff");
		}
		verify_proof(value->proof, challenge, diff_block);

		if(value->proof->score >= params->score_threshold) {
			throw std::logic_error("invalid score");
		}
		auto iter = proof_map.find(challenge);
		if(iter == proof_map.end() || value->proof->score <= iter->second->proof->score)
		{
			if(iter == proof_map.end()) {
				challenge_map.emplace(request->height, challenge);
			}
			else if(value->proof->score < iter->second->proof->score) {
				proof_map.erase(challenge);
			}
			proof_map.emplace(challenge, value);

			log(DEBUG) << "Got new best proof for height " << request->height << " with score " << value->proof->score;
		}
		publish(value, output_verified_proof);
	}
	catch(const std::exception& ex) {
		log(WARN) << "Got invalid proof: " << ex.what();
	}
	return true;
}

void Node::verify_proof(std::shared_ptr<fork_t> fork, const hash_t& vdf_challenge) const
{
	const auto block = fork->block;
	const auto prev = find_prev_header(block);
	const auto diff_block = fork->diff_block;
	if(!prev || !diff_block) {
		throw std::logic_error("cannot verify");
	}
	// Note: block->is_valid() & block->validate() already called in pre-validate step

	if(block->vdf_iters != prev->vdf_iters + diff_block->time_diff * params->time_diff_constant) {
		throw std::logic_error("invalid vdf_iters");
	}
	// Note: vdf_output already verified to match vdf_iters

	if(auto proof = block->proof) {
		const auto challenge = get_challenge(block, vdf_challenge);
		verify_proof(proof, challenge, diff_block);
	}

	// check some VDFs during sync
	if(!fork->is_proof_verified && !fork->is_vdf_verified) {
		if(vnx::rand64() % vdf_check_divider) {
			fork->is_vdf_verified = true;
		}
	}
	fork->is_proof_verified = true;
}

uint64_t Node::get_virtual_plot_balance(const addr_t& plot_id, const vnx::optional<hash_t>& block_hash) const
{
	hash_t hash;
	if(block_hash) {
		hash = *block_hash;
	} else {
		if(auto peak = get_peak()) {
			hash = peak->hash;
		} else {
			return uint128_0;
		}
	}
	auto fork = find_fork(hash);
	auto block = fork ? fork->block : get_header(hash);
	if(!block) {
		throw std::logic_error("no such block");
	}
	const auto root = get_root();
	const auto since = block->height - std::min(params->virtual_lifetime, block->height);

	uint128_t balance = 0;
	for(const auto& entry : get_history({plot_id}, since)) {
		if(entry.contract == addr_t() && entry.height <= std::min(root->height, block->height)) {
			switch(entry.type) {
				case tx_type_e::REWARD:
				case tx_type_e::RECEIVE:
					balance += entry.amount; break;
				default: break;
			}
		}
	}
	while(fork) {
		const auto& block = fork->block;
		if(block->height <= root->height || block->height < since) {
			break;
		}
		for(const auto& tx : block->get_all_transactions()) {
			for(const auto& out : tx->get_all_outputs()) {
				if(out.address == plot_id && out.contract == addr_t()) {
					balance += out.amount;
				}
			}
		}
		fork = fork->prev.lock();
	}
	if(balance.upper()) {
		throw std::logic_error("balance overflow");
	}
	return balance;
}

void Node::verify_proof(	std::shared_ptr<const ProofOfSpace> proof, const hash_t& challenge,
							std::shared_ptr<const BlockHeader> diff_block) const
{
	if(!proof->is_valid()) {
		throw std::logic_error("invalid proof");
	}
	proof->validate();

	if(!check_plot_filter(params, challenge, proof->plot_id)) {
		throw std::logic_error("plot filter failed");
	}
	uint256_t score = uint256_max;

	if(auto og_proof = std::dynamic_pointer_cast<const ProofOfSpaceOG>(proof))
	{
		if(og_proof->ksize < params->min_ksize) {
			throw std::logic_error("ksize too small");
		}
		if(og_proof->ksize > params->max_ksize) {
			throw std::logic_error("ksize too big");
		}
		const auto quality = hash_t::from_bytes(chiapos::verify(
				og_proof->ksize, proof->plot_id.bytes, challenge.bytes, og_proof->proof_bytes.data(), og_proof->proof_bytes.size()));

		score = calc_proof_score(params, og_proof->ksize, quality, diff_block->space_diff);
	}
	else if(auto stake = std::dynamic_pointer_cast<const ProofOfStake>(proof))
	{
		const auto plot = std::dynamic_pointer_cast<const contract::VirtualPlot>(
				get_contract_at(stake->contract, diff_block->hash));
		if(!plot) {
			throw std::logic_error("no such virtual plot");
		}
		// TODO: check reward address if set
		if(stake->farmer_key != plot->farmer_key) {
			throw std::logic_error("invalid farmer key");
		}
		const auto balance = get_virtual_plot_balance(stake->contract, diff_block->hash);
		score = calc_virtual_score(params, challenge, stake->contract, balance, diff_block->space_diff);
	}
	else {
		throw std::logic_error("invalid proof type");
	}
	if(score != proof->score) {
		throw std::logic_error("proof score mismatch: expected " + std::to_string(proof->score) + " but got " + score.str(10));
	}
	if(score >= params->score_threshold) {
		throw std::logic_error("score >= score_threshold");
	}
}

void Node::verify_vdf(std::shared_ptr<const ProofOfTime> proof) const
{
	if(proof->version != 0) {
		throw std::logic_error("invalid version");
	}
	// check number of segments
	if(proof->segments.size() < params->min_vdf_segments) {
		throw std::logic_error("not enough segments: " + std::to_string(proof->segments.size()));
	}
	if(proof->segments.size() > params->max_vdf_segments) {
		throw std::logic_error("too many segments: " + std::to_string(proof->segments.size()));
	}
	const auto proof_iters = proof->get_num_iters();
	const auto avg_seg_iters = proof_iters / proof->segments.size();
	for(const auto& seg : proof->segments) {
		if(seg.num_iters > 2 * avg_seg_iters) {
			throw std::logic_error("too many segment iterations: " + std::to_string(seg.num_iters) + " > 2 * " + std::to_string(avg_seg_iters));
		}
	}

	// check proper infusions
	if(proof->height > params->infuse_delay)
	{
		if(!proof->infuse[0]) {
			throw std::logic_error("missing infusion on chain 0");
		}
		const auto infused_block = get_header(*proof->infuse[0]);
		if(!infused_block) {
			throw std::logic_error("unknown block infused on chain 0");
		}
		if(infused_block->height + std::min(params->infuse_delay + 1, proof->height) != proof->height) {
			throw std::logic_error("invalid block height infused on chain 0");
		}
		const auto diff_block = get_diff_header(infused_block, params->infuse_delay + 1);
		const auto expected_iters = diff_block->time_diff * params->time_diff_constant;
		if(proof_iters != expected_iters) {
			throw std::logic_error("wrong number of iterations: " + std::to_string(proof_iters) + " != " + std::to_string(expected_iters));
		}
		if(infused_block->height >= params->challenge_interval
			&& infused_block->height % params->challenge_interval == 0)
		{
			if(!proof->infuse[1]) {
				throw std::logic_error("missing infusion on chain 1");
			}
			if(*proof->infuse[1] != *proof->infuse[0]) {
				throw std::logic_error("invalid infusion on chain 1");
			}
		} else if(proof->infuse[1]) {
			throw std::logic_error("invalid infusion on chain 1");
		}
	} else {
		if(proof->infuse[0]) {
			throw std::logic_error("invalid infusion on chain 0");
		}
		if(proof->infuse[1]) {
			throw std::logic_error("invalid infusion on chain 0");
		}
	}
	vdf_threads->add_task(std::bind(&Node::verify_vdf_task, this, proof));
}

void Node::verify_vdf(std::shared_ptr<const ProofOfTime> proof, const uint32_t chain) const
{
	const auto& segments = proof->segments;
	bool is_valid = !segments.empty();
	size_t invalid_segment = -1;

#pragma omp parallel for
	for(int i = 0; i < int(segments.size()); ++i)
	{
		if(!is_valid) {
			continue;
		}
		hash_t point;
		if(i > 0) {
			point = segments[i-1].output[chain];
		} else {
			point = proof->input[chain];
			if(auto infuse = proof->infuse[chain]) {
				point = hash_t(point + *infuse);
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

void Node::verify_vdf_success(std::shared_ptr<const ProofOfTime> proof, std::shared_ptr<vdf_point_t> point)
{
	if(is_synced && !verified_vdfs.count(proof->height)) {
		log(INFO) << "-------------------------------------------------------------------------------";
	}
	verified_vdfs.emplace(proof->height, point);
	vdf_verify_pending.erase(proof->height);

	const auto elapsed = (vnx::get_wall_time_micros() - point->recv_time) / 1e6;
	if(elapsed > params->block_time) {
		log(WARN) << "VDF verification took longer than block interval, unable to keep sync!";
	}
	else if(elapsed > 0.5 * params->block_time) {
		log(WARN) << "VDF verification took longer than recommended: " << elapsed << " sec";
	}

	std::shared_ptr<const vdf_point_t> prev;
	for(auto iter = verified_vdfs.lower_bound(proof->height - 1); iter != verified_vdfs.lower_bound(proof->height); ++iter) {
		if(iter->second->output == point->input) {
			prev = iter->second;
		}
	}
	log(INFO) << "Verified VDF for height " << proof->height <<
			(prev ? ", delta = " + std::to_string((point->recv_time - prev->recv_time) / 1e6) + " sec" : "") << ", took " << elapsed << " sec";

	// add dummy blocks
	const auto root = get_root();
	if(proof->height == root->height + 1) {
		add_dummy_blocks(root);
	}
	const auto range = fork_index.equal_range(proof->height - 1);
	for(auto iter = range.first; iter != range.second; ++iter) {
		add_dummy_blocks(iter->second->block);
	}
	add_task(std::bind(&Node::update, this));
}

void Node::verify_vdf_failed(std::shared_ptr<const ProofOfTime> proof)
{
	vdf_verify_pending.erase(proof->height);
	verify_vdfs();
}

void Node::verify_vdf_task(std::shared_ptr<const ProofOfTime> proof) const noexcept
{
	std::lock_guard lock(vdf_mutex);

	const auto time_begin = vnx::get_wall_time_micros();
	try {
		for(int i = 0; i < 2; ++i) {
			if(auto engine = opencl_vdf[i]) {
				engine->compute(proof, i);
			}
		}
		for(int i = 0; i < 2; ++i) {
			if(auto engine = opencl_vdf[i]) {
				engine->verify(proof, i);
			} else {
				verify_vdf(proof, i);
			}
		}
		auto point = std::make_shared<vdf_point_t>();
		point->height = proof->height;
		point->vdf_start = proof->start;
		point->vdf_iters = proof->get_vdf_iters();
		point->input = proof->input;
		point->output[0] = proof->get_output(0);
		point->output[1] = proof->get_output(1);
		point->infused = proof->infuse[0];
		point->proof = proof;
		// TODO: use actual receive time
		point->recv_time = time_begin;

		add_task([this, proof, point]() {
			((Node*)this)->verify_vdf_success(proof, point);
		});
	}
	catch(const std::exception& ex) {
		add_task([this, proof]() {
			((Node*)this)->verify_vdf_failed(proof);
		});
		log(WARN) << "VDF verification for height " << proof->height << " failed with: " << ex.what();
	}
}

void Node::check_vdf_task(std::shared_ptr<fork_t> fork, std::shared_ptr<const BlockHeader> prev, std::shared_ptr<const BlockHeader> infuse) const noexcept
{
	// MAY NOT ACCESS ANY NODE DATA EXCEPT "params"
	const auto time_begin = vnx::get_wall_time_micros();
	const auto& block = fork->block;

	auto point = prev->vdf_output;
	point[0] = hash_t(point[0] + infuse->hash);

	if(infuse->height >= params->challenge_interval && infuse->height % params->challenge_interval == 0) {
		point[1] = hash_t(point[1] + infuse->hash);
	}
	const auto num_iters = block->vdf_iters - prev->vdf_iters;

	for(uint64_t i = 0; i < num_iters; ++i) {
		for(int chain = 0; chain < 2; ++chain) {
			point[chain] = hash_t(point[chain].bytes);
		}
	}
	if(point == block->vdf_output) {
		fork->is_vdf_verified = true;
		const auto elapsed = (vnx::get_wall_time_micros() - time_begin) / 1e6;
		log(INFO) << "VDF check for height " << block->height << " passed, took " << elapsed << " sec";
	} else {
		fork->is_invalid = true;
		log(WARN) << "VDF check for height " << block->height << " failed!";
	}
}


} // mmx
