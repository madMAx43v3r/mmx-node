/*
 * verify.cpp
 *
 *  Created on: Nov 5, 2023
 *      Author: mad
 */

#include <mmx/pos/verify.h>
#include <mmx/pos/mem_hash.h>
#include <mmx/hash_t.hpp>
#include <tuple>


namespace mmx {
namespace pos {

static constexpr int N_META = 12;
static constexpr int N_TABLE = 9;
static constexpr int MEM_HASH_N = 32;
static constexpr int MEM_HASH_M = 8;
static constexpr int MEM_HASH_B = 5;

static constexpr uint64_t MEM_SIZE = uint64_t(MEM_HASH_N) << MEM_HASH_B;


std::vector<std::pair<uint32_t, bytes_t<48>>>
compute(const std::vector<uint32_t>& X_values, std::vector<uint32_t>* X_out, const uint8_t* id, const int ksize, const int xbits)
{
	if(ksize < 8 || ksize > 32) {
		throw std::logic_error("invalid ksize");
	}
	if(xbits > ksize) {
		throw std::logic_error("invalid xbits");
	}
	std::vector<uint32_t> mem_buf(MEM_SIZE);

	std::vector<uint32_t> X_tmp;
	std::vector<std::array<uint32_t, N_META>> M_tmp;
	std::vector<std::tuple<uint32_t, uint32_t>> entries;
	std::vector<std::vector<std::pair<uint32_t, uint32_t>>> LR(N_TABLE - 1);

	const uint32_t kmask = ((uint32_t(1) << ksize) - 1);
	const uint32_t num_entries_1 = X_values.size() << xbits;

	if(X_out) {
		X_tmp.reserve(num_entries_1);
	}
	M_tmp.reserve(num_entries_1);

	for(const uint32_t X : X_values)
	{
		for(uint32_t i = 0; i < (uint32_t(1) << xbits); ++i)
		{
			const uint32_t X_i = (X << xbits) | i;

			if(X_out) {
				X_tmp.push_back(X_i);
			}
			uint32_t msg[9] = {};
			msg[0] = X_i;
			::memcpy(msg + 1, id, 32);

			const hash_t hash(&msg, 4 + 32);
			gen_mem_array(mem_buf.data(), hash.data(), MEM_SIZE);

			uint32_t mem_hash[16] = {};
			calc_mem_hash(mem_buf.data(), (uint8_t*)mem_hash, MEM_HASH_M, MEM_HASH_B);

			const uint32_t Y_i = mem_hash[0] & kmask;

			std::array<uint32_t, N_META> meta = {};
			for(int k = 0; k < N_META; ++k) {
				meta[k] = mem_hash[1 + k] & kmask;
			}
			entries.emplace_back(Y_i, M_tmp.size());
			M_tmp.push_back(meta);
		}
	}

	for(int t = 2; t <= N_TABLE; ++t)
	{
		std::vector<std::array<uint32_t, N_META>> M_next;
		std::vector<std::tuple<uint32_t, uint32_t>> matches;

		const size_t num_entries = entries.size();

		std::sort(entries.begin(), entries.end());

		for(size_t x = 0; x < num_entries; ++x)
		{
			const auto YL = std::get<0>(entries[x]);

			for(size_t y = x + 1; y < num_entries; ++y)
			{
				const auto YR = std::get<0>(entries[y]);

				if(YR == YL + 1) {
					const auto PL = std::get<1>(entries[x]);
					const auto PR = std::get<1>(entries[y]);
					const auto& L_meta = M_tmp[PL];
					const auto& R_meta = M_tmp[PR];

					uint32_t hash[16] = {};

					for(int i = 0; i < 2; ++i)
					{
						uint32_t msg[2 + 2 * N_META] = {};
						msg[0] = (t << 8) | i;
						msg[1] = YL;

						for(int k = 0; k < N_META; ++k) {
							msg[2 + k] = L_meta[k];
						}
						for(int k = 0; k < N_META; ++k) {
							msg[2 + N_META + k] = R_meta[k];
						}
						const hash_t hash_i(&msg, sizeof(msg));

						::memcpy(hash + i * 8, hash_i.data(), 32);
					}
					const uint32_t Y_i = hash[0] & kmask;

					std::array<uint32_t, N_META> meta = {};
					for(int k = 0; k < N_META; ++k) {
						meta[k] = hash[1 + k] & kmask;
					}
					matches.emplace_back(Y_i, M_next.size());

					if(X_out) {
						LR[t-2].emplace_back(PL, PR);
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
	}

	std::vector<std::pair<uint32_t, bytes_t<48>>> out;
	for(const auto& entry : entries)
	{
		if(X_out) {
			std::vector<uint32_t> I_tmp;
			I_tmp.push_back(std::get<1>(entry));

			for(int k = N_TABLE - 2; k >= 0; --k)
			{
				std::vector<uint32_t> I_next;
				for(const auto i : I_tmp) {
					I_next.push_back(LR[k][i].first);
					I_next.push_back(LR[k][i].second);
				}
				I_tmp = std::move(I_next);
			}
			for(const auto i : I_tmp) {
				X_out->push_back(X_tmp[i]);
			}
		}
		const auto& Y = std::get<0>(entry);
		const auto& I = std::get<1>(entry);
		const auto& meta = M_tmp[I];
		out.emplace_back(Y, bytes_t<48>(meta.data(), sizeof(meta)));
	}
	return out;
}

hash_t verify(const std::vector<uint32_t>& X_values, const hash_t& challenge, const uint8_t* id, const int ksize)
{
	std::vector<uint32_t> X_out;
	const auto entries = compute(X_values, &X_out, id, ksize, 0);
	if(entries.empty()) {
		throw std::logic_error("invalid proof");
	}
	const auto& result = entries[0];

	const uint32_t kmask = ((uint32_t(1) << ksize) - 1);

	uint32_t tmp = {};
	::memcpy(&tmp, challenge.data(), sizeof(tmp));

	const auto Y_challenge = tmp & kmask;
	if(result.first != Y_challenge) {
		throw std::logic_error("Y output value != challenge");
	}
	if(X_out != X_values) {
		throw std::logic_error("invalid proof order");
	}
	return hash_t(result.second + challenge);
}




} // pos
} // mmx
