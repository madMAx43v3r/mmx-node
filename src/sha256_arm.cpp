/*
 * sha256_arm.cpp
 *
 *  Created on: Feb 21, 2024
 *      Author: mad, voidxno
 */

#include <sha256_arm.h>

#include <cstring>
#include <stdexcept>

#if defined(__aarch64__)

#include <sys/auxv.h>
#include <arm_neon.h>

static const uint32_t K64[] = {
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
	const uint8_t* input_mm = input;

	// Load initial values
	uint32x4_t STATE0 = vld1q_u32(&state[0]);
	uint32x4_t STATE1 = vld1q_u32(&state[4]);

	while(blocks > 0)
	{
		// Save current state
		const uint32x4_t ABCD_SAVE = STATE0;
		const uint32x4_t EFGH_SAVE = STATE1;

		uint32x4_t MSGV;
		uint32x4_t STATEV;

		uint32x4_t MSGTMP0 = vld1q_u32((const uint32_t*)(&input_mm[0]));
		uint32x4_t MSGTMP1 = vld1q_u32((const uint32_t*)(&input_mm[16]));
		uint32x4_t MSGTMP2 = vld1q_u32((const uint32_t*)(&input_mm[32]));
		uint32x4_t MSGTMP3 = vld1q_u32((const uint32_t*)(&input_mm[48]));

		MSGTMP0 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(MSGTMP0)));
		MSGTMP1 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(MSGTMP1)));
		MSGTMP2 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(MSGTMP2)));
		MSGTMP3 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(MSGTMP3)));

		// Rounds 0-3
		MSGV = vaddq_u32(MSGTMP0, vld1q_u32(&K64[0]));
		STATEV = STATE0;
		STATE0 = vsha256hq_u32(STATE0, STATE1, MSGV);
		STATE1 = vsha256h2q_u32(STATE1, STATEV, MSGV);
		MSGTMP0 = vsha256su0q_u32(MSGTMP0, MSGTMP1);

		// Rounds 4-7
		MSGV = vaddq_u32(MSGTMP1, vld1q_u32(&K64[4]));
		STATEV = STATE0;
		STATE0 = vsha256hq_u32(STATE0, STATE1, MSGV);
		STATE1 = vsha256h2q_u32(STATE1, STATEV, MSGV);
		MSGTMP0 = vsha256su1q_u32(MSGTMP0, MSGTMP2, MSGTMP3);
		MSGTMP1 = vsha256su0q_u32(MSGTMP1, MSGTMP2);

		// Rounds 8-11
		MSGV = vaddq_u32(MSGTMP2, vld1q_u32(&K64[8]));
		STATEV = STATE0;
		STATE0 = vsha256hq_u32(STATE0, STATE1, MSGV);
		STATE1 = vsha256h2q_u32(STATE1, STATEV, MSGV);
		MSGTMP1 = vsha256su1q_u32(MSGTMP1, MSGTMP3, MSGTMP0);
		MSGTMP2 = vsha256su0q_u32(MSGTMP2, MSGTMP3);

		// Rounds 12-15
		MSGV = vaddq_u32(MSGTMP3, vld1q_u32(&K64[12]));
		STATEV = STATE0;
		STATE0 = vsha256hq_u32(STATE0, STATE1, MSGV);
		STATE1 = vsha256h2q_u32(STATE1, STATEV, MSGV);
		MSGTMP2 = vsha256su1q_u32(MSGTMP2, MSGTMP0, MSGTMP1);
		MSGTMP3 = vsha256su0q_u32(MSGTMP3, MSGTMP0);

		// Rounds 16-19
		MSGV = vaddq_u32(MSGTMP0, vld1q_u32(&K64[16]));
		STATEV = STATE0;
		STATE0 = vsha256hq_u32(STATE0, STATE1, MSGV);
		STATE1 = vsha256h2q_u32(STATE1, STATEV, MSGV);
		MSGTMP3 = vsha256su1q_u32(MSGTMP3, MSGTMP1, MSGTMP2);
		MSGTMP0 = vsha256su0q_u32(MSGTMP0, MSGTMP1);

		// Rounds 20-23
		MSGV = vaddq_u32(MSGTMP1, vld1q_u32(&K64[20]));
		STATEV = STATE0;
		STATE0 = vsha256hq_u32(STATE0, STATE1, MSGV);
		STATE1 = vsha256h2q_u32(STATE1, STATEV, MSGV);
		MSGTMP0 = vsha256su1q_u32(MSGTMP0, MSGTMP2, MSGTMP3);
		MSGTMP1 = vsha256su0q_u32(MSGTMP1, MSGTMP2);

		// Rounds 24-27
		MSGV = vaddq_u32(MSGTMP2, vld1q_u32(&K64[24]));
		STATEV = STATE0;
		STATE0 = vsha256hq_u32(STATE0, STATE1, MSGV);
		STATE1 = vsha256h2q_u32(STATE1, STATEV, MSGV);
		MSGTMP1 = vsha256su1q_u32(MSGTMP1, MSGTMP3, MSGTMP0);
		MSGTMP2 = vsha256su0q_u32(MSGTMP2, MSGTMP3);

		// Rounds 28-31
		MSGV = vaddq_u32(MSGTMP3, vld1q_u32(&K64[28]));
		STATEV = STATE0;
		STATE0 = vsha256hq_u32(STATE0, STATE1, MSGV);
		STATE1 = vsha256h2q_u32(STATE1, STATEV, MSGV);
		MSGTMP2 = vsha256su1q_u32(MSGTMP2, MSGTMP0, MSGTMP1);
		MSGTMP3 = vsha256su0q_u32(MSGTMP3, MSGTMP0);

		// Rounds 32-35
		MSGV = vaddq_u32(MSGTMP0, vld1q_u32(&K64[32]));
		STATEV = STATE0;
		STATE0 = vsha256hq_u32(STATE0, STATE1, MSGV);
		STATE1 = vsha256h2q_u32(STATE1, STATEV, MSGV);
		MSGTMP3 = vsha256su1q_u32(MSGTMP3, MSGTMP1, MSGTMP2);
		MSGTMP0 = vsha256su0q_u32(MSGTMP0, MSGTMP1);

		// Rounds 36-39
		MSGV = vaddq_u32(MSGTMP1, vld1q_u32(&K64[36]));
		STATEV = STATE0;
		STATE0 = vsha256hq_u32(STATE0, STATE1, MSGV);
		STATE1 = vsha256h2q_u32(STATE1, STATEV, MSGV);
		MSGTMP0 = vsha256su1q_u32(MSGTMP0, MSGTMP2, MSGTMP3);
		MSGTMP1 = vsha256su0q_u32(MSGTMP1, MSGTMP2);

		// Rounds 40-43
		MSGV = vaddq_u32(MSGTMP2, vld1q_u32(&K64[40]));
		STATEV = STATE0;
		STATE0 = vsha256hq_u32(STATE0, STATE1, MSGV);
		STATE1 = vsha256h2q_u32(STATE1, STATEV, MSGV);
		MSGTMP1 = vsha256su1q_u32(MSGTMP1, MSGTMP3, MSGTMP0);
		MSGTMP2 = vsha256su0q_u32(MSGTMP2, MSGTMP3);

		// Rounds 44-47
		MSGV = vaddq_u32(MSGTMP3, vld1q_u32(&K64[44]));
		STATEV = STATE0;
		STATE0 = vsha256hq_u32(STATE0, STATE1, MSGV);
		STATE1 = vsha256h2q_u32(STATE1, STATEV, MSGV);
		MSGTMP2 = vsha256su1q_u32(MSGTMP2, MSGTMP0, MSGTMP1);
		MSGTMP3 = vsha256su0q_u32(MSGTMP3, MSGTMP0);

		// Rounds 48-51
		MSGV = vaddq_u32(MSGTMP0, vld1q_u32(&K64[48]));
		STATEV = STATE0;
		STATE0 = vsha256hq_u32(STATE0, STATE1, MSGV);
		STATE1 = vsha256h2q_u32(STATE1, STATEV, MSGV);
		MSGTMP3 = vsha256su1q_u32(MSGTMP3, MSGTMP1, MSGTMP2);

		// Rounds 52-55
		MSGV = vaddq_u32(MSGTMP1, vld1q_u32(&K64[52]));
		STATEV = STATE0;
		STATE0 = vsha256hq_u32(STATE0, STATE1, MSGV);
		STATE1 = vsha256h2q_u32(STATE1, STATEV, MSGV);

		// Rounds 56-59
		MSGV = vaddq_u32(MSGTMP2, vld1q_u32(&K64[56]));
		STATEV = STATE0;
		STATE0 = vsha256hq_u32(STATE0, STATE1, MSGV);
		STATE1 = vsha256h2q_u32(STATE1, STATEV, MSGV);

		// Rounds 60-63
		MSGV = vaddq_u32(MSGTMP3, vld1q_u32(&K64[60]));
		STATEV = STATE0;
		STATE0 = vsha256hq_u32(STATE0, STATE1, MSGV);
		STATE1 = vsha256h2q_u32(STATE1, STATEV, MSGV);

		// Add values back to state
		STATE0 = vaddq_u32(STATE0, ABCD_SAVE);
		STATE1 = vaddq_u32(STATE1, EFGH_SAVE);

		input_mm = &input_mm[64];
		blocks--;
	}

 // Save state
 vst1q_u32(&state[0], STATE0);
 vst1q_u32(&state[4], STATE1);
}

void sha256_arm(uint8_t* out, const uint8_t* in, const uint64_t length)
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

bool sha256_arm_available()
{
	// ARM Cryptography Extensions (SHA256 feature)
	// ID_AA64ISAR0_EL1 feature register, bits 15-12 (SHA2)

	// user space detect through getauxval()
	return (getauxval(AT_HWCAP) & HWCAP_SHA2) ? true : false;
}

#else // __aarch64__

void sha256_arm(uint8_t* out, const uint8_t* in, const uint64_t length) {
	throw std::logic_error("sha256_arm() not available");
}

bool sha256_arm_available() {
	return false;
}

#endif // __aarch64__

