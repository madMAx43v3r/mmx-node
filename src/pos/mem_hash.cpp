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

void gen_mem_array(uint32_t* mem, const uint8_t* key, const int key_size, const uint32_t mem_size)
{
	if(key_size > 64) {
		throw std::logic_error("key_size > 64");
	}
	if(key_size % 4) {
		throw std::logic_error("key_size % 4 != 0");
	}
	if(mem_size % 16) {
		throw std::logic_error("mem_size % 16 != 0");
	}
	uint32_t state[16];

	for(int i = 0; i < 16; ++i) {
		state[i] = MEM_HASH_INIT[i];
	}

	for(int i = 0; i < key_size / 4; ++i)
	{
		uint32_t tmp = 0;
		::memcpy(&tmp, key + i * 4, 4);
		state[i] ^= tmp;
	}

	for(uint32_t i = 0; i < mem_size; i += 16)
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

void calc_mem_hash(uint32_t* mem, uint8_t* hash, const int num_iter)
{
	static constexpr int N = 32;

	const uint32_t offset_mask = 31u << 5;

	uint32_t state[N];
	for(int i = 0; i < N; ++i) {
		state[i] = mem[31 * N + i];
	}

	for(int k = 0; k < num_iter; ++k)
	{
		uint32_t sum = 0;
		for(int i = 0; i < N; ++i) {
			sum += rotl_32(state[i], (k + i) % 32);
		}
		const uint32_t dir = sum % 1193u;		// mod by prime

		const uint32_t bits = dir % 32u;
		const uint32_t offset = dir & offset_mask;

		for(int i = 0; i < N; ++i)
		{
			state[i] += rotl_32(mem[offset + (k + i) % N], bits) ^ sum;

			mem[offset + i] ^= state[i];
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
