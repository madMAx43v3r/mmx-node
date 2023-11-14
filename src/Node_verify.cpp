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
#include <mmx/chiapos.h>
#include <mmx/utils.h>

#include <vnx/vnx.h>
#include <sha256_avx2.h>
#include <sha256_ni.h>


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
	const auto vdf_block = get_header_at(request->height - params->challenge_delay);
	if(!vdf_block) {
		return false;
	}
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
	// Note: block->is_valid() already called in pre-validate step

	if(block->vdf_iters != prev->vdf_iters + diff_block->time_diff * params->time_diff_constant) {
		throw std::logic_error("invalid vdf_iters");
	}
	// Note: vdf_output already verified to match vdf_iters

	if(auto proof = block->proof) {
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
			return 0;
		}
	}
	auto head = find_fork(hash);
	auto block = head ? head->block : get_header(hash);
	if(!block) {
		throw std::logic_error("no such block");
	}
	const auto root = get_root();
	const auto key = std::make_pair(plot_id, addr_t());

	uint128 balance = 0;
	balance_table.find(key, balance, std::min(root->height, block->height));

	if(head) {
		for(const auto& fork : get_fork_line(head)) {
			{
				auto iter = fork->balance.added.find(key);
				if(iter != fork->balance.added.end()) {
					balance += iter->second;
				}
			}
			{
				auto iter = fork->balance.removed.find(key);
				if(iter != fork->balance.removed.end()) {
					clamped_sub_assign(balance, iter->second);
				}
			}
		}
	}
	if(balance.upper()) {
		throw std::logic_error("balance overflow");
	}
	return balance;
}

template<typename T>
uint256_t Node::verify_proof_impl(	std::shared_ptr<const T> proof, const hash_t& challenge,
									std::shared_ptr<const BlockHeader> diff_block) const
{
	if(proof->ksize < params->min_ksize) {
		throw std::logic_error("ksize too small");
	}
	if(proof->ksize > params->max_ksize) {
		throw std::logic_error("ksize too big");
	}
	const auto plot_challenge = get_plot_challenge(challenge, proof->plot_id);
	const auto quality = hash_t::from_bytes(chiapos::verify(
			proof->ksize, proof->plot_id.bytes, plot_challenge.bytes, proof->proof_bytes.data(), proof->proof_bytes.size()));

	return calc_proof_score(params, proof->ksize, quality, diff_block->space_diff);
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
		score = verify_proof_impl(og_proof, challenge, diff_block);
	}
	else if(auto nft_proof = std::dynamic_pointer_cast<const ProofOfSpaceNFT>(proof))
	{
		score = verify_proof_impl(nft_proof, challenge, diff_block);
	}
	else if(auto stake = std::dynamic_pointer_cast<const ProofOfStake>(proof))
	{
		const auto plot = get_contract_as<contract::VirtualPlot>(stake->contract);
		if(!plot) {
			throw std::logic_error("no such virtual plot: " + stake->contract.to_string());
		}
		if(stake->farmer_key != plot->farmer_key) {
			throw std::logic_error("invalid farmer key for virtual plot: " + stake->farmer_key.to_string());
		}
		const auto balance = get_virtual_plot_balance(stake->contract, diff_block->hash);
		if(balance == 0) {
			throw std::logic_error("virtual plot has zero balance: " + stake->contract.to_string());
		}
		score = calc_virtual_score(params, challenge, stake->contract, balance, diff_block->space_diff);
	}
	else {
		throw std::logic_error("invalid proof type: " + proof->get_type_name());
	}
	if(score != proof->score) {
		throw std::logic_error("proof score mismatch: expected " + std::to_string(proof->score) + " but got " + score.str(10));
	}
	if(score >= params->score_threshold) {
		throw std::logic_error("proof score >= score_threshold");
	}
}

