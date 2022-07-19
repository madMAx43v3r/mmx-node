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



#endif /* INCLUDE_SHA256_NI_H_ */
