/*
 * uint128.hpp
 *
 *  Created on: Apr 11, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_UINT128_HPP_
#define INCLUDE_MMX_UINT128_HPP_

#include <uint128_t.h>

#include <vnx/Input.h>
#include <vnx/Output.h>
#include <vnx/Visitor.h>


namespace mmx {

struct uint128 : uint128_t {

	uint128() : uint128_t(0) {}

	uint128(const uint128&) = default;

	uint128(const uint128_t& value) : uint128_t(value) {}

};


} //mmx


namespace vnx {

inline
void read(vnx::TypeInput& in, mmx::uint128& value, const vnx::TypeCode* type_code, const uint16_t* code) {
	switch(code[0]) {
		case CODE_STRING:
		case CODE_ALT_STRING: {
			std::string tmp;
			vnx::read(in, tmp, type_code, code);
			value = uint128_t(tmp, 10);
			break;
		}
		default: {
			std::array<uint64_t, 2> tmp;
			vnx::read(in, tmp, type_code, code);
			value = uint128_t(tmp[0], tmp[1]);
		}
	}
}

inline
void write(vnx::TypeOutput& out, const mmx::uint128& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr) {
	const std::array<uint64_t, 2> tmp = {value.upper(), value.lower()};
	vnx::write(out, tmp, type_code, code);
}

inline
void read(std::istream& in, mmx::uint128& value) {
	std::string tmp;
	vnx::read(in, tmp);
	value = uint128_t(tmp, 10);
}

inline
void write(std::ostream& out, const mmx::uint128& value) {
	vnx::write(out, value.str(10));
}

inline
void accept(vnx::Visitor& visitor, const mmx::uint128& value) {
	vnx::accept(visitor, value.str(10));
}

} // vnx

#endif /* INCLUDE_MMX_UINT128_HPP_ */