void Node::verify_vdf(std::shared_ptr<const ProofOfTime> proof) const
{
	if(!proof->is_valid(params)) {
		throw std::logic_error("invalid vdf");
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
		if(proof->infuse[0] || proof->infuse[1]) {
			throw std::logic_error("invalid infusion");
		}
	}
	vdf_threads->add_task(std::bind(&Node::verify_vdf_task, this, proof));
}

void Node::verify_vdf(std::shared_ptr<const ProofOfTime> proof, const uint32_t chain) const
{
	static bool have_sha_ni = sha256_ni_available();

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

		uint32_t max_iters = 0;
		hash_t point[batch_size];
		uint8_t hash[batch_size][32];
		uint8_t input[batch_size][64];

		for(uint32_t j = 0; j < num_lanes; ++j)
		{
			const uint32_t i = chunk * batch_size + j;
			if(i > 0) {
				point[j] = chain < 2 ? segments[i-1].output[chain] : proof->reward_segments[i-1];
			} else {
				point[j] = proof->input[chain % 2];
				if(chain < 2) {
					if(auto infuse = proof->infuse[chain]) {
						point[j] = hash_t(point[j] + (*infuse));
					}
				} else if(proof->reward_addr) {
					point[j] = hash_t(point[j] + (*proof->reward_addr));
				}
			}
			max_iters = std::max(max_iters, segments[i].num_iters);
		}
		if(have_sha_ni) {
			for(uint32_t j = 0; j < num_lanes; ++j)
			{
				const uint32_t i = chunk * batch_size + j;
				recursive_sha256_ni(point[j].data(), segments[i].num_iters);
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
			const auto& output = chain < 2 ? segments[i].output[chain] : proof->reward_segments[i];
			if(point[j] != output) {
				is_valid = false;
				invalid_segment = i;
			}
		}
	}
	if(!is_valid) {
		throw std::logic_error("invalid output on chain " + std::to_string(chain) + ", segment " + std::to_string(invalid_segment));
	}
}

