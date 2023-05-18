/*
 * sha256_ni.cpp
 *
 *  Created on: Jul 19, 2022
 *      Author: mad
 */

#include <sha256_ni.h>

#include <cstring>
#include <stdexcept>

#if defined(__SHA__) || defined(_WIN32)

#include <immintrin.h>

#ifdef _WIN32
#include <intrin.h>
#define cpuid(info, x)    __cpuidex(info, x, 0)
#else
#include <cpuid.h>
inline void cpuid(int info[4], int InfoType) {
	__cpuid_count(InfoType, 0, info[0], info[1], info[2], info[3]);
}
#endif

alignas(64)
static const uint32_t K[] = {
	0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5,
	0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
	0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3,
	0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
	0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC,
	0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
	0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7,
	0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
	0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13,
	0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
	0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3,
	0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
	0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5,
	0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
	0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208,
	0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2,
};

inline uint32_t bswap_32(const uint32_t val) {
	return ((val & 0xFF) << 24) | ((val & 0xFF00) << 8) | ((val & 0xFF0000) >> 8) | ((val & 0xFF000000) >> 24);
}

inline uint64_t bswap_64(const uint64_t val) {
	return (uint64_t(bswap_32(val)) << 32) | bswap_32(val >> 32);
}

static void compress_digest(uint32_t* state, const uint8_t* input, size_t blocks)
{
	const __m128i* K_mm = reinterpret_cast<const __m128i*>(K);

	const __m128i* input_mm = reinterpret_cast<const __m128i*>(input);
	const __m128i MASK = _mm_set_epi64x(0x0c0d0e0f08090a0b, 0x0405060700010203);

	// Load initial values
	__m128i STATE0 = _mm_loadu_si128(reinterpret_cast<__m128i*>(&state[0]));
	__m128i STATE1 = _mm_loadu_si128(reinterpret_cast<__m128i*>(&state[4]));

	STATE0 = _mm_shuffle_epi32(STATE0, 0xB1); // CDAB
	STATE1 = _mm_shuffle_epi32(STATE1, 0x1B); // EFGH

	__m128i TMP = _mm_alignr_epi8(STATE0, STATE1, 8); // ABEF
	STATE1 = _mm_blend_epi16(STATE1, STATE0, 0xF0); // CDGH
	STATE0 = TMP;

	while(blocks > 0)
	{
		// Save current state
		const __m128i ABEF_SAVE = STATE0;
		const __m128i CDGH_SAVE = STATE1;

		__m128i MSG;

		__m128i TMSG0 = _mm_shuffle_epi8(_mm_loadu_si128(input_mm), MASK);
		__m128i TMSG1 = _mm_shuffle_epi8(_mm_loadu_si128(input_mm + 1), MASK);
		__m128i TMSG2 = _mm_shuffle_epi8(_mm_loadu_si128(input_mm + 2), MASK);
		__m128i TMSG3 = _mm_shuffle_epi8(_mm_loadu_si128(input_mm + 3), MASK);

		// Rounds 0-3
		MSG = _mm_add_epi32(TMSG0, _mm_load_si128(K_mm));
		STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
		STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

		// Rounds 4-7
		MSG = _mm_add_epi32(TMSG1, _mm_load_si128(K_mm + 1));
		STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
		STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

		TMSG0 = _mm_sha256msg1_epu32(TMSG0, TMSG1);

		// Rounds 8-11
		MSG = _mm_add_epi32(TMSG2, _mm_load_si128(K_mm + 2));
		STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
		STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

		TMSG1 = _mm_sha256msg1_epu32(TMSG1, TMSG2);

		// Rounds 12-15
		MSG = _mm_add_epi32(TMSG3, _mm_load_si128(K_mm + 3));
		STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
		STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

		TMSG0 = _mm_add_epi32(TMSG0, _mm_alignr_epi8(TMSG3, TMSG2, 4));
		TMSG0 = _mm_sha256msg2_epu32(TMSG0, TMSG3);
		TMSG2 = _mm_sha256msg1_epu32(TMSG2, TMSG3);

		// Rounds 16-19
		MSG = _mm_add_epi32(TMSG0, _mm_load_si128(K_mm + 4));
		STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
		STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

		TMSG1 = _mm_add_epi32(TMSG1, _mm_alignr_epi8(TMSG0, TMSG3, 4));
		TMSG1 = _mm_sha256msg2_epu32(TMSG1, TMSG0);
		TMSG3 = _mm_sha256msg1_epu32(TMSG3, TMSG0);

		// Rounds 20-23
		MSG = _mm_add_epi32(TMSG1, _mm_load_si128(K_mm + 5));
		STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
		STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

		TMSG2 = _mm_add_epi32(TMSG2, _mm_alignr_epi8(TMSG1, TMSG0, 4));
		TMSG2 = _mm_sha256msg2_epu32(TMSG2, TMSG1);
		TMSG0 = _mm_sha256msg1_epu32(TMSG0, TMSG1);

		// Rounds 24-27
		MSG = _mm_add_epi32(TMSG2, _mm_load_si128(K_mm + 6));
		STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
		STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

		TMSG3 = _mm_add_epi32(TMSG3, _mm_alignr_epi8(TMSG2, TMSG1, 4));
		TMSG3 = _mm_sha256msg2_epu32(TMSG3, TMSG2);
		TMSG1 = _mm_sha256msg1_epu32(TMSG1, TMSG2);

		// Rounds 28-31
		MSG = _mm_add_epi32(TMSG3, _mm_load_si128(K_mm + 7));
		STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
		STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

		TMSG0 = _mm_add_epi32(TMSG0, _mm_alignr_epi8(TMSG3, TMSG2, 4));
		TMSG0 = _mm_sha256msg2_epu32(TMSG0, TMSG3);
		TMSG2 = _mm_sha256msg1_epu32(TMSG2, TMSG3);

		// Rounds 32-35
		MSG = _mm_add_epi32(TMSG0, _mm_load_si128(K_mm + 8));
		STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
		STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

		TMSG1 = _mm_add_epi32(TMSG1, _mm_alignr_epi8(TMSG0, TMSG3, 4));
		TMSG1 = _mm_sha256msg2_epu32(TMSG1, TMSG0);
		TMSG3 = _mm_sha256msg1_epu32(TMSG3, TMSG0);

		// Rounds 36-39
		MSG = _mm_add_epi32(TMSG1, _mm_load_si128(K_mm + 9));
		STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
		STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

		TMSG2 = _mm_add_epi32(TMSG2, _mm_alignr_epi8(TMSG1, TMSG0, 4));
		TMSG2 = _mm_sha256msg2_epu32(TMSG2, TMSG1);
		TMSG0 = _mm_sha256msg1_epu32(TMSG0, TMSG1);

		// Rounds 40-43
		MSG = _mm_add_epi32(TMSG2, _mm_load_si128(K_mm + 10));
		STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
		STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

		TMSG3 = _mm_add_epi32(TMSG3, _mm_alignr_epi8(TMSG2, TMSG1, 4));
		TMSG3 = _mm_sha256msg2_epu32(TMSG3, TMSG2);
		TMSG1 = _mm_sha256msg1_epu32(TMSG1, TMSG2);

		// Rounds 44-47
		MSG = _mm_add_epi32(TMSG3, _mm_load_si128(K_mm + 11));
		STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
		STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

		TMSG0 = _mm_add_epi32(TMSG0, _mm_alignr_epi8(TMSG3, TMSG2, 4));
		TMSG0 = _mm_sha256msg2_epu32(TMSG0, TMSG3);
		TMSG2 = _mm_sha256msg1_epu32(TMSG2, TMSG3);

		// Rounds 48-51
		MSG = _mm_add_epi32(TMSG0, _mm_load_si128(K_mm + 12));
		STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
		STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

		TMSG1 = _mm_add_epi32(TMSG1, _mm_alignr_epi8(TMSG0, TMSG3, 4));
		TMSG1 = _mm_sha256msg2_epu32(TMSG1, TMSG0);
		TMSG3 = _mm_sha256msg1_epu32(TMSG3, TMSG0);

		// Rounds 52-55
		MSG = _mm_add_epi32(TMSG1, _mm_load_si128(K_mm + 13));
		STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
		STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

		TMSG2 = _mm_add_epi32(TMSG2, _mm_alignr_epi8(TMSG1, TMSG0, 4));
		TMSG2 = _mm_sha256msg2_epu32(TMSG2, TMSG1);

		// Rounds 56-59
		MSG = _mm_add_epi32(TMSG2, _mm_load_si128(K_mm + 14));
		STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
		STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

		TMSG3 = _mm_add_epi32(TMSG3, _mm_alignr_epi8(TMSG2, TMSG1, 4));
		TMSG3 = _mm_sha256msg2_epu32(TMSG3, TMSG2);

		// Rounds 60-63
		MSG = _mm_add_epi32(TMSG3, _mm_load_si128(K_mm + 15));
		STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
		STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

		// Add values back to state
		STATE0 = _mm_add_epi32(STATE0, ABEF_SAVE);
		STATE1 = _mm_add_epi32(STATE1, CDGH_SAVE);

		input_mm += 4;
		blocks--;
	}

	STATE0 = _mm_shuffle_epi32(STATE0, 0x1B); // FEBA
	STATE1 = _mm_shuffle_epi32(STATE1, 0xB1); // DCHG

	// Save state
	_mm_storeu_si128(reinterpret_cast<__m128i*>(&state[0]), _mm_blend_epi16(STATE0, STATE1, 0xF0)); // DCBA
	_mm_storeu_si128(reinterpret_cast<__m128i*>(&state[4]), _mm_alignr_epi8(STATE1, STATE0, 8)); // ABEF
}

