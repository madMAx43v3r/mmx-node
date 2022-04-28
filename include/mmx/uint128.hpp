/*
 * uint128.hpp
 *
 *  Created on: Apr 11, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_UINT128_HPP_
#define INCLUDE_MMX_UINT128_HPP_

#include <uint128_t.h>

#include <vnx/Input.hpp>
#include <vnx/Output.hpp>
#include <vnx/Variant.hpp>


namespace mmx {

struct uint128 : uint128_t {

	uint128() : uint128_t(0) {}

	uint128(const uint128&) = default;

	uint128(const uint64_t& value) : uint128_t(value) {}

	uint128(const uint128_t& value) : uint128_t(value) {}

};


} //mmx


namespace vnx {

inline
void read(vnx::TypeInput& in, mmx::uint128& value, const vnx::TypeCode* type_code, const uint16_t* code)
{
	if(sizeof(value) != 16) {
		throw std::logic_error("sizeof(uint128) != 16");
	}
	switch(code[0]) {
		case CODE_STRING:
		case CODE_ALT_STRING: {
			std::string str;
			vnx::read(in, str, type_code, code);
			try {
				value = uint128_t(str, 10);
			} catch(...) {
				value = uint128_0;
			}
			break;
		}
		case CODE_UINT64:
		case CODE_ALT_UINT64: {
			uint64_t tmp = 0;
			vnx::read(in, tmp, type_code, code);
			value = uint128_t(tmp);
			break;
		}
		case CODE_DYNAMIC:
		case CODE_ALT_DYNAMIC: {
			vnx::Variant tmp;
			vnx::read(in, tmp, type_code, code);
			if(tmp.is_string()) {
				std::string str;
				tmp.to(str);
				try {
					value = uint128_t(str, 10);
				} catch(...) {
					value = uint128_0;
				}
			} else {
				std::array<uint8_t, 16> array = {};
				tmp.to(array);
				::memcpy(&value, array.data(), array.size());
			}
			break;
		}
		default:
			vnx::read(in, reinterpret_cast<std::array<uint8_t, 16>&>(value), type_code, code);
	}
}

inline
void write(vnx::TypeOutput& out, const mmx::uint128& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr)
{
	::memcpy(out.write(16), &value, 16);
}

inline
void read(std::istream& in, mmx::uint128& value)
{
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
