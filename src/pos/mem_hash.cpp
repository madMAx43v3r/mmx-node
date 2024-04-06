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

static const uint32_t MEM_HASH_INIT[16] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174
};

void gen_mem_array(uint32_t* mem, const uint8_t* key, const uint32_t mem_size)
{
	if(mem_size % 32) {
		throw std::logic_error("mem_size % 32 != 0");
	}
	uint32_t state[32] = {};

	for(int i = 0; i < 16; ++i) {
		::memcpy(&state[i], key + i * 4, 4);
	}
	for(int i = 0; i < 16; ++i) {
		state[16 + i] = MEM_HASH_INIT[i];
	}

	uint32_t b = 0;
	uint32_t c = 0;

	for(uint32_t i = 0; i < mem_size; i += 32)
	{
		for(int j = 0; j < 4; ++j) {
			for(int k = 0; k < 16; ++k) {
				MMXPOS_HASHROUND(state[k], b, c, state[16 + k]);
			}
		}
		for(int k = 0; k < 32; ++k) {
			mem[i + k] = state[k];
		}
	}
}

void calc_mem_hash(uint32_t* mem, uint8_t* hash, const int num_iter)
{
	static constexpr int N = 32;

	uint32_t state[N];
	for(int i = 0; i < N; ++i) {
		state[i] = mem[(N - 1) * N + i];
	}

	for(int iter = 0; iter < num_iter; ++iter)
	{
		uint32_t sum = 0;
		for(int i = 0; i < N; ++i) {
			sum += rotl_32(state[i], i % 32);
		}
		const uint32_t dir = sum + (sum << 11) + (sum << 22);

		const uint32_t bits = (dir >> 22) % 32u;
		const uint32_t offset = (dir >> 27);

		for(int i = 0; i < N; ++i)
		{
			state[i] += rotl_32(mem[offset * N + (iter + i) % N], bits) ^ sum;
		}
		for(int i = 0; i < N; ++i)
		{
			mem[offset * N + i] ^= state[i];
		}
	}

	::memcpy(hash, state, N * 4);
}


} // pos
} // mmx