void sha256_ni(uint8_t* out, const uint8_t* in, const uint64_t length)
{
	uint32_t state[] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

	const auto num_blocks = length / 64;
	if(num_blocks) {
		compress_digest(state, in, num_blocks);
	}
	const auto remain = length % 64;
	const auto final_blocks = (remain + 9 + 63) / 64;
	const uint64_t num_bits = bswap_64(length * 8);
	const uint8_t end_bit = 0x80;

	uint8_t last[128] = {};
	::memcpy(last, in + num_blocks * 64, remain);
	::memset(last + remain, 0, sizeof(last) - remain);
	::memcpy(last + remain, &end_bit, 1);
	::memcpy(last + final_blocks * 64 - 8, &num_bits, 8);

	compress_digest(state, last, final_blocks);

	for(int k = 0; k < 8; ++k) {
		state[k] = bswap_32(state[k]);
	}
	::memcpy(out, state, 32);
}

bool sha256_ni_available()
{
//	int a, b, c, d;
//
//	// Look for CPUID.7.0.EBX[29]
//	// EAX = 7, ECX = 0
//	a = 7;
//	c = 0;
//
//	asm volatile ("cpuid"
//		:"=a"(a), "=b"(b), "=c"(c), "=d"(d)
//		:"a"(a), "c"(c)
//	);

	int info[4];
	cpuid(info, 0);
	const int nIds = info[0];

	if(nIds < 7) {
		return false;
	}
	cpuid(info, 7);

	// Intel® SHA Extensions feature bit is EBX[29]
	return ((info[1] >> 29) & 1);
}