// comment: compute and verify all VDF streams, load balanced on cpu/opencl
void Node::verify_vdf_cpuocl(std::shared_ptr<const ProofOfTime> proof) const
{
	// TODO: Check edge/limit cases on all areas below, 0/small input amounts

	static bool have_sha_ni = sha256_ni_available();
	static bool have_avx2 = avx2_available();
	bool have_ocl = false;
	if(auto engine = opencl_vdf[0]) {
		have_ocl = true;
	}
	if(!(have_ocl && (have_sha_ni || have_avx2))) {
		throw std::logic_error("verify_vdf_cpuocl missing hardware requirements");
	}

	// TODO: bad/temporary solution with 'static', find better/correct location between blocks
	static uint64_t prev_time_cpu = 0;
	static uint64_t prev_time_ocl = 0;
	static int prev_num_cpu = 0;
	static int prev_num_ocl = 0;

	const auto& segments = proof->segments;
	bool is_valid = !segments.empty();
	size_t invalid_segment = -1;
	size_t num_invalid_segment = 0;

	const int num_chain = (verify_vdf_rewards && proof->reward_addr) ? 3 : 2;
	const int num_point = segments.size() * num_chain;
	if(!num_point) {
		throw std::logic_error("no segments");
	}

	std::vector<hash_t> point;
	std::vector<hash_t> point_verify;
	std::vector<uint32_t> num_iters;
	uint32_t max_iters = 0;

	point.resize(num_point);
	point_verify.resize(num_point);
	num_iters.resize(num_point);

	// comment: copy/infuse points to be hashed, points to verify against, number of iters, 3x vectors
	for(int chain = 0; chain < num_chain; ++chain)
	{
		for(size_t i = 0, j = chain * segments.size(); i < segments.size(); ++i, ++j)
		{
			if(i > 0) {
				point[j] = chain < 2 ? segments[i-1].output[chain] : proof->reward_segments[i-1];
			} else {
				point[j] = proof->input[chain % 2];
				if(chain < 2) {
					if(auto infuse = proof->infuse[chain]) {
						point[j] = hash_t(point[j] + (*infuse));
					}
				} else if(proof->reward_addr) {
					point[j] = hash_t(point[j] + (*proof->reward_addr));
				}
			}
			point_verify[j] = chain < 2 ? segments[i].output[chain] : proof->reward_segments[i];
			num_iters[j] = segments[i].num_iters;
			max_iters = std::max(max_iters, num_iters[j]);
		}
	}

	// comment: find load balance between cpu/opencl, based on last block, or 50/50 if none
	// comment: calc time per point for cpu/opencl, estimate point balance to get equal total time
	// comment: limit minimum usage to 10% of points to either cpu/opencl
	int num_point_cpu = num_point / 2;
	int num_point_ocl = num_point - num_point_cpu;
	if(prev_time_cpu > 0 && prev_time_ocl > 0 && prev_num_cpu > 0 && prev_num_ocl > 0) {
		const int prev_num_point = prev_num_cpu + prev_num_ocl;
		const uint64_t cpu_micro_pt = prev_time_cpu / prev_num_cpu;
		const uint64_t ocl_micro_pt = prev_time_ocl / prev_num_ocl;
		uint64_t balance_pct = (((prev_num_point * ocl_micro_pt) / (cpu_micro_pt + ocl_micro_pt)) * 100) / prev_num_point;

		// TODO: find out real practical lower limit on each side of cpu/openCL, percent or points
		balance_pct = std::clamp(balance_pct, (uint64_t)10, (uint64_t)90);

		num_point_cpu = (num_point * balance_pct) / 100;
		num_point_ocl = num_point - num_point_cpu;
	}

	// comment: find balanced thread number based on recommended hardware concurrency
	// comment: - too few threads will choke cpu calc potential
	// comment: - too many threads will choke opencl thread potential
	// comment: - hardware_concurrency() looks to give logical cpu units, not physical
	// comment: balance found, good middleground: 'logic cpu units' + 50%
	// comment: limit minimum to 8, maximum to 72
	const uint32_t async_num = std::clamp((std::thread::hardware_concurrency() * 15) / 10, (uint32_t)8, (uint32_t)72);

	// comment: find batch size of points for each cpu thread, and how many full batches
	const uint32_t batch_size = (num_point_cpu + async_num - 1) / async_num;
	const uint32_t batch_full = (num_point_cpu % async_num) ? (num_point_cpu % async_num) : async_num;
	// TODO: check/fix logic if num_point_cpu less than async_num

	// comment: store start/stop timestamp for all cpu threads, 1x opencl thread
	std::vector<uint64_t> time_cpu_start;
	std::vector<uint64_t> time_cpu_stop;
	uint64_t time_ocl;
	time_cpu_start.resize(async_num);
	time_cpu_stop.resize(async_num);

	// comment: hash all points, load balanced on cpu/opencl
#pragma omp parallel for
	for(int chunk = 0; chunk < (int)async_num + 1; ++chunk)
	{
		if(chunk < (int)async_num) {
			// comment: cpu, async_num threads
			time_cpu_start[chunk] = vnx::get_time_micros();
			const uint32_t batch_real = (chunk < (int)batch_full) ? batch_size : batch_size - 1;
			const uint32_t batch_startp = (chunk < (int)batch_full) ? (chunk * batch_size) : (batch_full * batch_size) + ((chunk - batch_full) * (batch_size - 1));

			if(have_sha_ni) {
				// comment: cpu, sha-ni
				for(uint32_t j = 0; j < batch_real; ++j)
				{
					const uint32_t i = batch_startp + j;
					recursive_sha256_ni(point[i].data(),num_iters[i]);
				}
			} else {
				// comment: cpu, avx2
				constexpr uint32_t avx_batch_size = 8;
				const uint32_t avx_num_chunks = (batch_real + avx_batch_size - 1) / avx_batch_size;
				const uint32_t avx_batch_lastsize = (batch_real % avx_batch_size) ? batch_real % avx_batch_size : avx_batch_size;
				// TODO: check/fix logic if batch_real less than avx_batch_size

				for(uint32_t avx_chunk = 0; avx_chunk < avx_num_chunks; ++avx_chunk)
				{
					const uint32_t avx_batch_real = (avx_chunk < avx_num_chunks - 1) ? avx_batch_size : avx_batch_lastsize;
					uint32_t max_iters = 0;
					uint8_t hash[avx_batch_size][32];
					uint8_t input[avx_batch_size][64];

					for(uint32_t j = 0; j < avx_batch_real; ++j)
					{
						const uint32_t i = (batch_startp) + (avx_chunk * avx_batch_size) + j;
						max_iters = std::max(max_iters, num_iters[i]);
					}

					for(uint32_t k = 0; k < max_iters; ++k)
					{
						for(uint32_t j = 0; j < avx_batch_real; ++j)
						{
							const uint32_t i = (batch_startp) + (avx_chunk * avx_batch_size) + j;
							max_iters = std::max(max_iters, num_iters[i]);
							::memcpy(input[j], point[i].data(), 32);
						}
						// comment: if testing avx2, call direct sha256_avx2_64_x8(), or else sha-ni
						sha256_64_x8(hash[0],input[0],32);

						for(uint32_t j = 0; j < avx_batch_real; ++j)
						{
							const uint32_t i = (batch_startp) + (avx_chunk * avx_batch_size) + j;
							if(k < num_iters[i]) {
								::memcpy(point[i].data(), hash[j], 32);
							}
						}
					}
				}
			}

			time_cpu_stop[chunk] = vnx::get_time_micros();
		} else {
			// comment: opencl, 1x thread
			time_ocl = vnx::get_time_micros();
			auto engine = opencl_vdf[0];
			engine->compute_point(&point, &num_iters, num_point_cpu, num_point_ocl);
			time_ocl = vnx::get_time_micros() - time_ocl;
		}
	}

	// comment: store total cpu and opencl time, cpu is earliest/latest timestamp on cpu threads
	for(uint32_t chunk = 0; chunk < async_num; ++chunk)
	{
		time_cpu_start[0] = std::min(time_cpu_start[0], time_cpu_start[chunk]);
		time_cpu_stop[0] = std::max(time_cpu_stop[0], time_cpu_stop[chunk]);
	}
	prev_time_cpu = time_cpu_stop[0] - time_cpu_start[0];
	prev_time_ocl = time_ocl;

	prev_num_cpu = num_point_cpu;
	prev_num_ocl = num_point_ocl;

	log(INFO) << "CPU/OpenCL verify VDF stats for height " << proof->height << ", " << (prev_time_cpu / 1000) << "ms/" << (prev_time_ocl / 1000) << "ms (" << prev_num_cpu << "pts/" << prev_num_ocl << "pts), " << num_chain << "/" << segments.size() << " (segments/size), " << std::thread::hardware_concurrency() << "t/" << async_num << "t (hint/used)";
	// TODO: switch back to (DEBUG) before final version

	// comment: verify all hashed points
	for(int chain = 0; chain < num_chain; ++chain)
	{
		for(size_t i = 0, j = chain * segments.size(); i < segments.size(); ++i, ++j)
		{
			if(point[j] != point_verify[j]) {
				if(is_valid) {
					invalid_segment = j;
				}
				is_valid = false;
				++num_invalid_segment;
			}
		}
	}

	if(!is_valid) {
		throw std::logic_error("invalid output on chain " + std::to_string(invalid_segment / segments.size()) + ", segment " + std::to_string(invalid_segment % segments.size()) + ", total of " + std::to_string(num_invalid_segment) + " invalid segments on chains");
	}
}

