/*
 * mem_hash.cpp
 *
 *  Created on: Oct 30, 2023
 *      Author: mad
 */

#include <mmx/pos/mem_hash.h>

#include <stdexcept>


namespace mmx {
namespace pos {

#define QUARTERROUND(a, b, c, d) \
	a = a + b;              \
	d = rotl_32(d ^ a, 16); \
	c = c + d;              \
	b = rotl_32(b ^ c, 12); \
	a = a + b;              \
	d = rotl_32(d ^ a, 8);  \
	c = c + d;              \
	b = rotl_32(b ^ c, 7);

void gen_mem_array(uint32_t* mem, const uint8_t* key, const uint64_t mem_size)
{
	if(mem_size % 16) {
		throw std::logic_error("mem_size % 16 != 0");
	}
	uint32_t x[16] = {};

	for(int i = 0; i < 16; ++i)
	{
		uint32_t tmp = 0;
		for(int k = 0; k < 4; ++k) {
			tmp = (tmp << 8) | key[(i * 4 + k) % 32];
		}
		x[i] = rotl_32(tmp, i * 2 + 1);
	}

	for(uint64_t i = 0; i < mem_size; i += 16)
	{
		QUARTERROUND(x[0], x[4], x[8], x[12]);
		QUARTERROUND(x[1], x[5], x[9], x[13]);
		QUARTERROUND(x[2], x[6], x[10], x[14]);
		QUARTERROUND(x[3], x[7], x[11], x[15]);

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

//		QUARTERROUND(x[0], x[5], x[10], x[15]);
//		QUARTERROUND(x[1], x[6], x[11], x[12]);
//		QUARTERROUND(x[2], x[7], x[8], x[13]);
//		QUARTERROUND(x[3], x[4], x[9], x[14]);
	}
}


} // pos
} // mmx