#else // __SHA__

void sha256_ni(uint8_t* out, const uint8_t* in, const uint64_t length) {
	throw std::logic_error("sha256_ni() not available");
}

bool sha256_ni_available() {
	return false;
}

#endif // __SHA__

void recursive_sha256_ni(uint8_t* hash, const uint64_t num_iters)
{

        // prerequisite: length is 32 (recursive sha256)
        // optimization: inline rewrite sha256_ni() and compress_digest()

	for(uint64_t i = 0; i < num_iters; ++i) {

		// start: sha256_ni() optimize

        	uint32_t state[] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

        	const uint64_t num_bits = bswap_64(256);
        	const uint8_t end_bit = 0x80;

        	uint8_t last[128] = {};
        	::memcpy(last, hash, 32);
        	::memset(last + 32, 0, sizeof(last) - 32);
        	::memcpy(last + 32, &end_bit, 1);
        	::memcpy(last + 56, &num_bits, 8);

			// start: compress_digest() optimize

			const __m128i* K_mm = reinterpret_cast<const __m128i*>(K);

			const __m128i* input_mm = reinterpret_cast<const __m128i*>(last);
			const __m128i MASK = _mm_set_epi64x(0x0c0d0e0f08090a0b, 0x0405060700010203);

			// Load initial values
			__m128i STATE0 = _mm_loadu_si128(reinterpret_cast<__m128i*>(&state[0]));
			__m128i STATE1 = _mm_loadu_si128(reinterpret_cast<__m128i*>(&state[4]));

			STATE0 = _mm_shuffle_epi32(STATE0, 0xB1); // CDAB
			STATE1 = _mm_shuffle_epi32(STATE1, 0x1B); // EFGH

			__m128i TMP = _mm_alignr_epi8(STATE0, STATE1, 8); // ABEF
			STATE1 = _mm_blend_epi16(STATE1, STATE0, 0xF0); // CDGH
			STATE0 = TMP;

			// Save current state
			const __m128i ABEF_SAVE = STATE0;
			const __m128i CDGH_SAVE = STATE1;

			__m128i MSG;

			__m128i TMSG0 = _mm_shuffle_epi8(_mm_loadu_si128(input_mm), MASK);
			__m128i TMSG1 = _mm_shuffle_epi8(_mm_loadu_si128(input_mm + 1), MASK);
			__m128i TMSG2 = _mm_shuffle_epi8(_mm_loadu_si128(input_mm + 2), MASK);
			__m128i TMSG3 = _mm_shuffle_epi8(_mm_loadu_si128(input_mm + 3), MASK);

			// Rounds 0-3
			MSG = _mm_add_epi32(TMSG0, _mm_load_si128(K_mm));
			STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
			STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

			// Rounds 4-7
			MSG = _mm_add_epi32(TMSG1, _mm_load_si128(K_mm + 1));
			STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
			STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

			TMSG0 = _mm_sha256msg1_epu32(TMSG0, TMSG1);

			// Rounds 8-11
			MSG = _mm_add_epi32(TMSG2, _mm_load_si128(K_mm + 2));
			STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
			STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

			TMSG1 = _mm_sha256msg1_epu32(TMSG1, TMSG2);

			// Rounds 12-15
			MSG = _mm_add_epi32(TMSG3, _mm_load_si128(K_mm + 3));
			STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
			STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

			TMSG0 = _mm_add_epi32(TMSG0, _mm_alignr_epi8(TMSG3, TMSG2, 4));
			TMSG0 = _mm_sha256msg2_epu32(TMSG0, TMSG3);
			TMSG2 = _mm_sha256msg1_epu32(TMSG2, TMSG3);

			// Rounds 16-19
			MSG = _mm_add_epi32(TMSG0, _mm_load_si128(K_mm + 4));
			STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
			STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

			TMSG1 = _mm_add_epi32(TMSG1, _mm_alignr_epi8(TMSG0, TMSG3, 4));
			TMSG1 = _mm_sha256msg2_epu32(TMSG1, TMSG0);
			TMSG3 = _mm_sha256msg1_epu32(TMSG3, TMSG0);

			// Rounds 20-23
			MSG = _mm_add_epi32(TMSG1, _mm_load_si128(K_mm + 5));
			STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
			STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

			TMSG2 = _mm_add_epi32(TMSG2, _mm_alignr_epi8(TMSG1, TMSG0, 4));
			TMSG2 = _mm_sha256msg2_epu32(TMSG2, TMSG1);
			TMSG0 = _mm_sha256msg1_epu32(TMSG0, TMSG1);

			// Rounds 24-27
			MSG = _mm_add_epi32(TMSG2, _mm_load_si128(K_mm + 6));
			STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
			STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

			TMSG3 = _mm_add_epi32(TMSG3, _mm_alignr_epi8(TMSG2, TMSG1, 4));
			TMSG3 = _mm_sha256msg2_epu32(TMSG3, TMSG2);
			TMSG1 = _mm_sha256msg1_epu32(TMSG1, TMSG2);

			// Rounds 28-31
			MSG = _mm_add_epi32(TMSG3, _mm_load_si128(K_mm + 7));
			STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
			STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

			TMSG0 = _mm_add_epi32(TMSG0, _mm_alignr_epi8(TMSG3, TMSG2, 4));
			TMSG0 = _mm_sha256msg2_epu32(TMSG0, TMSG3);
			TMSG2 = _mm_sha256msg1_epu32(TMSG2, TMSG3);

			// Rounds 32-35
			MSG = _mm_add_epi32(TMSG0, _mm_load_si128(K_mm + 8));
			STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
			STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

			TMSG1 = _mm_add_epi32(TMSG1, _mm_alignr_epi8(TMSG0, TMSG3, 4));
			TMSG1 = _mm_sha256msg2_epu32(TMSG1, TMSG0);
			TMSG3 = _mm_sha256msg1_epu32(TMSG3, TMSG0);

			// Rounds 36-39
			MSG = _mm_add_epi32(TMSG1, _mm_load_si128(K_mm + 9));
			STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
			STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

			TMSG2 = _mm_add_epi32(TMSG2, _mm_alignr_epi8(TMSG1, TMSG0, 4));
			TMSG2 = _mm_sha256msg2_epu32(TMSG2, TMSG1);
			TMSG0 = _mm_sha256msg1_epu32(TMSG0, TMSG1);

			// Rounds 40-43
			MSG = _mm_add_epi32(TMSG2, _mm_load_si128(K_mm + 10));
			STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
			STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

			TMSG3 = _mm_add_epi32(TMSG3, _mm_alignr_epi8(TMSG2, TMSG1, 4));
			TMSG3 = _mm_sha256msg2_epu32(TMSG3, TMSG2);
			TMSG1 = _mm_sha256msg1_epu32(TMSG1, TMSG2);

			// Rounds 44-47
			MSG = _mm_add_epi32(TMSG3, _mm_load_si128(K_mm + 11));
			STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
			STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

			TMSG0 = _mm_add_epi32(TMSG0, _mm_alignr_epi8(TMSG3, TMSG2, 4));
			TMSG0 = _mm_sha256msg2_epu32(TMSG0, TMSG3);
			TMSG2 = _mm_sha256msg1_epu32(TMSG2, TMSG3);

			// Rounds 48-51
			MSG = _mm_add_epi32(TMSG0, _mm_load_si128(K_mm + 12));
			STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
			STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

			TMSG1 = _mm_add_epi32(TMSG1, _mm_alignr_epi8(TMSG0, TMSG3, 4));
			TMSG1 = _mm_sha256msg2_epu32(TMSG1, TMSG0);
			TMSG3 = _mm_sha256msg1_epu32(TMSG3, TMSG0);

			// Rounds 52-55
			MSG = _mm_add_epi32(TMSG1, _mm_load_si128(K_mm + 13));
			STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
			STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

			TMSG2 = _mm_add_epi32(TMSG2, _mm_alignr_epi8(TMSG1, TMSG0, 4));
			TMSG2 = _mm_sha256msg2_epu32(TMSG2, TMSG1);

			// Rounds 56-59
			MSG = _mm_add_epi32(TMSG2, _mm_load_si128(K_mm + 14));
			STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
			STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

			TMSG3 = _mm_add_epi32(TMSG3, _mm_alignr_epi8(TMSG2, TMSG1, 4));
			TMSG3 = _mm_sha256msg2_epu32(TMSG3, TMSG2);

			// Rounds 60-63
			MSG = _mm_add_epi32(TMSG3, _mm_load_si128(K_mm + 15));
			STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
			STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, _mm_shuffle_epi32(MSG, 0x0E));

			// Add values back to state
			STATE0 = _mm_add_epi32(STATE0, ABEF_SAVE);
			STATE1 = _mm_add_epi32(STATE1, CDGH_SAVE);

			input_mm += 4;

			STATE0 = _mm_shuffle_epi32(STATE0, 0x1B); // FEBA
			STATE1 = _mm_shuffle_epi32(STATE1, 0xB1); // DCHG

			// Save state
			_mm_storeu_si128(reinterpret_cast<__m128i*>(&state[0]), _mm_blend_epi16(STATE0, STATE1, 0xF0)); // DCBA
			_mm_storeu_si128(reinterpret_cast<__m128i*>(&state[4]), _mm_alignr_epi8(STATE1, STATE0, 8)); // ABEF

			// end: compress_digest() optimize

	        for(int k = 0; k < 8; ++k) {
	                state[k] = bswap_32(state[k]);
	        }
	        ::memcpy(hash, state, 32);

		// end: sha256_ni() optimize

	}

}

