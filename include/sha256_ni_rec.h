/*
 * sha256_ni_rec.h
 *
 *  Created on: May 28, 2023
 *      Author: mad
 */

#ifndef INCLUDE_SHA256_NI_REC_H_
#define INCLUDE_SHA256_NI_REC_H_

#include <cstdint>

void recursive_sha256_ni(uint8_t* hash, const uint64_t num_iters);

#endif /* INCLUDE_SHA256_NI_REC_H_ */
