/*
 * hash_t.hpp
 *
 *  Created on: Nov 25, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_HASH_T_HPP_
#define INCLUDE_MMX_HASH_T_HPP_

#include <vnx/Input.hpp>
#include <vnx/Output.hpp>

#include <array>


namespace mmx {

struct hash_t {

	std::array<uint8_t, 32> bytes;

};


inline
void read(vnx::TypeInput& in, hash_t& value, const vnx::TypeCode* type_code, const uint16_t* code) {
	vnx::read(in, value.bytes, type_code, code);
}

inline
void write(vnx::TypeOutput& out, const hash_t& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr) {
	vnx::write(out, value.bytes, type_code, code);
}

inline
void read(std::istream& in, hash_t& value) {
	vnx::read(in, value.bytes);
}

inline
void write(std::ostream& out, const hash_t& value) {
	vnx::write(out, value.bytes);
}

inline
void accept(vnx::Visitor& visitor, const hash_t& value) {
	vnx::accept(visitor, value.bytes);
}


} // mmx

#endif /* INCLUDE_MMX_HASH_T_HPP_ */
