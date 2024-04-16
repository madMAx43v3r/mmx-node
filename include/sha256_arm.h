/*
 * sha256_arm.h
 *
 *  Created on: Feb 21, 2024
 *      Author: mad, voidxno
 */

#ifndef INCLUDE_SHA256_ARM_H_
#define INCLUDE_SHA256_ARM_H_

#include <cstdint>

void sha256_arm(uint8_t* out, const uint8_t* in, const uint64_t length);

void recursive_sha256_arm(uint8_t* hash, const uint64_t num_iters);

void recursive_sha256_arm_x2(uint8_t* hash, const uint64_t num_iters);

bool sha256_arm_available();



#endif /* INCLUDE_SHA256_ARM_H_ */