void Node::verify_vdf_success(std::shared_ptr<const ProofOfTime> proof, std::shared_ptr<vdf_point_t> point)
{
	if(is_synced && proof->height > get_height()) {
		log(INFO) << "-------------------------------------------------------------------------------";
	}
	verified_vdfs.emplace(proof->height, point);
	vdf_verify_pending.erase(proof->height);

	const auto elapsed = (vnx::get_wall_time_micros() - point->recv_time) / 1000 / 1e3;
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
	std::stringstream ss_delta;
	std::stringstream ss_reward;
	if(prev) {
		ss_delta << ", delta = " << (point->recv_time - prev->recv_time) / 1000 / 1e3 << " sec" ;
	}
	if(verify_vdf_rewards && proof->reward_addr) {
		ss_reward << " (with reward)";
	}
	log(INFO) << "Verified VDF for height " << proof->height << ss_delta.str() << ", took " << elapsed << " sec" << ss_reward.str();

	// add dummy blocks
	const auto root = get_root();
	if(proof->height == root->height + 1) {
		add_dummy_block(root);
	}
	const auto range = fork_index.equal_range(proof->height - 1);
	for(auto iter = range.first; iter != range.second; ++iter) {
		add_dummy_block(iter->second->block);
	}
	trigger_update();
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
		auto point = std::make_shared<vdf_point_t>();
		bool ocl_available = false;
		if(auto engine = opencl_vdf[0]) {
			ocl_available = true;
		}
		// TODO: remove temporary create/set 'verify_vdf_multiple' variable when generated vnx ready
		bool verify_vdf_multiple = true;
		// comment: only verify on cpu/opencl if hardware available, and enabled in settings
		if(verify_vdf_multiple && ocl_available && (sha256_ni_available() || avx2_available())) {
			verify_vdf_cpuocl(proof);
			if(verify_vdf_rewards && proof->reward_addr) {
				point->vdf_reward_valid = true;
			}
		} else {
			for(int i = 0; i < 3; ++i) {
				if(i < 2 || (verify_vdf_rewards && proof->reward_addr)) {
					if(auto engine = opencl_vdf[i]) {
						engine->compute(proof, i);
					}
				}
			}

			for(int i = 0; i < 3; ++i) {
				if(i < 2 || (verify_vdf_rewards && proof->reward_addr)) {
					try {
						if(auto engine = opencl_vdf[i]) {
							engine->verify(proof, i);
						} else {
							verify_vdf(proof, i);
						}
						if(i == 2) {
							point->vdf_reward_valid = true;
						}
					} catch(const std::exception& ex) {
						if(i < 2) {
							throw;
						} else {
							log(WARN) << "Invalid VDF reward proof for height " << proof->height << ": " << ex.what();
						}
					}
				}
			}
		}
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
	const auto time_begin = vnx::get_wall_time_millis();
	const auto& block = fork->block;

	auto point = prev->vdf_output;
	if(block->height > params->infuse_delay) {
		point[0] = hash_t(point[0] + infuse->hash);
	}
	if(infuse->height >= params->challenge_interval && infuse->height % params->challenge_interval == 0) {
		point[1] = hash_t(point[1] + infuse->hash);
	}
	const auto num_iters = block->vdf_iters - prev->vdf_iters;

	vnx::ThreadPool threads(2);

	for(int chain = 0; chain < 2; ++chain) {
		threads.add_task([&point, chain, num_iters]() {
			for(uint64_t i = 0; i < num_iters; ++i) {
				point[chain] = hash_t(point[chain].bytes);
			}
		});
	}
	threads.sync();

	if(point == block->vdf_output) {
		fork->is_vdf_verified = true;
		const auto elapsed = (vnx::get_wall_time_millis() - time_begin) / 1e3;
		log(INFO) << "VDF check for height " << block->height << " passed, took " << elapsed << " sec";
	} else {
		fork->is_invalid = true;
		log(WARN) << "VDF check for height " << block->height << " failed!";
	}
}


} // mmx
