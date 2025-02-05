/*
 * verify_full.cpp
 *
 *  Created on: Feb 5, 2025
 *      Author: mad
 */

#include <mmx/pos/verify.h>
#include <mmx/hash_512_t.hpp>
#include <mmx/utils.h>


namespace mmx {
namespace pos {

std::vector<std::pair<uint32_t, bytes_t<META_BYTES_OUT>>>
compute_full(	const std::vector<uint32_t>& X_in,
				const std::vector<uint32_t>& Y_in,
				std::vector<std::array<uint32_t, N_META>>& M_in,
				std::vector<uint32_t>* X_out,
				const hash_t& id, const int ksize)
{
	if(M_in.size() != Y_in.size()) {
		throw std::logic_error("input length mismatch");
	}
	const uint32_t kmask = ((uint64_t(1) << ksize) - 1);

	std::vector<std::array<uint32_t, N_META>> M_tmp = std::move(M_in);
	std::vector<std::pair<uint32_t, uint32_t>> entries;
	std::vector<std::vector<std::pair<uint32_t, uint32_t>>> LR_tmp(N_TABLE + 1);

	entries.reserve(Y_in.size());
	for(size_t i = 0; i < Y_in.size(); ++i) {
		entries.emplace_back(Y_in[i], i);
	}

	// sort function for proof ordering (enforce unique proofs)
	const auto sort_func =
		[&M_tmp](const std::pair<uint32_t, uint32_t>& L, const std::pair<uint32_t, uint32_t>& R) -> bool {
			if(L.first == R.first) {
				return M_tmp[L.second] < M_tmp[R.second];
			}
			return L < R;
		};

	for(int t = 2; t <= N_TABLE; ++t)
	{
//		const auto time_begin = get_time_ms();

		std::vector<std::array<uint32_t, N_META>> M_next;
		std::vector<std::pair<uint32_t, uint32_t>> matches;

		std::sort(entries.begin(), entries.end(), sort_func);

//		std::cout << "Table " << t << " sort took " << (get_time_ms() - time_begin) << " ms" << std::endl;

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

//		std::cout << "Table " << t << " took " << (get_time_ms() - time_begin) << " ms, " << entries.size() << " entries" << std::endl;
	}

	if(X_out) {
		X_out->clear();
	}
	std::sort(entries.begin(), entries.end(), sort_func);

	std::set<std::array<uint32_t, N_META>> M_set;
	std::vector<std::pair<uint32_t, bytes_t<META_BYTES_OUT>>> out;
	for(const auto& entry : entries)
	{
		const auto& meta = M_tmp[entry.second];
		if(!M_set.insert(meta).second) {
			continue;
		}
		out.emplace_back(entry.first, bytes_t<META_BYTES_OUT>(meta.data(), META_BYTES_OUT));

		if(X_out && X_in.size() == Y_in.size())
		{
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
				X_out->push_back(X_in[i]);
			}
		}
	}
	return out;
}



} // pos
} // mmx
