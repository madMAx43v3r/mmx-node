/*
 * sha256_arm_rec.cpp
 *
 *  Created on: Feb 21, 2024
 *      Author: mad, voidxno
 */

// prerequisite: length is 32 bytes (recursive sha256)
// prerequisite: length is 64 bytes, 2x 32bytes (_x2)
// optimization: https://github.com/voidxno/fast-recursive-sha256

#include <sha256_arm.h>

#include <cstring>
#include <stdexcept>

#if defined(__aarch64__)

#include <arm_neon.h>

void recursive_sha256_arm(uint8_t* hash, const uint64_t num_iters)
{
	if(num_iters <= 0) {
		return;
	}

	static const uint32_t K64[64] = {
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
		0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2
	};

	// init values
	static const uint32_t abcdinit[4] = {0x6A09E667,0xBB67AE85,0x3C6EF372,0xA54FF53A};
	static const uint32_t efghinit[4] = {0x510E527F,0x9B05688C,0x1F83D9AB,0x5BE0CD19};
	const uint32x4_t ABCD_INIT = vld1q_u32(&abcdinit[0]);
	const uint32x4_t EFGH_INIT = vld1q_u32(&efghinit[0]);

	// pre-calc/cache padding
	static const uint32_t hpad0cache[4] = {0x80000000,0x00000000,0x00000000,0x00000000};
	static const uint32_t hpad1cache[4] = {0x00000000,0x00000000,0x00000000,0x00000100};
	const uint32x4_t HPAD0_CACHE = vld1q_u32(&hpad0cache[0]);
	const uint32x4_t HPAD1_CACHE = vld1q_u32(&hpad1cache[0]);

	uint32x4_t STATE0;
	uint32x4_t STATE1;
	uint32x4_t STATEV;
	uint32x4_t MSGV;
	uint32x4_t MSGTMP0;
	uint32x4_t MSGTMP1;
	uint32x4_t MSGTMP2;
	uint32x4_t MSGTMP3;

	// init/shuffle hash
	uint32x4_t HASH0_SAVE = vld1q_u32((const uint32_t*)(&hash[0]));
	uint32x4_t HASH1_SAVE = vld1q_u32((const uint32_t*)(&hash[16]));
	HASH0_SAVE = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(HASH0_SAVE)));
	HASH1_SAVE = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(HASH1_SAVE)));

	for(uint64_t i = 0; i < num_iters; ++i) {

		// init state
		STATE0 = ABCD_INIT;
		STATE1 = EFGH_INIT;

		// rounds 0-3
		MSGV = vaddq_u32(HASH0_SAVE,vld1q_u32(&K64[0]));
		STATEV = STATE0;
		STATE0 = vsha256hq_u32(STATE0,STATE1,MSGV);
		STATE1 = vsha256h2q_u32(STATE1,STATEV,MSGV);
		MSGTMP0 = vsha256su0q_u32(HASH0_SAVE,HASH1_SAVE);

		// rounds 4-7
		MSGV = vaddq_u32(HASH1_SAVE,vld1q_u32(&K64[4]));
		STATEV = STATE0;
		STATE0 = vsha256hq_u32(STATE0,STATE1,MSGV);
		STATE1 = vsha256h2q_u32(STATE1,STATEV,MSGV);
		MSGTMP0 = vsha256su1q_u32(MSGTMP0,HPAD0_CACHE,HPAD1_CACHE);
		MSGTMP1 = vsha256su0q_u32(HASH1_SAVE,HPAD0_CACHE);

		// rounds 8-11
		MSGV = vaddq_u32(HPAD0_CACHE,vld1q_u32(&K64[8]));
		STATEV = STATE0;
		STATE0 = vsha256hq_u32(STATE0,STATE1,MSGV);
		STATE1 = vsha256h2q_u32(STATE1,STATEV,MSGV);
		MSGTMP1 = vsha256su1q_u32(MSGTMP1,HPAD1_CACHE,MSGTMP0);
		MSGTMP2 = HPAD0_CACHE;

		// rounds 12-15
		MSGV = vaddq_u32(HPAD1_CACHE,vld1q_u32(&K64[12]));
		STATEV = STATE0;
		STATE0 = vsha256hq_u32(STATE0,STATE1,MSGV);
		STATE1 = vsha256h2q_u32(STATE1,STATEV,MSGV);
		MSGTMP2 = vsha256su1q_u32(MSGTMP2,MSGTMP0,MSGTMP1);
		MSGTMP3 = vsha256su0q_u32(HPAD1_CACHE,MSGTMP0);

#define SHA256ROUND( \
msgv, msgtmp0, msgtmp1, msgtmp2, msgtmp3, statev, state0, state1, kvalue) \
	msgv = vaddq_u32(msgtmp0,vld1q_u32(kvalue)); \
	statev = state0; \
	state0 = vsha256hq_u32(state0,state1,msgv); \
	state1 = vsha256h2q_u32(state1,statev,msgv); \
	msgtmp3 = vsha256su1q_u32(msgtmp3,msgtmp1,msgtmp2); \
	msgtmp0 = vsha256su0q_u32(msgtmp0,msgtmp1);

		// rounds 16-19, 20-23, 24-27, 28-31
		SHA256ROUND(MSGV,MSGTMP0,MSGTMP1,MSGTMP2,MSGTMP3,STATEV,STATE0,STATE1,&K64[16]);
		SHA256ROUND(MSGV,MSGTMP1,MSGTMP2,MSGTMP3,MSGTMP0,STATEV,STATE0,STATE1,&K64[20]);
		SHA256ROUND(MSGV,MSGTMP2,MSGTMP3,MSGTMP0,MSGTMP1,STATEV,STATE0,STATE1,&K64[24]);
		SHA256ROUND(MSGV,MSGTMP3,MSGTMP0,MSGTMP1,MSGTMP2,STATEV,STATE0,STATE1,&K64[28]);

		// rounds 32-35, 36-39, 40-43, 44-47
		SHA256ROUND(MSGV,MSGTMP0,MSGTMP1,MSGTMP2,MSGTMP3,STATEV,STATE0,STATE1,&K64[32]);
		SHA256ROUND(MSGV,MSGTMP1,MSGTMP2,MSGTMP3,MSGTMP0,STATEV,STATE0,STATE1,&K64[36]);
		SHA256ROUND(MSGV,MSGTMP2,MSGTMP3,MSGTMP0,MSGTMP1,STATEV,STATE0,STATE1,&K64[40]);
		SHA256ROUND(MSGV,MSGTMP3,MSGTMP0,MSGTMP1,MSGTMP2,STATEV,STATE0,STATE1,&K64[44]);

		// rounds 48-51
		MSGV = vaddq_u32(MSGTMP0,vld1q_u32(&K64[48]));
		STATEV = STATE0;
		STATE0 = vsha256hq_u32(STATE0,STATE1,MSGV);
		STATE1 = vsha256h2q_u32(STATE1,STATEV,MSGV);
		MSGTMP3 = vsha256su1q_u32(MSGTMP3,MSGTMP1,MSGTMP2);

		// rounds 52-55
		MSGV = vaddq_u32(MSGTMP1,vld1q_u32(&K64[52]));
		STATEV = STATE0;
		STATE0 = vsha256hq_u32(STATE0,STATE1,MSGV);
		STATE1 = vsha256h2q_u32(STATE1,STATEV,MSGV);

		// rounds 56-59
		MSGV = vaddq_u32(MSGTMP2,vld1q_u32(&K64[56]));
		STATEV = STATE0;
		STATE0 = vsha256hq_u32(STATE0,STATE1,MSGV);
		STATE1 = vsha256h2q_u32(STATE1,STATEV,MSGV);

		// rounds 60-63
		MSGV = vaddq_u32(MSGTMP3,vld1q_u32(&K64[60]));
		STATEV = STATE0;
		STATE0 = vsha256hq_u32(STATE0,STATE1,MSGV);
		STATE1 = vsha256h2q_u32(STATE1,STATEV,MSGV);

		// add to state
		HASH0_SAVE = vaddq_u32(STATE0,ABCD_INIT);
		HASH1_SAVE = vaddq_u32(STATE1,EFGH_INIT);
	}

	// shuffle/return hash
	HASH0_SAVE = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(HASH0_SAVE)));
	HASH1_SAVE = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(HASH1_SAVE)));
	vst1q_u32((uint32_t*)(&hash[0]),HASH0_SAVE);
	vst1q_u32((uint32_t*)(&hash[16]),HASH1_SAVE);
}

