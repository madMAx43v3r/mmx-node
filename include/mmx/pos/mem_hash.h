/*
 * mem_hash.h
 *
 *  Created on: Oct 29, 2023
 *      Author: mad
 */

#ifndef INCLUDE_MMX_POS_MEM_HASH_H_
#define INCLUDE_MMX_POS_MEM_HASH_H_

#include <cstdint>


namespace mmx {
namespace pos {

inline uint32_t rotl_32(const uint32_t v, int bits) {
	return (v << bits) | (v >> (32 - bits));
}

inline uint64_t rotl_64(const uint64_t v, int bits) {
	return (v << bits) | (v >> (64 - bits));
}

/*
 * mem = array of size mem_size
 * key = array of size 32
 */
void gen_mem_array(uint32_t* mem, const uint8_t* key, const uint64_t mem_size);

/*
 * M = number of iterations
 * mem = array of size N * B
 * hash = array of size N * 4
 */
template<int N, int M, int B>
void calc_mem_hash(uint32_t* mem, uint8_t* hash)
{
	uint32_t state[N];
	for(int i = 0; i < N; ++i) {
		state[i] = mem[(B - 1) * N + i];
	}

	for(int k = 0; k < M; ++k)
	{
		auto tmp = state[0];
		for(int i = 1; i < N; ++i) {
			tmp = rotl_32(tmp, 7) + state[i];
		}
		const auto bits = tmp % 32;
		const auto offset = uint64_t((tmp >> 5) % B) * N;

		for(int i = 0; i < N; ++i) {
			state[i] += rotl_32(mem[offset + i], bits);
		}
		for(int i = 0; i < N; ++i) {
			mem[offset + i] = state[(i + 1) % N];
		}
	}

	for(int i = 0; i < N; ++i) {
		for(int k = 0; k < 4; ++k) {
			hash[i * 4 + k] = state[i] >> (24 - k * 8);
		}
	}
}


} // pos
} // mmx

#endif /* INCLUDE_MMX_POS_MEM_HASH_H_ */
