/*
 * mem_hash.cpp
 *
 *  Created on: Oct 30, 2023
 *      Author: mad
 */

#include <mmx/pos/mem_hash.h>

#include <map>
#include <iostream>
#include <stdexcept>

static const uint32_t K[8] = {
	0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5,
	0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5
};


namespace mmx {
namespace pos {

void gen_mem_array(uint32_t* mem, const uint8_t* key, const uint64_t mem_size)
{
	if(mem_size % 16) {
		throw std::logic_error("mem_size % 16 != 0");
	}
	uint32_t x[16];

	for(int i = 0; i < 8; ++i) {
		x[i] = K[i];
	}
	for(int i = 0; i < 8; ++i)
	{
		uint32_t tmp = 0;
		for(int k = 0; k < 4; ++k) {
			tmp = (tmp << 8) | key[i * 4 + k];
		}
		x[8 + i] = tmp;
	}

	for(uint64_t i = 0; i < mem_size; i += 16)
	{
		MMXPOS_QUARTERROUND(x[0], x[4], x[8], x[12]);
		MMXPOS_QUARTERROUND(x[1], x[5], x[9], x[13]);
		MMXPOS_QUARTERROUND(x[2], x[6], x[10], x[14]);
		MMXPOS_QUARTERROUND(x[3], x[7], x[11], x[15]);

		for(int k = 0; k < 16; ++k) {
			mem[i + k] = x[k];
		}

		x[4] = x[5];
		x[5] = x[6];
		x[6] = x[7];
		x[7] = x[4];

		x[8] = x[10];
		x[9] = x[11];
		x[10] = x[8];
		x[11] = x[9];

		x[12] = x[15];
		x[13] = x[12];
		x[14] = x[13];
		x[15] = x[14];

//		MMXPOS_QUARTERROUND(x[0], x[5], x[10], x[15]);
//		MMXPOS_QUARTERROUND(x[1], x[6], x[11], x[12]);
//		MMXPOS_QUARTERROUND(x[2], x[7], x[8], x[13]);
//		MMXPOS_QUARTERROUND(x[3], x[4], x[9], x[14]);
	}
}

void calc_mem_hash(uint32_t* mem, uint8_t* hash, const int M, const int B)
{
	static constexpr int N = 16;

	const int num_iter = (1 << M);
	const uint32_t index_mask = ((uint32_t(1) << B) - 1);

	uint32_t state[N];
	for(int i = 0; i < N; ++i) {
		state[i] = mem[index_mask * N + i];
	}
//	std::map<uint32_t, uint32_t> count;

	for(int k = 0; k < num_iter; ++k)
	{
		auto tmp = state[0];
		for(int i = 1; i < N; ++i) {
			tmp = rotl_32(tmp, 7) + state[i];
		}
		const auto bits = tmp % 32;
		const auto offset = ((tmp >> 5) & index_mask) * N;
//		count[offset]++;

		for(int i = 0; i < N; ++i) {
			state[i] += rotl_32(mem[offset + (k + i) % N], bits);
		}
		for(int i = 0; i < N; ++i) {
			mem[offset + i] = state[i];
		}
	}

	for(int i = 0; i < 4; ++i)
	{
		MMXPOS_QUARTERROUND(state[0], state[4], state[8], state[12]);
		MMXPOS_QUARTERROUND(state[1], state[5], state[9], state[13]);
		MMXPOS_QUARTERROUND(state[2], state[6], state[10], state[14]);
		MMXPOS_QUARTERROUND(state[3], state[7], state[11], state[15]);

		MMXPOS_QUARTERROUND(state[0], state[5], state[10], state[15]);
		MMXPOS_QUARTERROUND(state[1], state[6], state[11], state[12]);
		MMXPOS_QUARTERROUND(state[2], state[7], state[8], state[13]);
		MMXPOS_QUARTERROUND(state[3], state[4], state[9], state[14]);
	}

//	for(const auto& entry : count) {
//		if(entry.second > uint32_t(3 << (M - B))) {
//			std::cout << "WARN [" << entry.first << "] " << entry.second << std::endl;
//		}
//	}

	for(int i = 0; i < N; ++i) {
		for(int k = 0; k < 4; ++k) {
			hash[i * 4 + k] = state[i] >> (24 - k * 8);
		}
	}
}


} // pos
} // mmx
