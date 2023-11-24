/*
 * sha256_ni.h
 *
 *  Created on: Jul 19, 2022
 *      Author: mad
 */

#ifndef INCLUDE_SHA256_NI_H_
#define INCLUDE_SHA256_NI_H_

#include <cstdint>

void sha256_ni(uint8_t* out, const uint8_t* in, const uint64_t length);

void recursive_sha256_ni(uint8_t* hash, const uint64_t num_iters);

void recursive_sha256_ni_x2(uint8_t* hash, const uint64_t num_iters);

bool sha256_ni_available();



#endif /* INCLUDE_SHA256_NI_H_ */
