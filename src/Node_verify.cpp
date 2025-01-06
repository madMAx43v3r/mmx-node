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
	if(vnx::get_pipe(farmer_mac)) {
		farmer_keys[proof->farmer_key] = farmer_mac;
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

void Node::verify(std::shared_ptr<const ProofResponse> value) const
{
	if(!value->is_valid()) {
		throw std::logic_error("invalid response");
	}
	if(auto root = get_root()) {
		if(value->vdf_height <= root->vdf_height) {
			throw std::logic_error("proof too old");
		}
	}
	hash_t challenge;
	uint64_t space_diff = 0;
	if(!find_challenge(value->vdf_height, challenge, space_diff)) {
		throw std::logic_error("cannot find challenge");
	}
	verify_proof(value->proof, challenge, space_diff);

	publish(value, output_verified_proof);
}

void Node::verify_proof(std::shared_ptr<fork_t> fork) const
{
	const auto block = fork->block;
	const auto prev = find_prev(block);
	if(!prev) {
		throw std::logic_error("cannot verify");
	}
	if(block->proof.empty()) {
		throw std::logic_error("missing proof");
	}
	if(block->vdf_count > params->max_vdf_count) {
		throw std::logic_error("invalid vdf_count");
	}
	if(block->vdf_height != prev->vdf_height + block->vdf_count) {
		throw std::logic_error("invalid vdf_height");
	}
	{
		hash_t prev;
		std::set<hash_t> set;
		for(auto proof : block->proof) {
			const auto hash = proof->calc_proof_hash();
			if(hash < prev) {
				throw std::logic_error("invalid proof order");
			}
			if(set.empty() && block->proof_hash != hash) {
				throw std::logic_error("invalid proof_hash");
			}
			if(!set.insert(hash).second) {
				throw std::logic_error("duplicate proof");
			}
			prev = hash;
		}
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
	if(block->time_diff < params->time_diff_divider) {
		throw std::logic_error("time_diff too low");
	}
	validate_diff_adjust(block->time_diff, prev->time_diff);	// need to check here to avoid VDF verify attack

	// need to verify challenge and space_diff update here
	bool is_space_fork = false;
	const auto next_challenge = calc_next_challenge(params, prev->challenge, block->vdf_count, block->proof_hash, is_space_fork);
	if(block->challenge != next_challenge) {
		throw std::logic_error("invalid challenge");
	}
	if(block->is_space_fork != is_space_fork) {
		throw std::logic_error("invalid is_space_fork");
	}
	const auto proof_count = block->proof.size();

	if(is_space_fork) {
		const auto space_diff = calc_new_space_diff(params, prev);
		if(block->space_diff != space_diff) {
			throw std::logic_error("invalid space_diff: " + std::to_string(block->space_diff) + " != " + std::to_string(space_diff));
		}
		if(block->space_fork_len != block->vdf_count) {
			throw std::logic_error("invalid space_fork_len at space fork");
		}
		if(block->space_fork_proofs != proof_count) {
			throw std::logic_error("invalid space_fork_proofs at space fork");
		}
	} else {
		if(block->space_diff != prev->space_diff) {
			throw std::logic_error("invalid space_diff change");
		}
		if(block->space_fork_len != prev->space_fork_len + block->vdf_count) {
			throw std::logic_error("invalid space_fork_len");
		}
		if(block->space_fork_proofs != prev->space_fork_proofs + proof_count) {
			throw std::logic_error("invalid space_fork_proofs");
		}
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

	for(auto proof : block->proof) {
		verify_proof(proof, challenge, space_diff);
	}
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

void Node::verify_vdf(std::shared_ptr<const ProofOfTime> proof, const int64_t recv_time)
{
	if(!proof->is_valid()) {
		throw std::logic_error("static validation failed");
	}
	proof->validate();

	const auto prev = get_header(proof->prev);
	if(!prev) {
		throw std::logic_error("could not find infused block");
	}
	const auto num_iters = get_block_iters(params, get_time_diff(prev));

	if(proof->num_iters != num_iters) {
		throw std::logic_error("invalid num_iters");
	}
	if(proof->segment_size != params->vdf_segment_size) {
		throw std::logic_error("invalid segment size");
	}
	if(proof->segments.size() * proof->segment_size != num_iters) {
		throw std::logic_error("invalid segment count");
	}
	vdf_threads->add_task(std::bind(&Node::verify_vdf_task, this, proof, recv_time));
	vdf_verify_pending.insert(proof->hash);
	timelord_trust[proof->timelord_key]--;
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
				point[j] = hash_t(proof->input + proof->prev);
				point[j] = hash_t(point[j] + proof->reward_addr);
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

void Node::verify_vdf_success(std::shared_ptr<const VDF_Point> point, const int64_t took_ms)
{
	const auto peak = get_peak();
	const auto proof = point->proof;
	const auto chain = find_next_vdf_points(peak);

	bool is_advance = false;
	if(chain.empty()) {
		is_advance = point->input == peak->vdf_output;
	} else {
		const auto prev = chain.back();
		is_advance = point->input == prev->output;
	}

	if(is_advance) {
		if(chain.size() >= params->max_vdf_count) {
			log(WARN) << "VDF chain reached maximum length, discarded VDF for height " << point->vdf_height;
			return;
		}
		if(is_synced) {
			log(INFO) << "-------------------------------------------------------------------------------";
		}
		stuck_timer->reset();	// make sure we keep sync status
		timelord_trust[proof->timelord_key] += 2;

		publish(proof, output_verified_vdfs);
	}
	const auto vdf_iters = point->start + point->num_iters;

	vdf_tree.emplace(point->output, point);
	vdf_index.emplace(vdf_iters, point);
	vdf_verify_pending.erase(proof->hash);

	publish(point, output_vdf_points);

	if(took_ms > params->block_interval_ms) {
		log(WARN) << "VDF verification took longer than block interval, unable to keep sync!";
	} else if(took_ms > params->block_interval_ms / 2) {
		log(WARN) << "VDF verification took longer than recommended: " << took_ms / 1e3 << " sec";
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
		ss_delta << ", delta = " << (point->recv_time - prev->recv_time) / 1e3 << " sec" ;
	}
	const char* clocks[] = {
			u8"\U0001F550", u8"\U0001F551", u8"\U0001F552", u8"\U0001F553", u8"\U0001F554", u8"\U0001F555",
			u8"\U0001F556", u8"\U0001F557", u8"\U0001F558", u8"\U0001F559", u8"\U0001F55A", u8"\U0001F55B" };

	log(INFO) << clocks[point->vdf_height % 12] << " Verified VDF for height "
			<< point->vdf_height << ss_delta.str() << ", took " << took_ms / 1e3 << " sec";

	trigger_update();
}

void Node::verify_vdf_task(std::shared_ptr<const ProofOfTime> proof, const int64_t recv_time) noexcept
{
	std::shared_ptr<OCL_VDF> engine;
	try {
		if(opencl_vdf_enable) {
			std::unique_lock lock(vdf_mutex);
			while(opencl_vdf.empty()) {
				vdf_signal.wait(lock);
			}
			engine = opencl_vdf.back();
			opencl_vdf.pop_back();
		}
		const auto begin = get_time_ms();

		if(engine) {
			engine->compute(proof);
			engine->verify(proof);
		} else {
			verify_vdf_cpu(proof);
		}
		const auto took_ms = get_time_ms() - begin;

		auto point = VDF_Point::create();
		point->vdf_height = proof->vdf_height;
		point->start = proof->start;
		point->num_iters = proof->num_iters;
		point->input = proof->input;
		point->output = proof->get_output();
		point->prev = proof->prev;
		point->reward_addr = proof->reward_addr;
		point->recv_time = recv_time;
		point->proof = proof;
		point->content_hash = point->calc_hash();

		add_task(std::bind(&Node::verify_vdf_success, this, point, took_ms));
	}
	catch(const std::exception& ex) {
		add_task([this, proof]() {
			timelord_trust[proof->timelord_key] = 0;
			vdf_verify_pending.erase(proof->hash);
			trigger_update();
		});
		log(WARN) << "VDF verification for height " << proof->vdf_height << " failed with: " << ex.what();
	}

	if(engine) {
		std::unique_lock lock(vdf_mutex);
		opencl_vdf.push_back(engine);
	}
	vdf_signal.notify_all();
}

void Node::check_vdf(std::shared_ptr<fork_t> fork)
{
	static bool have_sha_ni = sha256_ni_available();
	if(!have_sha_ni) {
		fork->is_vdf_verified = true;	// by-pass as it takes too long
		return;
	}
	const auto& block = fork->block;
	const auto prev = find_prev(block);
	if(!prev) {
		throw std::logic_error("cannot check VDF");
	}
	std::vector<std::tuple<uint64_t, hash_t, addr_t>> list;
	for(uint32_t i = 0; i < block->vdf_count; ++i) {
		uint64_t num_iters = 0;
		const auto infuse = get_infusion(prev, i, num_iters);
		list.emplace_back(num_iters, infuse, block->vdf_reward_addr[i]);
	}
	log(INFO) << "Checking VDF for height " << block->height << " ...";

	vdf_threads->add_task(std::bind(&Node::check_vdf_task, this, fork, prev, list));
}

void Node::check_vdf_task(
		std::shared_ptr<fork_t> fork,
		std::shared_ptr<const BlockHeader> prev,
		const std::vector<std::tuple<uint64_t, hash_t, addr_t>>& list) noexcept
{
	static bool have_sha_ni = sha256_ni_available();

	// MAY NOT ACCESS OR MODIFY ANY NON-CONSTNAT NODE DATA
	const auto time_begin = get_time_ms();
	const auto& block = fork->block;

	auto point = prev->vdf_output;
	uint64_t vdf_iters = prev->vdf_iters;

	for(const auto& entry : list)
	{
		const auto num_iters = std::get<0>(entry);
		point = hash_t(point + std::get<1>(entry));
		point = hash_t(point + std::get<2>(entry));

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
	const auto elapsed = (get_time_ms() - time_begin) / 1e3;

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
