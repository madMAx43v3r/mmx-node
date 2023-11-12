/*
 * verify.cpp
 *
 *  Created on: Nov 5, 2023
 *      Author: mad
 */

#include <mmx/pos/verify.h>
#include <mmx/pos/mem_hash.h>
#include <mmx/hash_t.hpp>

#include <algorithm>
#include <vnx/vnx.h>
#include <vnx/ThreadPool.h>


namespace mmx {
namespace pos {

static constexpr int MEM_HASH_N = 32;
static constexpr int MEM_HASH_M = 8;
static constexpr int MEM_HASH_B = 5;

static constexpr uint64_t MEM_SIZE = uint64_t(MEM_HASH_N) << MEM_HASH_B;

static std::mutex g_mutex;
static std::shared_ptr<vnx::ThreadPool> g_threads;


void compute_f1(std::vector<uint32_t>* X_tmp,
				std::vector<std::array<uint32_t, N_META>>& M_tmp,
				std::vector<std::pair<uint64_t, uint32_t>>& entries,
				std::mutex& mutex,
				const uint32_t X,
				const uint8_t* id, const int ksize, const int xbits)
{
	const uint32_t kmask = ((uint64_t(1) << ksize) - 1);

	std::vector<uint32_t> mem_buf(MEM_SIZE);

	for(uint32_t x_i = 0; x_i < (uint64_t(1) << xbits); ++x_i)
	{
		const uint32_t X_i = (X << xbits) | x_i;

		uint32_t msg[9] = {};
		msg[0] = X_i;
		::memcpy(msg + 1, id, 32);

		const hash_t key(&msg, 36);
		gen_mem_array(mem_buf.data(), key.data(), MEM_SIZE);

		uint8_t mem_hash[128 + 36] = {};
		calc_mem_hash(mem_buf.data(), mem_hash, MEM_HASH_M, MEM_HASH_B);

		::memcpy(mem_hash + 128, msg, 36);

		const hash_t mem_hash_hash(mem_hash, sizeof(mem_hash));

		uint32_t hash[16] = {};
		::memcpy(hash, hash_t(mem_hash_hash.data(), 32).data(), 32);
		::memcpy(hash + 8, hash_t(hash, 32).data(), 32);

		std::array<uint32_t, N_META> meta = {};
		for(int i = 0; i < N_META; ++i) {
			meta[i] = hash[i] & kmask;
		}
		const uint32_t Y_i = hash[N_META] & kmask;

		std::lock_guard<std::mutex> lock(mutex);
		if(X_tmp) {
			X_tmp->push_back(X_i);
		}
		entries.emplace_back(Y_i, M_tmp.size());
		M_tmp.push_back(meta);
	}
}

std::vector<std::pair<uint64_t, bytes_t<META_BYTES>>>
compute(const std::vector<uint32_t>& X_values, std::vector<uint32_t>* X_out, const uint8_t* id, const int ksize, const int xbits)
{
	if(ksize < 8 || ksize > 32) {
		throw std::logic_error("invalid ksize");
	}
	if(xbits > ksize - 8 && xbits != ksize) {
		throw std::logic_error("invalid xbits");
	}
	const bool use_threads = (xbits > 6);

	if(use_threads) {
		std::lock_guard<std::mutex> lock(g_mutex);
		if(!g_threads) {
			const auto cpu_threads = std::thread::hardware_concurrency();
			const auto num_threads = cpu_threads > 0 ? cpu_threads : 16;
			g_threads = std::make_shared<vnx::ThreadPool>(num_threads, 1024);
			vnx::log_info() << "Using " << num_threads << " CPU threads (for proof compute)";
		}
	}
	const uint32_t kmask = ((uint64_t(1) << ksize) - 1);
	const uint64_t ymask = ((uint64_t(1) << (ksize + Y_EXTRABITS)) - 1);
	const uint32_t num_entries_1 = X_values.size() << xbits;

	std::mutex mutex;
	std::vector<int64_t> jobs;
	std::vector<uint32_t> X_tmp;
	std::vector<uint32_t> mem_buf(MEM_SIZE);
	std::vector<std::array<uint32_t, N_META>> M_tmp;
	std::vector<std::pair<uint64_t, uint32_t>> entries;
	std::vector<std::vector<std::pair<uint32_t, uint32_t>>> LR_tmp(N_TABLE - 1);

//	const auto t1_begin = vnx::get_time_millis();

	if(X_out) {
		X_tmp.reserve(num_entries_1);
	}
	M_tmp.reserve(num_entries_1);
	entries.reserve(num_entries_1);

	for(const auto X : X_values)
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
		std::vector<std::pair<uint64_t, uint32_t>> matches;

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
						uint32_t msg[2 + 2 * N_META] = {};
						msg[0] = t;
						msg[1] = YL;

						for(int i = 0; i < N_META; ++i) {
							msg[2 + i] = L_meta[i];
						}
						for(int i = 0; i < N_META; ++i) {
							msg[2 + N_META + i] = R_meta[i];
						}
						const hash_t first(&msg, sizeof(msg));

						::memcpy(hash, hash_t(first.data(), 32).data(), 32);
						::memcpy(hash + 8, hash_t(hash, 32).data(), 32);
					}
					std::array<uint32_t, N_META> meta = {};
					for(int i = 0; i < N_META; ++i) {
						meta[i] = hash[i] & kmask;
					}
					const uint64_t Y_i = ((uint64_t(hash[N_META + 1]) << 32) | hash[N_META]) & (t < N_TABLE ? kmask : ymask);

					matches.emplace_back(Y_i, M_next.size());

					if(X_out) {
						LR_tmp[t-2].emplace_back(PL, PR);
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

	std::vector<std::pair<uint64_t, bytes_t<META_BYTES>>> out;
	for(const auto& entry : entries)
	{
		if(X_out) {
			std::vector<uint32_t> I_tmp;
			I_tmp.push_back(std::get<1>(entry));

			for(int k = N_TABLE - 2; k >= 0; --k)
			{
				std::vector<uint32_t> I_next;
				for(const auto i : I_tmp) {
					I_next.push_back(LR_tmp[k][i].first);
					I_next.push_back(LR_tmp[k][i].second);
				}
				I_tmp = std::move(I_next);
			}
			for(const auto i : I_tmp) {
				X_out->push_back(X_tmp[i]);
			}
		}
		const auto& meta = M_tmp[entry.second];
		out.emplace_back(entry.first, bytes_t<META_BYTES>(meta.data(), sizeof(meta)));
	}
	return out;
}

hash_t verify(const std::vector<uint32_t>& X_values, const hash_t& challenge, const uint8_t* id, const int ksize, const int ycount)
{
	if(ycount < 0) {
		throw std::logic_error("invalid ycount");
	}
	std::vector<uint32_t> X_out;
	const auto entries = compute(X_values, &X_out, id, ksize, 0);
	if(entries.empty()) {
		throw std::logic_error("invalid proof");
	}
	const auto& result = entries[0];
	const auto& Y = result.first;

	const uint64_t ymask = ((uint64_t(1) << (ksize + Y_EXTRABITS)) - 1);

	uint64_t Y_0 = 0;
	::memcpy(&Y_0, challenge.data(), 8);
	Y_0 &= ymask;

	const uint64_t Y_end = Y_0 + ycount;

	if(Y < Y_0 || Y >= Y_end) {
		throw std::logic_error("invalid Y output");
	}
	if(X_out != X_values) {
		throw std::logic_error("invalid proof order");
	}
	return hash_t(result.second + challenge);
}




} // pos
} // mmx
