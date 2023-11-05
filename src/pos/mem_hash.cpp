/*
 * mem_hash.cpp
 *
 *  Created on: Oct 30, 2023
 *      Author: mad
 */

#include <mmx/pos/mem_hash.h>

#include <map>
#include <cstring>
#include <iostream>
#include <stdexcept>


namespace mmx {
namespace pos {

static const uint32_t MEM_HASH_INIT[8] = {
	0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

void gen_mem_array(uint32_t* mem, const uint8_t* key, const uint64_t mem_size)
{
	if(mem_size % 16) {
		throw std::logic_error("mem_size % 16 != 0");
	}
	uint32_t state[16];

	for(int i = 0; i < 8; ++i) {
		state[i] = MEM_HASH_INIT[i];
	}
	for(int i = 0; i < 8; ++i) {
		::memcpy(state + 8 + i, key + i * 4, 4);
	}

	for(uint64_t i = 0; i < mem_size; i += 16)
	{
		for(int k = 0; k < 16; ++k) {
			MMXPOS_QUARTERROUND(state[0], state[4], state[8], state[12]);
			MMXPOS_QUARTERROUND(state[1], state[5], state[9], state[13]);
			MMXPOS_QUARTERROUND(state[2], state[6], state[10], state[14]);
			MMXPOS_QUARTERROUND(state[3], state[7], state[11], state[15]);

			MMXPOS_QUARTERROUND(state[0], state[5], state[10], state[15]);
			MMXPOS_QUARTERROUND(state[1], state[6], state[11], state[12]);
			MMXPOS_QUARTERROUND(state[2], state[7], state[8], state[13]);
			MMXPOS_QUARTERROUND(state[3], state[4], state[9], state[14]);
		}

		for(int k = 0; k < 16; ++k) {
			mem[i + k] = state[k];
		}
	}
}

void calc_mem_hash(uint32_t* mem, uint8_t* hash, const int M, const int B)
{
	static constexpr int N = 32;

	const int num_iter = (1 << M);
	const uint32_t index_mask = ((1 << B) - 1);

	uint32_t state[N];
	for(int i = 0; i < N; ++i) {
		state[i] = mem[index_mask * N + i];
	}
//	std::map<uint32_t, uint32_t> count;

	for(int k = 0; k < num_iter; ++k)
	{
		uint32_t tmp = 0;
		for(int i = 0; i < N; ++i) {
			tmp += rotl_32(state[i] ^ 0x55555555, (k + i) % N);
		}
		tmp ^= 0x55555555;

		const auto bits = tmp % 32;
//		const auto offset = ((tmp >> 5) & index_mask) * N;
		const auto offset = tmp & (index_mask << 5);
//		count[offset]++;

		for(int i = 0; i < N; ++i) {
			const int shift = (k + i) % N;
			state[i] += rotl_32(mem[offset + shift] ^ state[i], bits);
		}
		for(int i = 0; i < N; ++i) {
			mem[offset + i] = state[i];
		}
	}

//	for(const auto& entry : count) {
//		if(entry.second > uint32_t(3 << (M - B))) {
//			std::cout << "WARN [" << entry.first << "] " << entry.second << std::endl;
//		}
//	}

	for(int i = 0; i < 8; ++i) {
		for(int k = 0; k < 4; ++k) {
			hash[i * 4 + k] = state[i] >> (24 - k * 8);
		}
	}
}


} // pos
} // mmx
