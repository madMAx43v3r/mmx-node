/*
 * verify.cpp
 *
 *  Created on: Nov 5, 2023
 *      Author: mad
 */

#include <mmx/pos/verify.h>
#include <mmx/pos/mem_hash.h>
#include <mmx/hash_512_t.hpp>
#include <mmx/utils.h>

#ifdef WITH_CUDA
#include <mmx/pos/cuda_recompute.h>
#endif

#include <set>
#include <algorithm>
#include <vnx/vnx.h>
#include <vnx/ThreadPool.h>


namespace mmx {
namespace pos {

static constexpr uint32_t MEM_SIZE = 32 * 32;

static std::mutex g_mutex;
static std::shared_ptr<vnx::ThreadPool> g_threads;


void compute_f1(std::vector<uint32_t>* X_out,
				std::vector<uint32_t>& Y_out,
				std::vector<std::array<uint32_t, N_META>>& M_out,
				std::mutex& mutex,
				const uint32_t X,
				const hash_t& id, const int ksize, const int xbits)
{
	const uint32_t kmask = ((uint64_t(1) << ksize) - 1);

	std::vector<uint32_t> mem_buf(MEM_SIZE);

	for(uint32_t x_i = 0; x_i < (uint64_t(1) << xbits); ++x_i)
	{
		const uint32_t X_i = (X << xbits) | x_i;

		uint32_t msg[9] = {};
		msg[0] = X_i;
		::memcpy(msg + 1, id.data(), id.size());

		const hash_512_t key(&msg, sizeof(msg));
		gen_mem_array(mem_buf.data(), key.data(), MEM_SIZE);

		uint8_t mem_hash[64 + 128] = {};
		::memcpy(mem_hash, key.data(), key.size());

		calc_mem_hash(mem_buf.data(), mem_hash + 64, MEM_HASH_ITER);

		const hash_512_t mem_hash_hash(mem_hash, sizeof(mem_hash));

		uint32_t hash[16] = {};
		::memcpy(hash, mem_hash_hash.data(), mem_hash_hash.size());

		uint32_t Y_i = 0;
		std::array<uint32_t, N_META> meta = {};
		for(int i = 0; i < N_META; ++i) {
			Y_i = Y_i ^ hash[i];
			meta[i] = hash[i] & kmask;
		}
		Y_i &= kmask;

		std::lock_guard<std::mutex> lock(mutex);
		if(X_out) {
			X_out->push_back(X_i);
		}
		Y_out.push_back(Y_i);
		M_out.push_back(meta);
	}
}

std::vector<std::pair<uint32_t, bytes_t<META_BYTES_OUT>>>
compute(const std::vector<uint32_t>& X_values, std::vector<uint32_t>* X_out, const hash_t& id, const int ksize, const int xbits)
{
	if(ksize < 8 || ksize > 32) {
		throw std::logic_error("invalid ksize");
	}
	if(xbits < 0 || xbits > ksize) {
		throw std::logic_error("invalid xbits");
	}

#ifdef WITH_CUDA
	if(have_cuda_recompute()) {
		const auto job = cuda_recompute(ksize, xbits, id, X_values);
		const auto res = cuda_recompute_poll({job});
		if(res) {
			if(res->failed) {
				throw std::runtime_error("CUDA failed with: " + res->error);
			}
			if(X_out) {
				*X_out = res->X;
			}
			return res->entries;
		}
	}
#endif

	const bool use_threads = (xbits >= 5);
	if(use_threads) {
		std::lock_guard<std::mutex> lock(g_mutex);
		if(!g_threads) {
			const auto cpu_threads = std::thread::hardware_concurrency();
			const auto num_threads = cpu_threads > 0 ? cpu_threads : 16;
			g_threads = std::make_shared<vnx::ThreadPool>(num_threads, 1024);
			vnx::log_info() << "Using " << num_threads << " CPU threads for proof recompute";
		}
	}
	const auto X_set = std::set<uint32_t>(X_values.begin(), X_values.end());

	const uint64_t num_entries_1 = X_set.size() << xbits;

	std::mutex mutex;
	std::vector<int64_t> jobs;
	std::vector<uint32_t> X_tmp;
	std::vector<uint32_t> Y_tmp;
	std::vector<uint32_t> mem_buf(MEM_SIZE);
	std::vector<std::array<uint32_t, N_META>> M_tmp;

//	const auto t1_begin = get_time_ms();

	if(X_out) {
		X_tmp.reserve(num_entries_1);
	}
	Y_tmp.reserve(num_entries_1);
	M_tmp.reserve(num_entries_1);

	for(const auto X : X_set)
	{
		if(use_threads) {
			const auto job = g_threads->add_task(
				[X_out, &X_tmp, &Y_tmp, &M_tmp, &mutex, X, id, ksize, xbits]() {
					compute_f1(X_out ? &X_tmp : nullptr, Y_tmp, M_tmp, mutex, X, id, ksize, xbits);
				});
			jobs.push_back(job);
		} else {
			compute_f1(X_out ? &X_tmp : nullptr, Y_tmp, M_tmp, mutex, X, id, ksize, xbits);
		}
	}

	if(use_threads) {
		g_threads->sync(jobs);
		jobs.clear();
	}
//	std::cout << "Table 1 took " << (get_time_ms() - t1_begin) << " ms" << std::endl;

	return compute_full(X_tmp, Y_tmp, M_tmp, X_out, id, ksize);
}

hash_t calc_quality(const hash_t& challenge, const bytes_t<META_BYTES_OUT>& meta)
{
	// proof output needs to be hashed after challenge, otherwise compression to 256-bit is possible
	return hash_t(std::string("proof_quality") + challenge + meta);
}

bool check_post_filter(const hash_t& challenge, const bytes_t<META_BYTES_OUT>& meta, const int post_filter)
{
	// proof output needs to be hashed after challenge, otherwise compression to 256-bit is possible
	const hash_t hash(std::string("post_filter") + challenge + meta);
	return (hash.to_uint256() >> (256 - post_filter)) == 0;
}

hash_t verify(	const std::vector<uint32_t>& X_values, const hash_t& challenge, const hash_t& id,
				const int plot_filter, const int post_filter, const int ksize, const bool hard_fork)
{
	if(X_values.size() != (1 << (N_TABLE - 1))) {
		throw std::logic_error("invalid proof size");
	}
	std::vector<uint32_t> X_out;
	const auto entries = compute(X_values, &X_out, id, ksize, 0);
	if(entries.empty()) {
		throw std::logic_error("invalid proof");
	}
	if(entries.size() > 1) {
		throw std::logic_error("more than one proof found");
	}
	const auto& result = entries[0];
	const auto& Y = result.first;

	const uint32_t kmask = ((uint64_t(1) << ksize) - 1);

	const uint32_t Y_0 = bytes_t<4>(challenge.data(), 4).to_uint<uint32_t>() & kmask;

	const uint64_t Y_end = uint64_t(Y_0) + (1 << plot_filter);

	if((Y < Y_0) || (Y >= Y_end)) {
		throw std::logic_error("invalid Y value");
	}
	if(X_out != X_values) {
		throw std::logic_error("invalid proof order");
	}

	if(hard_fork) {
		if(!check_post_filter(challenge, result.second, post_filter)) {
			throw std::logic_error("post filter failed");
		}
		return calc_proof_hash(challenge, X_out);
	} else {
		return calc_quality(challenge, result.second);
	}
}




} // pos
} // mmx
