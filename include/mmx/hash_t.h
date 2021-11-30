/*
 * hash_t.h
 *
 *  Created on: Nov 27, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_HASH_T_H_
#define INCLUDE_MMX_HASH_T_H_

#include <array>
#include <vector>
#include <functional>


namespace mmx {

struct hash_t {

	std::array<uint8_t, 32> bytes = {};

	hash_t() = default;

	hash_t(const void* data, const size_t num_bytes);

	explicit hash_t(const std::vector<uint8_t>& data);

	explicit hash_t(const std::array<uint8_t, 32>& data);

	const uint8_t* data() const;

	bool is_zero() const;

	std::string to_string() const;

	bool operator==(const hash_t& other) const {
		return bytes == other.bytes;
	}

	bool operator!=(const hash_t& other) const {
		return bytes != other.bytes;
	}

	static hash_t from_string(const std::string& str);

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
