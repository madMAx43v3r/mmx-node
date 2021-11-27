/*
 * hash_t.h
 *
 *  Created on: Nov 27, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_HASH_T_H_
#define INCLUDE_MMX_HASH_T_H_

#include <array>
#include <algorithm>


namespace mmx {

struct hash_t {

	hash_t();

	hash_t(const void* data, const size_t num_bytes);

	bool operator==(const hash_t& other) const {
		return bytes == other.bytes;
	}

	bool operator!=(const hash_t& other) const {
		return bytes != other.bytes;
	}

	std::array<uint8_t, 32> bytes;

};


} // mmx

#endif /* INCLUDE_MMX_HASH_T_H_ */
