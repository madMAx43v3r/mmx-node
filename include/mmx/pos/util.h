/*
 * util.h
 *
 *  Created on: Oct 30, 2023
 *      Author: mad
 */

#ifndef INCLUDE_MMX_POS_UTIL_H_
#define INCLUDE_MMX_POS_UTIL_H_

#include <cstdint>
#include <algorithm>

// compiler-specific byte swap macros.
#if defined(_MSC_VER)
	#include <cstdlib>
	// https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/byteswap-uint64-byteswap-ulong-byteswap-ushort?view=msvc-160
	inline uint16_t bswap_16(uint16_t x) { return _byteswap_ushort(x); }
	inline uint32_t bswap_32(uint32_t x) { return _byteswap_ulong(x); }
	inline uint64_t bswap_64(uint64_t x) { return _byteswap_uint64(x); }
#elif defined(__clang__) || defined(__GNUC__)
	inline uint16_t bswap_16(uint16_t x) { return __builtin_bswap16(x); }
	inline uint32_t bswap_32(uint32_t x) { return __builtin_bswap32(x); }
	inline uint64_t bswap_64(uint64_t x) { return __builtin_bswap64(x); }
#else
#error "unknown compiler, don't know how to swap bytes"
#endif

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

inline
uint64_t write_bits(uint64_t* dst, const uint64_t value, const uint64_t bit_offset, const int num_bits)
{
	const int free_bits = 64 - (bit_offset % 64);
	if(free_bits >= num_bits) {
		dst[bit_offset / 64]     |= bswap_64(value << (free_bits - num_bits));
	} else {
		const int suffix_size = num_bits - free_bits;
		const uint64_t suffix = value & ((uint64_t(1) << suffix_size) - 1);
		dst[bit_offset / 64]     |= bswap_64(value >> suffix_size);			// prefix (high bits)
		dst[bit_offset / 64 + 1] |= bswap_64(suffix << (64 - suffix_size));	// suffix (low bits)
	}
	return bit_offset + num_bits;
}

inline
uint64_t read_bits(const uint64_t* src, const uint64_t bit_offset, const int num_bits)
{
	int count = 0;
	uint64_t offset = bit_offset;
	uint64_t result = 0;
	while(count < num_bits) {
		const int shift = offset % 64;
		const int bits = std::min(num_bits - count, 64 - shift);
		uint64_t value = bswap_64(src[offset / 64]) << shift;
		if(bits < 64) {
			value >>= (64 - bits);
		}
		result |= value << (num_bits - count - bits);
		count += bits;
		offset += bits;
	}
	return result;
}


} // pos
} // mmx

#endif /* INCLUDE_MMX_POS_UTIL_H_ */