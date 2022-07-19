/*
 * sha256_avx2.h
 *
 *  Created on: Jul 19, 2022
 *      Author: mad
 */

#ifndef INCLUDE_SHA256_AVX2_H_
#define INCLUDE_SHA256_AVX2_H_

#include <cstdint>

void sha256_avx2_64_x8(uint8_t* out, uint8_t* in, const uint64_t length);

void sha256_64_x8(uint8_t* out, uint8_t* in, const uint64_t length);


#endif /* INCLUDE_SHA256_AVX2_H_ */
