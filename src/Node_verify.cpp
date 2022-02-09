/*
 * Node_verify.cpp
 *
 *  Created on: Jan 20, 2022
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/chiapos.h>
#include <mmx/utils.h>

#include <vnx/vnx.h>


namespace mmx {

void Node::verify_proof(std::shared_ptr<fork_t> fork, const hash_t& vdf_challenge) const
{
	const auto block = fork->block;
	const auto prev = find_prev_header(block);
	const auto diff_block = fork->diff_block;
	if(!prev || !diff_block) {
		throw std::logic_error("cannot verify");
	}
	if(block->vdf_iters != prev->vdf_iters + diff_block->time_diff * params->time_diff_constant) {
		throw std::logic_error("invalid vdf_iters");
	}
	// Note: vdf_output already verified to match vdf_iters

	if(block->proof) {
		const auto challenge = get_challenge(block, vdf_challenge);
		fork->proof_score = verify_proof(block->proof, challenge, diff_block->space_diff);

		// check if block has a weak proof
		const auto iter = proof_map.find(challenge);
		if(iter != proof_map.end()) {
			if(fork->proof_score > iter->second->score) {
				fork->has_weak_proof = true;
				log(INFO) << "Got weak proof block for height " << block->height << " with score " << fork->proof_score << " > " << iter->second->score;
			}
		}
	} else {
		fork->proof_score = params->score_threshold;
	}

	fork->weight = 1;
	if(block->proof) {
		fork->weight += params->score_threshold - fork->proof_score;
	}
	fork->weight *= diff_block->space_diff;
	fork->weight *= diff_block->time_diff;

	fork->buffer_delta = 0;
	if(!block->proof || fork->has_weak_proof) {
		fork->buffer_delta -= params->score_threshold;
	} else {
		fork->buffer_delta += int32_t(2 * params->score_target) - fork->proof_score;
	}

	// check some VDFs during sync
	if(!fork->is_proof_verified && !fork->is_vdf_verified) {
		if(vnx::rand64() % vdf_check_divider) {
			fork->is_vdf_verified = true;
		}
	}
	fork->is_proof_verified = true;
}

uint32_t Node::verify_proof(std::shared_ptr<const ProofOfSpace> proof, const hash_t& challenge, const uint64_t space_diff) const
{
	if(proof->ksize < params->min_ksize) {
		throw std::logic_error("ksize too small");
	}
	if(proof->ksize > params->max_ksize) {
		throw std::logic_error("ksize too big");
	}
	const bls_pubkey_t plot_key = proof->local_key.to_bls() + proof->farmer_key.to_bls();

	const uint32_t port = 11337;
	if(hash_t(hash_t(proof->pool_key + plot_key) + bytes_t<4>(&port, 4)) != proof->plot_id) {
		throw std::logic_error("invalid proof keys or port");
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

void Node::verify_vdf(std::shared_ptr<const ProofOfTime> proof) const
{
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
			throw std::logic_error("too many segment iterations: " + std::to_string(seg.num_iters));
		}
	}

	// check proper infusions
	if(proof->start > 0) {
		if(!proof->infuse[0]) {
			throw std::logic_error("missing infusion on chain 0");
		}
		const auto infused_block = find_header(*proof->infuse[0]);
		if(!infused_block) {
			throw std::logic_error("unknown block infused on chain 0");
		}
		if(infused_block->height + std::min(params->finality_delay + 1, proof->height) != proof->height) {
			throw std::logic_error("invalid block height infused on chain 0");
		}
		const auto diff_block = find_diff_header(infused_block, params->finality_delay + 1);
		if(!diff_block) {
			throw std::logic_error("cannot verify");
		}
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
	}
	vdf_threads->add_task(std::bind(&Node::verify_vdf_task, this, proof));
}

void Node::verify_vdf(std::shared_ptr<const ProofOfTime> proof, const uint32_t chain) const
{
	const auto& segments = proof->segments;
	bool is_valid = !segments.empty();
	size_t invalid_segment = -1;

#pragma omp parallel for
	for(size_t i = 0; i < segments.size(); ++i)
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
	if(verified_vdfs.count(proof->height) == 0) {
		log(INFO) << "-------------------------------------------------------------------------------";
	}
	verified_vdfs.emplace(proof->height, point);
	vdf_verify_pending.erase(proof->height);

	const auto elapsed = (vnx::get_wall_time_micros() - point->recv_time) / 1e6;
	if(elapsed > params->block_time) {
		log(WARN) << "VDF verification took longer than block interval, unable to keep sync!";
	}
	else if(elapsed > 0.7 * params->block_time) {
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

	// add dummy blocks in case no proof is found
	{
		std::vector<std::shared_ptr<const BlockHeader>> prev_blocks;
		if(auto root = get_root()) {
			if(root->height + 1 == proof->height) {
				prev_blocks.push_back(root);
			}
		}
		for(auto iter = fork_index.lower_bound(proof->height - 1); iter != fork_index.upper_bound(proof->height - 1); ++iter) {
			prev_blocks.push_back(iter->second->block);
		}
		const auto infused_hash = proof->infuse[0];
		for(auto prev : prev_blocks) {
			if(infused_hash) {
				if(auto infused = find_prev_header(prev, params->finality_delay, true)) {
					if(infused->hash != *infused_hash) {
						continue;
					}
				}
			}
			auto block = Block::create();
			block->prev = prev->hash;
			block->height = proof->height;
			block->time_diff = prev->time_diff;
			block->space_diff = prev->space_diff;
			block->vdf_iters = point->vdf_iters;
			block->vdf_output = point->output;
			block->finalize();
			add_block(block);
		}
	}
	update();
}

void Node::verify_vdf_failed(std::shared_ptr<const ProofOfTime> proof)
{
	vdf_verify_pending.erase(proof->height);
	check_vdfs();
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
