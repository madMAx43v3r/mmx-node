/*
 * util.h
 *
 *  Created on: Oct 30, 2023
 *      Author: mad
 */

#ifndef INCLUDE_MMX_POS_UTIL_H_
#define INCLUDE_MMX_POS_UTIL_H_

#include <cstdint>

#define MMXPOS_QUARTERROUND(a, b, c, d) \
	a = a + b;              \
	d = rotl_32(d ^ a, 16); \
	c = c + d;              \
	b = rotl_32(b ^ c, 12); \
	a = a + b;              \
	d = rotl_32(d ^ a, 8);  \
	c = c + d;              \
	b = rotl_32(b ^ c, 7);


namespace mmx {
namespace pos {

inline uint32_t rotl_32(const uint32_t v, int bits) {
	return (v << bits) | (v >> (32 - bits));
}

inline uint64_t rotl_64(const uint64_t v, int bits) {
	return (v << bits) | (v >> (64 - bits));
}


} // pos
} // mmx

#endif /* INCLUDE_MMX_POS_UTIL_H_ */
