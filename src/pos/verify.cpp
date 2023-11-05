/*
 * verify.cpp
 *
 *  Created on: Nov 5, 2023
 *      Author: mad
 */

#include <mmx/pos/verify.h>
#include <mmx/pos/mem_hash.h>
#include <mmx/hash_t.hpp>


namespace mmx {
namespace pos {

static constexpr int N = 32;
static constexpr int M = 8;
static constexpr int B = 5;

static constexpr uint64_t MEM_SIZE = uint64_t(N) << B;

int get_meta_count(const int table)
{
	if(table < 1) {
		return 0;
	}
	switch(table) {
		case 1: return 2;
		case 2: return 4;
		case 3: return 8;
	}
	return 12;
}

std::vector<std::pair<uint64_t, bytes_t<48>>>
compute(const std::vector<uint64_t>& X_values, std::vector<uint64_t>* X_out, const uint8_t* id, const int ksize, const int xbits)
{
	std::vector<uint32_t> mem_buf(MEM_SIZE);

	std::vector<uint64_t> X_tmp;
	std::vector<std::vector<std::pair<uint32_t, uint32_t>>> LR(8);
	std::vector<std::tuple<uint64_t, uint64_t, std::array<uint64_t, 6>>> entries;

	for(const uint64_t X : X_values)
	{
		for(uint32_t i = 0; i < (uint32_t(1) << xbits); ++i)
		{
			const uint64_t X_i = (X << xbits) | i;

			if(X_out) {
				X_tmp.push_back(X_i);
			}
			uint64_t msg[5] = {};
			msg[0] = bswap_64(X_i);
			::memcpy(msg + 1, id, 32);

			const hash_t hash(&msg, 8 + 32);
			gen_mem_array(mem_buf.data(), hash.data(), MEM_SIZE);

			uint64_t mem_hash[4] = {};
			calc_mem_hash(mem_buf.data(), (uint8_t*)hash, M, B);

			const uint64_t Y_i = read_bits(mem_hash, 0, ksize);

			std::array<uint64_t, 6> meta = {};
			for(int k = 0; k < get_meta_count(1); ++k)
			{
				const auto tmp = read_bits(mem_hash, (k + 1) * ksize, ksize);
				write_bits(meta.data(), tmp, k * ksize, ksize);
			}
			entries.emplace_back(Y_i, X_tmp.size(), meta);
		}
	}

	for(int t = 2; t <= 9; ++t)
	{
		std::vector<std::pair<uint64_t, bytes_t<48>>> matches;

		const int meta_count_in = get_meta_count(t - 1);
		const int meta_count_out = get_meta_count(t);

		std::sort(entries.begin(), entries.end());

		for(auto iter = entries.begin(); iter != entries.end(); ++iter)
		{
			const uint64_t YL = std::get<0>(*iter);
			const uint64_t PL = std::get<1>(*iter);
			const uint64_t* L_meta = (const uint64_t*)std::get<2>(*iter).data();

			for(auto iter2 = iter; iter2 != entries.end(); ++iter2)
			{
				const uint64_t YR = std::get<0>(*iter2);
				const uint64_t PR = std::get<1>(*iter2);
				const uint64_t* R_meta = (const uint64_t*)std::get<2>(*iter2).data();

				if(YR == YL + 1) {
					uint64_t hash[8] = {};

					for(int i = 0; i < (t < 3 ? 1 : 2); ++i)
					{
						uint64_t msg[16] = {};
						auto bit_offset = write_bits(msg, (t << 4) | i, 0, 8);

						bit_offset = write_bits(msg, YL, bit_offset, ksize);

						for(int k = 0; k < meta_count_in; ++k)
						{
							const auto tmp = read_bits(L_meta, k * ksize, ksize);
							bit_offset = write_bits(msg, tmp, bit_offset, ksize);
						}
						for(int k = 0; k < meta_count_in; ++k)
						{
							const auto tmp = read_bits(R_meta, k * ksize, ksize);
							bit_offset = write_bits(msg, tmp, bit_offset, ksize);
						}
						const hash_t hash_i(&msg, (bit_offset + 7) / 8);

						::memcpy(hash + i * 4, hash_i.data(), 32);
					}
					const uint64_t Y_new = read_bits(hash, 0, ksize);

					uint64_t bit_offset = 0;
					std::array<uint64_t, 6> meta = {};

					for(int k = 0; k < meta_count_out; ++k)
					{
						const auto tmp = read_bits(hash, (k + 1) * ksize, ksize);
						bit_offset = write_bits(meta.data(), tmp, bit_offset, ksize);
					}
					matches.emplace_back(Y_new, LR[t-2].size(), meta);

					if(X_out) {
						LR[t-2].emplace_back(PL, PR);
					}
				}
				else if(YR > YL) {
					break;
				}
			}
		}

		if(matches.empty()) {
			throw std::logic_error("zero matches at table " + std::to_string(t));
		}
		entries = std::move(matches);
	}

	std::vector<std::pair<uint64_t, bytes_t<48>>> out;
	for(const auto& entry : entries)
	{
		if(X_out) {
			std::vector<uint32_t> I_tmp;
			I_tmp.push_back(std::get<1>(entry));

			for(int t = 7; t >= 0; --t)
			{
				std::vector<uint32_t> I_next;
				for(const auto i : I_tmp) {
					I_next.push_back(LR[t][i].first);
					I_next.push_back(LR[t][i].second);
				}
				I_tmp = std::move(I_next);
			}
			X_out->reserve(I_tmp.size());

			for(const auto i : I_tmp) {
				X_out->push_back(X_tmp[i]);
			}
		}
		const auto& Y = std::get<0>(entry);
		const auto& meta = std::get<2>(entry);
		out.emplace_back(Y, bytes_t<48>(meta.data(), sizeof(meta)));
	}
	return out;
}

hash_t verify(const std::vector<uint64_t>& X_values, const uint8_t* id, const hash_t& challenge, const int ksize)
{
	const auto entries = compute(X_values, nullptr, id, ksize, 0);

	if(entries.empty()) {
		throw std::logic_error("invalid proof");
	}
	const auto& result = entries[0];

	uint64_t tmp[4] = {};
	::memcpy(tmp, challenge.data(), 32);

	const auto Y_challenge = read_bits(tmp, 0, ksize);
	if(result.first != Y_challenge) {
		throw std::logic_error("Y output value != challenge");
	}
	return hash_t(std::string("proof_quality") + result.second + challenge);
}




} // pos
} // mmx