void recursive_sha256_arm_x2(uint8_t* hash, const uint64_t num_iters)
{
	if(num_iters <= 0) {
		return;
	}

	static const uint32_t K64[64] = {
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
		0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2
	};

	// init values
	static const uint32_t abcdinit[4] = {0x6A09E667,0xBB67AE85,0x3C6EF372,0xA54FF53A};
	static const uint32_t efghinit[4] = {0x510E527F,0x9B05688C,0x1F83D9AB,0x5BE0CD19};
	const uint32x4_t ABCD_INIT = vld1q_u32(&abcdinit[0]);
	const uint32x4_t EFGH_INIT = vld1q_u32(&efghinit[0]);

	// pre-calc/cache padding
	static const uint32_t hpad0cache[4] = {0x80000000,0x00000000,0x00000000,0x00000000};
	static const uint32_t hpad1cache[4] = {0x00000000,0x00000000,0x00000000,0x00000100};
	const uint32x4_t HPAD0_CACHE = vld1q_u32(&hpad0cache[0]);
	const uint32x4_t HPAD1_CACHE = vld1q_u32(&hpad1cache[0]);

	uint32x4_t STATE0_P1;
	uint32x4_t STATE1_P1;
	uint32x4_t STATEV_P1;
	uint32x4_t MSGV_P1;
	uint32x4_t MSGTMP0_P1;
	uint32x4_t MSGTMP1_P1;
	uint32x4_t MSGTMP2_P1;
	uint32x4_t MSGTMP3_P1;
	uint32x4_t STATE0_P2;
	uint32x4_t STATE1_P2;
	uint32x4_t STATEV_P2;
	uint32x4_t MSGV_P2;
	uint32x4_t MSGTMP0_P2;
	uint32x4_t MSGTMP1_P2;
	uint32x4_t MSGTMP2_P2;
	uint32x4_t MSGTMP3_P2;

	// init/shuffle hash
	uint32x4_t HASH0_SAVE_P1 = vld1q_u32((const uint32_t*)(&hash[0]));
	uint32x4_t HASH1_SAVE_P1 = vld1q_u32((const uint32_t*)(&hash[16]));
	uint32x4_t HASH0_SAVE_P2 = vld1q_u32((const uint32_t*)(&hash[32]));
	uint32x4_t HASH1_SAVE_P2 = vld1q_u32((const uint32_t*)(&hash[48]));
	HASH0_SAVE_P1 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(HASH0_SAVE_P1)));
	HASH1_SAVE_P1 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(HASH1_SAVE_P1)));
	HASH0_SAVE_P2 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(HASH0_SAVE_P2)));
	HASH1_SAVE_P2 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(HASH1_SAVE_P2)));

	for(uint64_t i = 0; i < num_iters; ++i) {

		// init state
		STATE0_P1 = ABCD_INIT;
		STATE0_P2 = ABCD_INIT;
		STATE1_P1 = EFGH_INIT;
		STATE1_P2 = EFGH_INIT;

		// rounds 0-3
		MSGV_P1 = vaddq_u32(HASH0_SAVE_P1,vld1q_u32(&K64[0]));
		MSGV_P2 = vaddq_u32(HASH0_SAVE_P2,vld1q_u32(&K64[0]));
		STATEV_P1 = STATE0_P1;
		STATEV_P2 = STATE0_P2;
		STATE0_P1 = vsha256hq_u32(STATE0_P1,STATE1_P1,MSGV_P1);
		STATE0_P2 = vsha256hq_u32(STATE0_P2,STATE1_P2,MSGV_P2);
		STATE1_P1 = vsha256h2q_u32(STATE1_P1,STATEV_P1,MSGV_P1);
		STATE1_P2 = vsha256h2q_u32(STATE1_P2,STATEV_P2,MSGV_P2);
		MSGTMP0_P1 = vsha256su0q_u32(HASH0_SAVE_P1,HASH1_SAVE_P1);
		MSGTMP0_P2 = vsha256su0q_u32(HASH0_SAVE_P2,HASH1_SAVE_P2);

		// rounds 4-7
		MSGV_P1 = vaddq_u32(HASH1_SAVE_P1,vld1q_u32(&K64[4]));
		MSGV_P2 = vaddq_u32(HASH1_SAVE_P2,vld1q_u32(&K64[4]));
		STATEV_P1 = STATE0_P1;
		STATEV_P2 = STATE0_P2;
		STATE0_P1 = vsha256hq_u32(STATE0_P1,STATE1_P1,MSGV_P1);
		STATE0_P2 = vsha256hq_u32(STATE0_P2,STATE1_P2,MSGV_P2);
		STATE1_P1 = vsha256h2q_u32(STATE1_P1,STATEV_P1,MSGV_P1);
		STATE1_P2 = vsha256h2q_u32(STATE1_P2,STATEV_P2,MSGV_P2);
		MSGTMP0_P1 = vsha256su1q_u32(MSGTMP0_P1,HPAD0_CACHE,HPAD1_CACHE);
		MSGTMP0_P2 = vsha256su1q_u32(MSGTMP0_P2,HPAD0_CACHE,HPAD1_CACHE);
		MSGTMP1_P1 = vsha256su0q_u32(HASH1_SAVE_P1,HPAD0_CACHE);
		MSGTMP1_P2 = vsha256su0q_u32(HASH1_SAVE_P2,HPAD0_CACHE);

		// rounds 8-11
		MSGV_P1 = vaddq_u32(HPAD0_CACHE,vld1q_u32(&K64[8]));
		MSGV_P2 = vaddq_u32(HPAD0_CACHE,vld1q_u32(&K64[8]));
		STATEV_P1 = STATE0_P1;
		STATEV_P2 = STATE0_P2;
		STATE0_P1 = vsha256hq_u32(STATE0_P1,STATE1_P1,MSGV_P1);
		STATE0_P2 = vsha256hq_u32(STATE0_P2,STATE1_P2,MSGV_P2);
		STATE1_P1 = vsha256h2q_u32(STATE1_P1,STATEV_P1,MSGV_P1);
		STATE1_P2 = vsha256h2q_u32(STATE1_P2,STATEV_P2,MSGV_P2);
		MSGTMP1_P1 = vsha256su1q_u32(MSGTMP1_P1,HPAD1_CACHE,MSGTMP0_P1);
		MSGTMP1_P2 = vsha256su1q_u32(MSGTMP1_P2,HPAD1_CACHE,MSGTMP0_P2);
		MSGTMP2_P1 = HPAD0_CACHE;
		MSGTMP2_P2 = HPAD0_CACHE;

		// rounds 12-15
		MSGV_P1 = vaddq_u32(HPAD1_CACHE,vld1q_u32(&K64[12]));
		MSGV_P2 = vaddq_u32(HPAD1_CACHE,vld1q_u32(&K64[12]));
		STATEV_P1 = STATE0_P1;
		STATEV_P2 = STATE0_P2;
		STATE0_P1 = vsha256hq_u32(STATE0_P1,STATE1_P1,MSGV_P1);
		STATE0_P2 = vsha256hq_u32(STATE0_P2,STATE1_P2,MSGV_P2);
		STATE1_P1 = vsha256h2q_u32(STATE1_P1,STATEV_P1,MSGV_P1);
		STATE1_P2 = vsha256h2q_u32(STATE1_P2,STATEV_P2,MSGV_P2);
		MSGTMP2_P1 = vsha256su1q_u32(MSGTMP2_P1,MSGTMP0_P1,MSGTMP1_P1);
		MSGTMP2_P2 = vsha256su1q_u32(MSGTMP2_P2,MSGTMP0_P2,MSGTMP1_P2);
		MSGTMP3_P1 = vsha256su0q_u32(HPAD1_CACHE,MSGTMP0_P1);
		MSGTMP3_P2 = vsha256su0q_u32(HPAD1_CACHE,MSGTMP0_P2);

#define SHA256ROUND_X2( \
msgv_p1, msgtmp0_p1, msgtmp1_p1, msgtmp2_p1, msgtmp3_p1, statev_p1, state0_p1, state1_p1, \
msgv_p2, msgtmp0_p2, msgtmp1_p2, msgtmp2_p2, msgtmp3_p2, statev_p2, state0_p2, state1_p2, kvalue) \
	msgv_p1 = vaddq_u32(msgtmp0_p1,vld1q_u32(kvalue)); \
	msgv_p2 = vaddq_u32(msgtmp0_p2,vld1q_u32(kvalue)); \
	statev_p1 = state0_p1; \
	statev_p2 = state0_p2; \
	state0_p1 = vsha256hq_u32(state0_p1,state1_p1,msgv_p1); \
	state0_p2 = vsha256hq_u32(state0_p2,state1_p2,msgv_p2); \
	state1_p1 = vsha256h2q_u32(state1_p1,statev_p1,msgv_p1); \
	state1_p2 = vsha256h2q_u32(state1_p2,statev_p2,msgv_p2); \
	msgtmp3_p1 = vsha256su1q_u32(msgtmp3_p1,msgtmp1_p1,msgtmp2_p1); \
	msgtmp3_p2 = vsha256su1q_u32(msgtmp3_p2,msgtmp1_p2,msgtmp2_p2); \
	msgtmp0_p1 = vsha256su0q_u32(msgtmp0_p1,msgtmp1_p1); \
	msgtmp0_p2 = vsha256su0q_u32(msgtmp0_p2,msgtmp1_p2);

		// rounds 16-19, 20-23, 24-27, 28-31
		SHA256ROUND_X2(MSGV_P1,MSGTMP0_P1,MSGTMP1_P1,MSGTMP2_P1,MSGTMP3_P1,STATEV_P1,STATE0_P1,STATE1_P1,MSGV_P2,MSGTMP0_P2,MSGTMP1_P2,MSGTMP2_P2,MSGTMP3_P2,STATEV_P2,STATE0_P2,STATE1_P2,&K64[16]);
		SHA256ROUND_X2(MSGV_P1,MSGTMP1_P1,MSGTMP2_P1,MSGTMP3_P1,MSGTMP0_P1,STATEV_P1,STATE0_P1,STATE1_P1,MSGV_P2,MSGTMP1_P2,MSGTMP2_P2,MSGTMP3_P2,MSGTMP0_P2,STATEV_P2,STATE0_P2,STATE1_P2,&K64[20]);
		SHA256ROUND_X2(MSGV_P1,MSGTMP2_P1,MSGTMP3_P1,MSGTMP0_P1,MSGTMP1_P1,STATEV_P1,STATE0_P1,STATE1_P1,MSGV_P2,MSGTMP2_P2,MSGTMP3_P2,MSGTMP0_P2,MSGTMP1_P2,STATEV_P2,STATE0_P2,STATE1_P2,&K64[24]);
		SHA256ROUND_X2(MSGV_P1,MSGTMP3_P1,MSGTMP0_P1,MSGTMP1_P1,MSGTMP2_P1,STATEV_P1,STATE0_P1,STATE1_P1,MSGV_P2,MSGTMP3_P2,MSGTMP0_P2,MSGTMP1_P2,MSGTMP2_P2,STATEV_P2,STATE0_P2,STATE1_P2,&K64[28]);

		// rounds 32-35, 36-39, 40-43, 44-47
		SHA256ROUND_X2(MSGV_P1,MSGTMP0_P1,MSGTMP1_P1,MSGTMP2_P1,MSGTMP3_P1,STATEV_P1,STATE0_P1,STATE1_P1,MSGV_P2,MSGTMP0_P2,MSGTMP1_P2,MSGTMP2_P2,MSGTMP3_P2,STATEV_P2,STATE0_P2,STATE1_P2,&K64[32]);
		SHA256ROUND_X2(MSGV_P1,MSGTMP1_P1,MSGTMP2_P1,MSGTMP3_P1,MSGTMP0_P1,STATEV_P1,STATE0_P1,STATE1_P1,MSGV_P2,MSGTMP1_P2,MSGTMP2_P2,MSGTMP3_P2,MSGTMP0_P2,STATEV_P2,STATE0_P2,STATE1_P2,&K64[36]);
		SHA256ROUND_X2(MSGV_P1,MSGTMP2_P1,MSGTMP3_P1,MSGTMP0_P1,MSGTMP1_P1,STATEV_P1,STATE0_P1,STATE1_P1,MSGV_P2,MSGTMP2_P2,MSGTMP3_P2,MSGTMP0_P2,MSGTMP1_P2,STATEV_P2,STATE0_P2,STATE1_P2,&K64[40]);
		SHA256ROUND_X2(MSGV_P1,MSGTMP3_P1,MSGTMP0_P1,MSGTMP1_P1,MSGTMP2_P1,STATEV_P1,STATE0_P1,STATE1_P1,MSGV_P2,MSGTMP3_P2,MSGTMP0_P2,MSGTMP1_P2,MSGTMP2_P2,STATEV_P2,STATE0_P2,STATE1_P2,&K64[44]);

		// rounds 48-51
		MSGV_P1 = vaddq_u32(MSGTMP0_P1,vld1q_u32(&K64[48]));
		MSGV_P2 = vaddq_u32(MSGTMP0_P2,vld1q_u32(&K64[48]));
		STATEV_P1 = STATE0_P1;
		STATEV_P2 = STATE0_P2;
		STATE0_P1 = vsha256hq_u32(STATE0_P1,STATE1_P1,MSGV_P1);
		STATE0_P2 = vsha256hq_u32(STATE0_P2,STATE1_P2,MSGV_P2);
		STATE1_P1 = vsha256h2q_u32(STATE1_P1,STATEV_P1,MSGV_P1);
		STATE1_P2 = vsha256h2q_u32(STATE1_P2,STATEV_P2,MSGV_P2);
		MSGTMP3_P1 = vsha256su1q_u32(MSGTMP3_P1,MSGTMP1_P1,MSGTMP2_P1);
		MSGTMP3_P2 = vsha256su1q_u32(MSGTMP3_P2,MSGTMP1_P2,MSGTMP2_P2);

		// rounds 52-55
		MSGV_P1 = vaddq_u32(MSGTMP1_P1,vld1q_u32(&K64[52]));
		MSGV_P2 = vaddq_u32(MSGTMP1_P2,vld1q_u32(&K64[52]));
		STATEV_P1 = STATE0_P1;
		STATEV_P2 = STATE0_P2;
		STATE0_P1 = vsha256hq_u32(STATE0_P1,STATE1_P1,MSGV_P1);
		STATE0_P2 = vsha256hq_u32(STATE0_P2,STATE1_P2,MSGV_P2);
		STATE1_P1 = vsha256h2q_u32(STATE1_P1,STATEV_P1,MSGV_P1);
		STATE1_P2 = vsha256h2q_u32(STATE1_P2,STATEV_P2,MSGV_P2);

		// rounds 56-59
		MSGV_P1 = vaddq_u32(MSGTMP2_P1,vld1q_u32(&K64[56]));
		MSGV_P2 = vaddq_u32(MSGTMP2_P2,vld1q_u32(&K64[56]));
		STATEV_P1 = STATE0_P1;
		STATEV_P2 = STATE0_P2;
		STATE0_P1 = vsha256hq_u32(STATE0_P1,STATE1_P1,MSGV_P1);
		STATE0_P2 = vsha256hq_u32(STATE0_P2,STATE1_P2,MSGV_P2);
		STATE1_P1 = vsha256h2q_u32(STATE1_P1,STATEV_P1,MSGV_P1);
		STATE1_P2 = vsha256h2q_u32(STATE1_P2,STATEV_P2,MSGV_P2);

		// rounds 60-63
		MSGV_P1 = vaddq_u32(MSGTMP3_P1,vld1q_u32(&K64[60]));
		MSGV_P2 = vaddq_u32(MSGTMP3_P2,vld1q_u32(&K64[60]));
		STATEV_P1 = STATE0_P1;
		STATEV_P2 = STATE0_P2;
		STATE0_P1 = vsha256hq_u32(STATE0_P1,STATE1_P1,MSGV_P1);
		STATE0_P2 = vsha256hq_u32(STATE0_P2,STATE1_P2,MSGV_P2);
		STATE1_P1 = vsha256h2q_u32(STATE1_P1,STATEV_P1,MSGV_P1);
		STATE1_P2 = vsha256h2q_u32(STATE1_P2,STATEV_P2,MSGV_P2);

		// add to state
		HASH0_SAVE_P1 = vaddq_u32(STATE0_P1,ABCD_INIT);
		HASH0_SAVE_P2 = vaddq_u32(STATE0_P2,ABCD_INIT);
		HASH1_SAVE_P1 = vaddq_u32(STATE1_P1,EFGH_INIT);
		HASH1_SAVE_P2 = vaddq_u32(STATE1_P2,EFGH_INIT);
	}

	// shuffle/return hash
	HASH0_SAVE_P1 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(HASH0_SAVE_P1)));
	HASH1_SAVE_P1 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(HASH1_SAVE_P1)));
	HASH0_SAVE_P2 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(HASH0_SAVE_P2)));
	HASH1_SAVE_P2 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(HASH1_SAVE_P2)));
	vst1q_u32((uint32_t*)(&hash[0]),HASH0_SAVE_P1);
	vst1q_u32((uint32_t*)(&hash[16]),HASH1_SAVE_P1);
	vst1q_u32((uint32_t*)(&hash[32]),HASH0_SAVE_P2);
	vst1q_u32((uint32_t*)(&hash[48]),HASH1_SAVE_P2);
}

#else // __aarch64__

void recursive_sha256_arm(uint8_t* hash, const uint64_t num_iters) {
	throw std::logic_error("recursive_sha256_arm() not available");
}

void recursive_sha256_arm_x2(uint8_t* hash, const uint64_t num_iters) {
	throw std::logic_error("recursive_sha256_arm_x2() not available");
}

#endif // __aarch64__

