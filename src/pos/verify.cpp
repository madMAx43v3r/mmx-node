/*
 * verify.cpp
 *
 *  Created on: Nov 5, 2023
 *      Author: mad
 */

#include <mmx/pos/verify.h>
#include <mmx/pos/mem_hash.h>
#include <mmx/hash_512_t.hpp>

#include <set>
#include <algorithm>
#include <vnx/vnx.h>
#include <vnx/ThreadPool.h>


namespace mmx {
namespace pos {

static constexpr int MEM_HASH_ITER = 256;

static constexpr uint64_t MEM_SIZE = 32 * 32;

static std::mutex g_mutex;
static std::shared_ptr<vnx::ThreadPool> g_threads;


void compute_f1(std::vector<uint32_t>* X_tmp,
				std::vector<std::array<uint32_t, N_META>>& M_tmp,
				std::vector<std::pair<uint32_t, uint32_t>>& entries,
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
		gen_mem_array(mem_buf.data(), key.data(), key.size(), MEM_SIZE);

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
		if(X_tmp) {
			X_tmp->push_back(X_i);
		}
		entries.emplace_back(Y_i, M_tmp.size());
		M_tmp.push_back(meta);
	}
}

std::vector<std::pair<uint32_t, bytes_t<META_BYTES_OUT>>>
compute(const std::vector<uint32_t>& X_values, std::vector<uint32_t>* X_out, const hash_t& id, const int ksize, const int xbits)
{
	if(ksize < 8 || ksize > 32) {
		throw std::logic_error("invalid ksize");
	}
	if(xbits > ksize) {
		throw std::logic_error("invalid xbits");
	}
	const auto X_set = std::set<uint32_t>(X_values.begin(), X_values.end());

	const bool use_threads = (xbits >= 6);

	if(use_threads) {
		std::lock_guard<std::mutex> lock(g_mutex);
		if(!g_threads) {
			const auto cpu_threads = std::thread::hardware_concurrency();
			const auto num_threads = cpu_threads > 0 ? cpu_threads : 16;
			g_threads = std::make_shared<vnx::ThreadPool>(num_threads, 1024);
			vnx::log_info() << "Using " << num_threads << " CPU threads for proof compute";
		}
	}
	const uint32_t kmask = ((uint64_t(1) << ksize) - 1);
	const uint64_t num_entries_1 = X_set.size() << xbits;

	std::mutex mutex;
	std::vector<int64_t> jobs;
	std::vector<uint32_t> X_tmp;
	std::vector<uint32_t> mem_buf(MEM_SIZE);
	std::vector<std::array<uint32_t, N_META>> M_tmp;
	std::vector<std::pair<uint32_t, uint32_t>> entries;
	std::vector<std::vector<std::pair<uint32_t, uint32_t>>> LR_tmp(N_TABLE + 1);

//	const auto t1_begin = vnx::get_time_millis();

	if(X_out) {
		X_tmp.reserve(num_entries_1);
	}
	M_tmp.reserve(num_entries_1);
	entries.reserve(num_entries_1);

	for(const auto X : X_set)
	{
		if(use_threads) {
			const auto job = g_threads->add_task(
				[X_out, &X_tmp, &M_tmp, &entries, &mutex, X, id, ksize, xbits]() {
					compute_f1(X_out ? &X_tmp : nullptr, M_tmp, entries, mutex, X, id, ksize, xbits);
				});
			jobs.push_back(job);
		} else {
			compute_f1(X_out ? &X_tmp : nullptr, M_tmp, entries, mutex, X, id, ksize, xbits);
		}
	}

	if(use_threads) {
		g_threads->sync(jobs);
		jobs.clear();
	}
//	std::cout << "Table 1 took " << (vnx::get_time_millis() - t1_begin) << " ms" << std::endl;

	for(int t = 2; t <= N_TABLE; ++t)
	{
//		const auto time_begin = vnx::get_time_millis();

		std::vector<std::array<uint32_t, N_META>> M_next;
		std::vector<std::pair<uint32_t, uint32_t>> matches;

		std::sort(entries.begin(), entries.end());

//		std::cout << "Table " << t << " sort took " << (vnx::get_time_millis() - time_begin) << " ms" << std::endl;

		for(size_t x = 0; x < entries.size(); ++x)
		{
			const auto YL = entries[x].first;

			for(size_t y = x + 1; y < entries.size(); ++y)
			{
				const auto YR = entries[y].first;

				if(YR == YL + 1) {
					const auto PL = entries[x].second;
					const auto PR = entries[y].second;
					const auto& L_meta = M_tmp[PL];
					const auto& R_meta = M_tmp[PR];

					uint32_t hash[16] = {};
					{
						uint32_t msg[N_META * 2] = {};
						::memcpy(msg, id.data(), id.size());
						for(int i = 0; i < N_META; ++i) {
							msg[i] = L_meta[i];
							msg[N_META + i] = R_meta[i];
						}
						const hash_512_t tmp(&msg, sizeof(msg));

						::memcpy(hash, tmp.data(), tmp.size());
					}
					uint32_t Y_i = 0;
					std::array<uint32_t, N_META> meta = {};
					for(int i = 0; i < N_META; ++i) {
						Y_i = Y_i ^ hash[i];
						meta[i] = hash[i] & kmask;
					}
					Y_i &= kmask;

					matches.emplace_back(Y_i, M_next.size());

					if(X_out) {
						LR_tmp[t].emplace_back(PL, PR);
					}
					M_next.push_back(meta);
				}
				else if(YR > YL) {
					break;
				}
			}
		}

		if(matches.empty()) {
			throw std::logic_error("zero matches at table " + std::to_string(t));
		}
		M_tmp = std::move(M_next);
		entries = std::move(matches);

//		std::cout << "Table " << t << " took " << (vnx::get_time_millis() - time_begin) << " ms, " << entries.size() << " entries" << std::endl;
	}

	std::sort(entries.begin(), entries.end());

	std::vector<std::pair<uint32_t, bytes_t<META_BYTES_OUT>>> out;
	for(const auto& entry : entries)
	{
		if(X_out) {
			std::vector<uint32_t> I_tmp;
			I_tmp.push_back(std::get<1>(entry));

			for(int t = N_TABLE; t >= 2; --t)
			{
				std::vector<uint32_t> I_next;
				for(const auto i : I_tmp) {
					I_next.push_back(LR_tmp[t][i].first);
					I_next.push_back(LR_tmp[t][i].second);
				}
				I_tmp = std::move(I_next);
			}
			for(const auto i : I_tmp) {
				X_out->push_back(X_tmp[i]);
			}
		}
		const auto& meta = M_tmp[entry.second];
		out.emplace_back(entry.first, bytes_t<META_BYTES_OUT>(meta.data(), META_BYTES_OUT));
	}
	return out;
}

hash_t verify(const std::vector<uint32_t>& X_values, const hash_t& challenge, const hash_t& id, const int plot_filter, const int ksize)
{
	if(X_values.size() != (1 << (N_TABLE - 1))) {
		throw std::logic_error("invalid proof size");
	}
	std::vector<uint32_t> X_out;
	const auto entries = compute(X_values, &X_out, id, ksize, 0);
	if(entries.empty()) {
		throw std::logic_error("invalid proof");
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
	// proof output needs to be hashed after challenge, otherwise compression to 256-bit is possible
	return hash_t(std::string("proof_quality") + challenge + result.second);
}




} // pos
} // mmx
