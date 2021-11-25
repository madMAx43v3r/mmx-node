/*
 * signature_t.hpp
 *
 *  Created on: Nov 25, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_SIGNATURE_T_HPP_
#define INCLUDE_MMX_SIGNATURE_T_HPP_

#include <vnx/Input.hpp>
#include <vnx/Output.hpp>

#include <array>
#include <bls.hpp>


namespace mmx {

struct signature_t {

	std::array<uint8_t, 96> bytes;

	bls::G2Element to_bls() const;

};


inline
bls::G2Element signature_t::to_bls() const
{
	return bls::G2Element::FromBytes(bls::Bytes(bytes.data(), bytes.size()));
}

inline
void read(vnx::TypeInput& in, signature_t& value, const vnx::TypeCode* type_code, const uint16_t* code) {
	vnx::read(in, value.bytes, type_code, code);
}

inline
void write(vnx::TypeOutput& out, const signature_t& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr) {
	vnx::write(out, value.bytes, type_code, code);
}

inline
void read(std::istream& in, signature_t& value) {
	vnx::read(in, value.bytes);
}

inline
void write(std::ostream& out, const signature_t& value) {
	vnx::write(out, value.bytes);
}

inline
void accept(vnx::Visitor& visitor, const signature_t& value) {
	vnx::accept(visitor, value.bytes);
}


} // mmx

#endif /* INCLUDE_MMX_SIGNATURE_T_HPP_ */
