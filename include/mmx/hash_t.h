/*
 * hash_t.h
 *
 *  Created on: Nov 27, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_HASH_T_H_
#define INCLUDE_MMX_HASH_T_H_

#include <array>


namespace mmx {

struct hash_t {

	std::array<uint8_t, 32> bytes = {};

	hash_t() = default;

	hash_t(const void* data, const size_t num_bytes);

	bool is_zero() const;

	bool operator==(const hash_t& other) const {
		return bytes == other.bytes;
	}

	bool operator!=(const hash_t& other) const {
		return bytes != other.bytes;
	}

};


} // mmx


namespace std {
	template<> struct hash<mmx::hash_t> {
		size_t operator()(const mmx::hash_t& x) const {
			return *((const size_t*)x.bytes.data());
		}
	};
} // std

#endif /* INCLUDE_MMX_HASH_T_H_ */
