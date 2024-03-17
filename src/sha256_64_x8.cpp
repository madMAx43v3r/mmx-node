/*
 * sha256_64_x8.cpp
 *
 *  Created on: Oct 17, 2022
 *      Author: mad
 */

#include <sha256_avx2.h>
#include <sha256_ni.h>
#include <sha256_arm.h>
#include <mmx/hash_t.hpp>


void sha256_64_x8(uint8_t* out, uint8_t* in, const uint64_t length)
{
	static bool have_avx2 = avx2_available();
	static bool have_sha_ni = sha256_ni_available();
	static bool have_sha_arm = sha256_arm_available();

	if(have_sha_ni) {
		for(int i = 0; i < 8; ++i) {
			sha256_ni(out + i * 32, in + i * 64, length);
		}
	} else if(have_sha_arm) {
		for(int i = 0; i < 8; ++i) {
			sha256_arm(out + i * 32, in + i * 64, length);
		}
	} else if(have_avx2) {
		sha256_avx2_64_x8(out, in, length);
	} else {
		for (int i = 0; i < 8; ++i) {
			const mmx::hash_t hash(in + i * 64, length);
			::memcpy(out + i * 32, hash.data(), 32);
		}
	}
}

