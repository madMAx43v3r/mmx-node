/*
 * hash_t.hpp
 *
 *  Created on: Nov 25, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_HASH_T_HPP_
#define INCLUDE_MMX_HASH_T_HPP_

#include <mmx/hash_t.h>

#include <vnx/Input.hpp>
#include <vnx/Output.hpp>

#include <bls.hpp>


namespace mmx {

inline
hash_t::hash_t(const void* data, const size_t num_bytes)
{
	bls::Util::Hash256(bytes.data(), (const uint8_t*)data, num_bytes);
}

} // mmx


namespace vnx {

inline
void read(vnx::TypeInput& in, mmx::hash_t& value, const vnx::TypeCode* type_code, const uint16_t* code) {
	vnx::read(in, value.bytes, type_code, code);
}

inline
void write(vnx::TypeOutput& out, const mmx::hash_t& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr) {
	vnx::write(out, value.bytes, type_code, code);
}

inline
void read(std::istream& in, mmx::hash_t& value) {
	vnx::read(in, value.bytes);
}

inline
void write(std::ostream& out, const mmx::hash_t& value) {
	vnx::write(out, value.bytes);
}

inline
void accept(vnx::Visitor& visitor, const mmx::hash_t& value) {
	vnx::accept(visitor, value.bytes);
}

} // vnx

#endif /* INCLUDE_MMX_HASH_T_HPP_ */
