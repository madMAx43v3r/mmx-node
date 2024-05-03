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
#ifdef _MSC_VER
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

#define MMXPOS_HASHROUND(a, b, c, d) \
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

template<typename Int, typename Int2>
constexpr inline Int cdiv(const Int& a, const Int2& b) {
	return (a + b - 1) / b;
}

inline uint32_t rotl_32(const uint32_t v, int bits)
{
#ifdef _MSC_VER
	return _rotl(v, bits);
#else
	return (v << bits) | (v >> (32 - bits));
#endif
}

inline uint64_t rotl_64(const uint64_t v, int bits)
{
#ifdef _MSC_VER
	return _rotl64(v, bits);
#else
	return (v << bits) | (v >> (64 - bits));
#endif
}

inline
uint64_t write_bits(uint64_t* dst, uint64_t value, const uint64_t bit_offset, const uint32_t num_bits)
{
	if(num_bits < 64) {
		value &= ((uint64_t(1) << num_bits) - 1);
	}
	const uint32_t shift = bit_offset % 64;
	const uint32_t free_bits = 64 - shift;

	dst[bit_offset / 64]         |= (value << shift);

	if(free_bits < num_bits) {
		dst[bit_offset / 64 + 1] |= (value >> free_bits);
	}
	return bit_offset + num_bits;
}

inline
uint64_t read_bits(const uint64_t* src, const uint64_t bit_offset, const uint32_t num_bits)
{
	uint32_t count = 0;
	uint64_t offset = bit_offset;
	uint64_t result = 0;
	while(count < num_bits) {
		const uint32_t shift = offset % 64;
		const uint32_t bits = std::min<uint32_t>(num_bits - count, 64 - shift);
		const uint64_t value = src[offset / 64] >> shift;
		result |= value << count;
		count += bits;
		offset += bits;
	}
	if(num_bits < 64) {
		result &= ((uint64_t(1) << num_bits) - 1);
	}
	return result;
}


} // pos
} // mmx

#endif /* INCLUDE_MMX_POS_UTIL_H_ */
