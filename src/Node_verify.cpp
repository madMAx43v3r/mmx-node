/*
 * Node_verify.cpp
 *
 *  Created on: Jan 20, 2022
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/ProofOfSpaceOG.hxx>
#include <mmx/ProofOfSpaceNFT.hxx>
#include <mmx/pos/verify.h>
#include <mmx/utils.h>

#include <vnx/vnx.h>
#include <sha256_avx2.h>
#include <sha256_ni.h>
#include <sha256_arm.h>


namespace mmx {

void Node::add_proof(std::shared_ptr<const ProofOfSpace> proof, const uint32_t vdf_height, const vnx::Hash64 farmer_mac)
{
	auto& list = proof_map[proof->challenge];

	const auto hash = proof->calc_proof_hash();
	for(const auto& entry : list) {
		if(hash == entry.hash) {
			return;		// prevent replay attack
		}
	}
	if(list.empty()) {
		challenge_map.emplace(vdf_height, proof->challenge);
	}
	proof_data_t data;
	data.hash = hash;
	data.proof = proof;
	data.farmer_mac = farmer_mac;
	list.push_back(data);

	std::sort(list.begin(), list.end(),
		[](const proof_data_t& L, const proof_data_t& R) -> bool {
			return L.hash < R.hash;
		});
}

bool Node::verify(std::shared_ptr<const ProofResponse> value)
{
	if(!value->is_valid()) {
		throw std::logic_error("invalid response");
	}
	if(auto root = get_root()) {
		if(value->vdf_height <= root->vdf_height) {
			throw std::logic_error("proof too old: vdf_height = " + std::to_string(value->vdf_height));
		}
	}
	hash_t challenge;
	uint64_t space_diff = 0;
	if(!find_challenge(value->vdf_height, challenge, space_diff)) {
		return false;
	}
	verify_proof(value->proof, challenge, space_diff);

	add_proof(value->proof, value->vdf_height, value->farmer_addr);

	publish(value, output_verified_proof);
	return true;
}

void Node::verify_proof(std::shared_ptr<fork_t> fork) const
{
	const auto block = fork->block;
	const auto prev = find_prev_header(block);
	if(!prev) {
		throw std::logic_error("cannot verify");
	}
	if(!block->proof) {
		throw std::logic_error("missing proof");
	}
	if(block->vdf_count > params->max_vdf_count) {
		throw std::logic_error("invalid vdf_count");
	}

	uint64_t expected_iters = prev->vdf_iters;
	for(uint32_t i = 0; i < block->vdf_count; ++i) {
		uint64_t num_iters = 0;
		get_infusion(prev, i, num_iters);
		expected_iters += num_iters;
	}
	if(block->vdf_iters != expected_iters) {
		throw std::logic_error("invalid vdf_iters: " + std::to_string(block->vdf_iters) + " != " + std::to_string(expected_iters));
	}

	const auto weight = calc_block_weight(params, block, prev);
	const auto total_weight = prev->total_weight + block->weight;
	if(block->weight != weight) {
		throw std::logic_error("invalid block weight: " + block->weight.str(10) + " != " + weight.str(10));
	}
	if(block->total_weight != total_weight) {
		throw std::logic_error("invalid total weight: " + block->total_weight.str(10) + " != " + total_weight.str(10));
	}

	uint64_t space_diff = 0;
	const auto challenge = get_challenge(block, 0, space_diff);

	verify_proof(block->proof, challenge, space_diff);

	fork->is_proof_verified = true;
}

template<typename T>
void Node::verify_proof_impl(
		std::shared_ptr<const T> proof, const hash_t& challenge, const uint64_t space_diff) const
{
	if(proof->ksize < params->min_ksize) {
		throw std::logic_error("ksize too low");
	}
	if(proof->ksize > params->max_ksize) {
		throw std::logic_error("ksize too high");
	}
	const auto plot_challenge = get_plot_challenge(challenge, proof->plot_id);

	const auto quality = pos::verify(proof->proof_xs, plot_challenge, proof->plot_id, params->plot_filter, proof->ksize);

	if(!check_proof_threshold(params, proof->ksize, quality, space_diff)) {
		throw std::logic_error("not good enough");
	}
}

void Node::verify_proof(std::shared_ptr<const ProofOfSpace> proof, const hash_t& challenge, const uint64_t space_diff) const
{
	if(space_diff <= 0) {
		throw std::logic_error("difficulty zero");
	}
	if(!proof || !proof->is_valid()) {
		throw std::logic_error("invalid proof");
	}
	if(proof->challenge != challenge) {
		throw std::logic_error("invalid challenge");
	}
	if(proof->difficulty != space_diff) {
		throw std::logic_error("invalid difficulty");
	}
	proof->validate();

	if(!check_plot_filter(params, challenge, proof->plot_id)) {
		throw std::logic_error("plot filter failed");
	}
	const auto og_proof = std::dynamic_pointer_cast<const ProofOfSpaceOG>(proof);
	const auto nft_proof = std::dynamic_pointer_cast<const ProofOfSpaceNFT>(proof);

	if(og_proof) {
		verify_proof_impl(og_proof, challenge, space_diff);
	} else if(nft_proof) {
		verify_proof_impl(nft_proof, challenge, space_diff);
	} else {
		throw std::logic_error("invalid proof type: " + proof->get_type_name());
	}
	const auto score = get_proof_score(proof->calc_proof_hash());
	if(score != proof->score) {
		throw std::logic_error("proof score mismatch: expected " + std::to_string(proof->score) + " but got " + std::to_string(score));
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
	const uint32_t num_iters = proof->segment_size;
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
				point[j] = segments[i - 1];
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
			// Note: `num_lanes` is always a multiple of 2 (based on chain params)
			for(uint32_t j = 0; j < num_lanes; j += 2)
			{
				uint8_t hashx2[32 * 2];
				::memcpy(hashx2, point[j].data(), 32);
				::memcpy(hashx2 + 32, point[j + 1].data(), 32);
				if(have_sha_ni) {
					recursive_sha256_ni_x2(hashx2, num_iters);
				} else if(have_sha_arm) {
					recursive_sha256_arm_x2(hashx2, num_iters);
				} else {
					throw std::logic_error("invalid feature state");
				}
				::memcpy(point[j].data(), hashx2, 32);
				::memcpy(point[j + 1].data(), hashx2 + 32, 32);
			}
		} else {
			for(uint32_t k = 0; k < num_iters; ++k)
			{
				for(uint32_t j = 0; j < num_lanes; ++j) {
					::memcpy(input[j], point[j].data(), 32);
				}
				sha256_64_x8(hash[0], input[0], 32);
				sha256_64_x8(hash[8], input[8], 32);

				for(uint32_t j = 0; j < num_lanes; ++j) {
					::memcpy(point[j].data(), hash[j], 32);
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
	const auto took_ms = vnx::get_wall_time_millis() - point->recv_time;

	if(point->input == get_vdf_peak()) {
		if(is_synced) {
			log(INFO) << "-------------------------------------------------------------------------------";
		}
		publish(point->proof, output_verified_vdfs);
	}
	const auto vdf_iters = point->start + point->num_iters;

	vdf_tree.emplace(point->output, point);
	vdf_index.emplace(vdf_iters, point);
	vdf_verify_pending.erase(point->proof->hash);

	publish(point, output_vdf_points);

	if(took_ms > params->block_interval_ms) {
		log(WARN) << "VDF verification took longer than block interval, unable to keep sync!";
	} else if(took_ms > params->block_interval_ms / 2) {
		log(WARN) << "VDF verification took longer than recommended: " << took_ms / 1e3 << " sec";
	}

	// check if this proof helps advancing the chain
	if(auto peak = get_peak()) {
		auto vdf_height = point->vdf_height;
		if(vdf_height > peak->vdf_height && vdf_height - peak->vdf_height <= params->max_vdf_count) {
			auto input = point->input;
			while(vdf_height > peak->vdf_height) {
				if(input == peak->vdf_output) {
					stuck_timer->reset();	// if so, make sure we keep sync status
					break;
				}
				const auto iter = vdf_tree.find(input);
				if(iter != vdf_tree.end()) {
					input = iter->second->input;
					vdf_height--;
				} else {
					break;
				}
			}
		}
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
			<< height << " + " << index << " (" << point->vdf_height << ")"
			<< ss_delta.str() << ", took " << took_ms / 1e3 << " sec";

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
		point->vdf_height = proof->vdf_height;
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
	const auto prev = find_prev_header(block);
	if(!prev) {
		throw std::logic_error("cannot check VDF");
	}
	uint64_t vdf_iters = prev->vdf_iters;
	std::map<uint64_t, vnx::optional<hash_t>> infuse_map;
	for(uint32_t i = 0; i < block->vdf_count; ++i) {
		uint64_t num_iters = 0;
		infuse_map[vdf_iters] = get_infusion(prev, i, num_iters);
		vdf_iters += num_iters;
	}
	if(vdf_iters != block->vdf_iters) {
		throw std::logic_error("invalid vdf_iters");
	}
	log(INFO) << "Checking VDF for height " << block->height << " ...";

	vdf_threads->add_task(std::bind(&Node::check_vdf_task, this, fork, prev, infuse_map));
}

void Node::check_vdf_task(
		std::shared_ptr<fork_t> fork,
		std::shared_ptr<const BlockHeader> prev,
		const std::map<uint64_t, vnx::optional<hash_t>>& infuse) noexcept
{
	static bool have_sha_ni = sha256_ni_available();

	// MAY NOT ACCESS OR MODIFY ANY NON-CONSTNAT NODE DATA
	const auto time_begin = vnx::get_wall_time_millis();
	const auto& block = fork->block;

	auto next = infuse.begin();
	auto point = prev->vdf_output;
	uint64_t vdf_iters = prev->vdf_iters;

	while(vdf_iters < block->vdf_iters)
	{
		if(next != infuse.end() && next->first == vdf_iters) {
			if(auto prev = next->second) {
				point = hash_t(point + *prev);
			}
			if(auto addr = block->vdf_reward_addr) {
				point = hash_t(point + *addr);
			}
			next++;
		}
		const auto num_iters = (next != infuse.end() ? next->first : block->vdf_iters) - vdf_iters;
		if(have_sha_ni) {
			recursive_sha256_ni(point.bytes.data(), num_iters);
		} else {
			for(uint64_t i = 0; i < num_iters; ++i) {
				point = hash_t(point.bytes);
			}
		}
		vdf_iters += num_iters;
	}
	const bool is_valid = (point == block->vdf_output);
	const auto elapsed = (vnx::get_wall_time_millis() - time_begin) / 1e3;

	if(is_valid) {
		log(INFO) << "VDF check for height " << block->height << " passed, took " << elapsed << " sec";
	} else {
		log(WARN) << "VDF check for height " << block->height << " failed, took " << elapsed << " sec";
	}

	add_task([this, fork, is_valid]() {
		if(is_valid) {
			fork->is_vdf_verified = true;
		} else {
			fork->is_invalid = true;
		}
		trigger_update();
	});
}


} // mmx
