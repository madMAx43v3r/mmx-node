/*
 * Node_verify.cpp
 *
 *  Created on: Jan 20, 2022
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/ProofOfSpaceOG.hxx>
#include <mmx/ProofOfSpaceNFT.hxx>
#include <mmx/ProofOfStake.hxx>
#include <mmx/contract/VirtualPlot.hxx>
#include <mmx/pos/verify.h>
#include <mmx/utils.h>

#include <vnx/vnx.h>
#include <sha256_avx2.h>
#include <sha256_ni.h>
#include <sha256_arm.h>


namespace mmx {

void Node::add_proof(	const uint32_t height, const hash_t& challenge,
						std::shared_ptr<const ProofOfSpace> proof, const vnx::Hash64 farmer_mac)
{
	auto& list = proof_map[challenge];

	const auto hash = proof->calc_hash();
	for(const auto& entry : list) {
		if(hash == entry.hash) {
			return;
		}
	}
	if(list.empty()) {
		challenge_map.emplace(height, challenge);
	}
	proof_data_t data;
	data.hash = hash;
	data.height = height;
	data.proof = proof;
	data.farmer_mac = farmer_mac;
	list.push_back(data);

	std::sort(list.begin(), list.end(),
		[](const proof_data_t& L, const proof_data_t& R) -> bool {
			return L.proof->score < R.proof->score;
		});

	if(list.size() > max_blocks_per_height) {
		list.resize(max_blocks_per_height);
	}
}

bool Node::verify(std::shared_ptr<const ProofResponse> value)
{
	if(!value->is_valid()) {
		throw std::logic_error("invalid response");
	}
	const auto request = value->request;
	const auto block = get_header_at(request->height - params->challenge_delay);
	if(!block) {
		return false;
	}
	const auto diff_block = get_diff_header(block, params->challenge_delay);

	const auto& challenge = block->challenge;
	if(request->challenge != challenge) {
		throw std::logic_error("invalid challenge");
	}
	if(request->space_diff != diff_block->space_diff) {
		throw std::logic_error("invalid space_diff");
	}
	verify_proof(value->proof, challenge, diff_block->space_diff);

	add_proof(request->height, challenge, value->proof, value->farmer_addr);

	publish(value, output_verified_proof);
	return true;
}

void Node::verify_proof(std::shared_ptr<fork_t> fork, const hash_t& challenge) const
{
	const auto block = fork->block;
	const auto prev = find_prev_header(block);
	const auto diff_block = fork->diff_block;
	if(!prev || !diff_block) {
		throw std::logic_error("cannot verify");
	}
	if(!block->proof) {
		throw std::logic_error("missing proof");
	}
	const auto expected_iters = prev->vdf_iters + block->vdf_count * diff_block->time_diff * params->time_diff_constant;
	if(block->vdf_iters != expected_iters) {
		throw std::logic_error("invalid vdf_iters: " + std::to_string(block->vdf_iters) + " != " + std::to_string(expected_iters));
	}

	for(uint32_t i = 0; i < fork->vdf_points.size(); ++i) {
		const auto& point = fork->vdf_points[i];
		if(point->reward_addr != block->vdf_reward_addr) {
			throw std::logic_error("invalid VDF reward_addr");
		}
		if(point->prev != get_infusion(prev, i)) {
			throw std::logic_error("invalid VDF infusion");
		}
	}
	verify_proof(block->proof, challenge, diff_block->space_diff);

	fork->is_proof_verified = true;
}

template<typename T>
uint256_t Node::verify_proof_impl(
		std::shared_ptr<const T> proof, const hash_t& challenge, const uint64_t space_diff) const
{
	if(proof->ksize < params->min_ksize) {
		throw std::logic_error("ksize too small");
	}
	if(proof->ksize > params->max_ksize) {
		throw std::logic_error("ksize too big");
	}
	const auto plot_challenge = get_plot_challenge(challenge, proof->plot_id);

	const auto quality = pos::verify(proof->proof_xs, plot_challenge, proof->plot_id, params->plot_filter, proof->ksize);

	return calc_proof_score(params, proof->ksize, quality, space_diff);
}

void Node::verify_proof(std::shared_ptr<const ProofOfSpace> proof, const hash_t& challenge, const uint64_t space_diff) const
{
	if(!proof->is_valid()) {
		throw std::logic_error("invalid proof");
	}
	if(proof->challenge != challenge) {
		throw std::logic_error("invalid challenge");
	}
	proof->validate();

	const auto og_proof = std::dynamic_pointer_cast<const ProofOfSpaceOG>(proof);
	const auto nft_proof = std::dynamic_pointer_cast<const ProofOfSpaceNFT>(proof);

	if(space_diff <= 0) {
		throw std::logic_error("invalid space difficulty");
	}
	if(!check_plot_filter(params, challenge, proof->plot_id)) {
		throw std::logic_error("plot filter failed");
	}
	uint256_t score = uint256_max;

	if(og_proof) {
		score = verify_proof_impl(og_proof, challenge, space_diff);
	} else if(nft_proof) {
		score = verify_proof_impl(nft_proof, challenge, space_diff);
	} else {
		throw std::logic_error("invalid proof type: " + proof->get_type_name());
	}
	if(score != proof->score) {
		throw std::logic_error("proof score mismatch: expected " + std::to_string(proof->score) + " but got " + score.str(10));
	}
	if(score >= params->score_threshold) {
		throw std::logic_error("proof score >= score_threshold");
	}
}

void Node::verify_vdf(std::shared_ptr<const ProofOfTime> proof, const uint32_t height, const uint32_t index)
{
	if(!proof->is_valid()) {
		throw std::logic_error("static validation failed");
	}
	proof->validate();

	// Note: `prev` + `num_iters` already verified in `verify_vdfs()`

	if(proof->num_iters % params->time_diff_constant) {
		throw std::logic_error("invalid num_iters");
	}
	if(proof->segment_size != params->vdf_segment_size) {
		throw std::logic_error("invalid segment size");
	}
	if(proof->segments.size() != proof->num_iters / params->vdf_segment_size) {
		throw std::logic_error("invalid segment count");
	}
	vdf_threads->add_task(std::bind(&Node::verify_vdf_task, this, proof, height, index));
	vdf_verify_pending.insert(proof->hash);
}

void Node::verify_vdf_cpu(std::shared_ptr<const ProofOfTime> proof) const
{
	static bool have_sha_ni = sha256_ni_available();
	static bool have_sha_arm = sha256_arm_available();

	const auto& segments = proof->segments;
	bool is_valid = !segments.empty();
	size_t invalid_segment = -1;

	constexpr uint32_t batch_size = 16;
	const uint32_t num_chunks = (segments.size() + batch_size - 1) / batch_size;

#pragma omp parallel for
	for(int chunk = 0; chunk < int(num_chunks); ++chunk)
	{
		if(!is_valid) {
			continue;
		}
		const auto num_lanes = std::min<uint32_t>(batch_size, segments.size() - chunk * batch_size);

		hash_t point[batch_size];
		uint8_t hash[batch_size][32];
		uint8_t input[batch_size][64];

		for(uint32_t j = 0; j < num_lanes; ++j)
		{
			const uint32_t i = chunk * batch_size + j;
			if(i > 0) {
				point[j] = segments[i-1];
			} else {
				point[j] = proof->input;
				if(proof->prev) {
					point[j] = hash_t(point[j] + (*proof->prev));
				}
				if(proof->reward_addr) {
					point[j] = hash_t(point[j] + (*proof->reward_addr));
				}
			}
		}
		if(have_sha_ni || have_sha_arm) {
			for(uint32_t j = 0; j < num_lanes; j += 2)
			{
				const uint32_t i = chunk * batch_size + j;
				const uint32_t k = (j + 1 < num_lanes) ? 1 : 0;
				const auto num_iters_0 = segments[i].num_iters;
				const auto num_iters_1 = segments[i + k].num_iters;
				const auto max_iters = std::max(num_iters_0, num_iters_1);
				const auto min_iters = std::min(num_iters_0, num_iters_1);
				uint8_t hashx2[32 * 2];

				::memcpy(hashx2, point[j].data(), 32);
				::memcpy(hashx2 + 32, point[j + k].data(), 32);
				if(have_sha_ni) {
					recursive_sha256_ni_x2(hashx2, min_iters);
					if(num_iters_0 != num_iters_1) {
						recursive_sha256_ni(hashx2 + ((num_iters_0 > num_iters_1) ? 0 : 32), max_iters - min_iters);
					}
				} else if(have_sha_arm) {
					recursive_sha256_arm_x2(hashx2, min_iters);
					if(num_iters_0 != num_iters_1) {
						recursive_sha256_arm(hashx2 + ((num_iters_0 > num_iters_1) ? 0 : 32), max_iters - min_iters);
					}
				} else {
					throw std::logic_error("invalid feature state");
				}
				::memcpy(point[j].data(), hashx2, 32);
				::memcpy(point[j + k].data(), hashx2 + 32, 32);
			}
		} else {
			for(uint32_t k = 0; k < max_iters; ++k)
			{
				for(uint32_t j = 0; j < num_lanes; ++j) {
					::memcpy(input[j], point[j].data(), 32);
				}
				sha256_64_x8(hash[0], input[0], 32);
				sha256_64_x8(hash[8], input[8], 32);

				for(uint32_t j = 0; j < num_lanes; ++j) {
					const uint32_t i = chunk * batch_size + j;
					if(k < segments[i].num_iters) {
						::memcpy(point[j].data(), hash[j], 32);
					}
				}
			}
		}
		for(uint32_t j = 0; j < num_lanes; ++j)
		{
			const uint32_t i = chunk * batch_size + j;
			if(point[j] != segments[i]) {
				is_valid = false;
				invalid_segment = i;
			}
		}
	}
	if(!is_valid) {
		throw std::logic_error("invalid output on segment " + std::to_string(invalid_segment));
	}
}

void Node::verify_vdf_success(std::shared_ptr<const VDF_Point> point, const uint32_t height, const uint32_t index)
{
	if(point->input == get_vdf_peak()) {
		if(is_synced) {
			log(INFO) << "-------------------------------------------------------------------------------";
		}
		publish(point->proof, output_verified_vdfs);
	}
	const auto vdf_iters = point->start + point->num_iters;

	vdf_tree[point->output] = point;
	vdf_index.emplace(vdf_iters, point);
	vdf_verify_pending.erase(point->proof->hash);

	publish(point, output_vdf_points);

	const auto elapsed = (vnx::get_wall_time_micros() - point->recv_time) / 1000;
	if(elapsed > params->block_interval_ms) {
		log(WARN) << "VDF verification took longer than block interval, unable to keep sync!";
	}
	else if(elapsed > params->block_interval_ms / 2) {
		log(WARN) << "VDF verification took longer than recommended: " << elapsed / 1e3 << " sec";
	}

	std::shared_ptr<const VDF_Point> prev;
	{
		const auto iter = vdf_tree.find(point->input);
		if(iter != vdf_tree.end()) {
			prev = iter->second;
		}
	}
	std::stringstream ss_delta;
	if(prev) {
		ss_delta << ", delta = " << (point->recv_time - prev->recv_time) / 1000 / 1e3 << " sec" ;
	}
	const char* clocks[] = {
			u8"\U0001F550", u8"\U0001F551", u8"\U0001F552", u8"\U0001F553", u8"\U0001F554", u8"\U0001F555",
			u8"\U0001F556", u8"\U0001F557", u8"\U0001F558", u8"\U0001F559", u8"\U0001F55A", u8"\U0001F55B" };

	log(INFO) << clocks[(height + index) % 12] << " Verified VDF for height "
			<< height << " + " << index << ss_delta.str() << ", took " << elapsed / 1e3 << " sec";

	trigger_update();
}

void Node::verify_vdf_task(std::shared_ptr<const ProofOfTime> proof, const uint32_t height, const uint32_t index) noexcept
{
	std::shared_ptr<OCL_VDF> engine;
	try {
		const auto time_begin = vnx::get_wall_time_millis();

		if(opencl_vdf_enable) {
			std::unique_lock lock(vdf_mutex);
			while(opencl_vdf.empty()) {
				vdf_signal.wait(lock);
			}
			engine = opencl_vdf.back();
			opencl_vdf.pop_back();
		}

		if(engine) {
			engine->compute(proof);
			engine->verify(proof);
		} else {
			verify_vdf_cpu(proof);
		}
		auto point = VDF_Point::create();
		point->start = proof->start;
		point->num_iters = proof->num_iters;
		point->input = proof->input;
		point->output = proof->get_output();
		point->prev = proof->prev;
		point->reward_addr = proof->reward_addr;
		point->recv_time = time_begin;
		point->proof = proof;
		point->content_hash = point->calc_hash();

		add_task(std::bind(&Node::verify_vdf_success, this, point, height, index));
	}
	catch(const std::exception& ex) {
		add_task([this, proof]() {
			vdf_verify_pending.erase(proof->hash);
			verify_vdfs();
		});
		log(WARN) << "VDF verification for " << height << " + " << index << " failed with: " << ex.what();
	}

	if(engine) {
		std::unique_lock lock(vdf_mutex);
		opencl_vdf.push_back(engine);
	}
	vdf_signal.notify_all();
}

void Node::check_vdf(std::shared_ptr<fork_t> fork)
{
	const auto& block = fork->block;
	// TODO
	if(auto prev = find_prev_header(block)) {
		if(auto infuse = find_prev_header(block, params->infuse_delay + 1)) {
			log(INFO) << "Checking VDF for block at height " << block->height << " ...";
			vdf_threads->add_task(std::bind(&Node::check_vdf_task, this, fork, prev, infuse));
		} else {
			throw std::logic_error("cannot check VDF");
		}
	} else {
		throw std::logic_error("cannot check VDF");
	}
}

void Node::check_vdf_task(std::shared_ptr<fork_t> fork, std::shared_ptr<const BlockHeader> prev, std::shared_ptr<const BlockHeader> infuse) noexcept
{
	static bool have_sha_ni = sha256_ni_available();

	// MAY NOT ACCESS ANY NODE DATA EXCEPT "params"
	const auto time_begin = vnx::get_wall_time_millis();
	const auto& block = fork->block;

	auto point = prev->vdf_output;
	if(infuse) {
		point = hash_t(point + infuse->hash);
	}
	const auto num_iters = block->vdf_iters - prev->vdf_iters;

	if(have_sha_ni) {
		recursive_sha256_ni(point.bytes.data(), num_iters);
	} else {
		for(uint64_t i = 0; i < num_iters; ++i) {
			point = hash_t(point.bytes);
		}
	}

	if(point == block->vdf_output) {
		fork->is_vdf_verified = true;
		const auto elapsed = (vnx::get_wall_time_millis() - time_begin) / 1e3;
		log(INFO) << "VDF check for height " << block->height << " passed, took " << elapsed << " sec";
	} else {
		fork->is_invalid = true;
		add_task(std::bind(&Node::trigger_update, this));
		log(WARN) << "VDF check for height " << block->height << " failed!";
	}
}


} // mmx
