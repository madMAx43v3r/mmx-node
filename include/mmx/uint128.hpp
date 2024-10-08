/*
 * uint128.hpp
 *
 *  Created on: Apr 11, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_UINT128_HPP_
#define INCLUDE_MMX_UINT128_HPP_

#include <mmx/package.hxx>

#include <uint128_t.h>
#include <uint256_t.h>

#include <vnx/Input.hpp>
#include <vnx/Output.hpp>
#include <vnx/Visitor.h>

#include <cmath>


namespace mmx {

class uint128 : public uint128_t {
public:
	uint128() : uint128_t(0) {}

	uint128(const uint128&) = default;

	uint128(const uint64_t& value) : uint128_t(value) {}

	uint128(const uint128_t& value) : uint128_t(value) {}

	uint128(const uint256_t& value) : uint128_t(value.lower()) {}

	uint128(const std::string& str) {
		if(str.substr(0, 2) == "0x") {
			*this = uint128_t(str.substr(2), 16);
		} else {
			*this = uint128_t(str, 10);
		}
	}

	std::string to_string() const {
		return str(10);
	}

	std::string to_hex_string() const {
		return "0x" + str(16);
	}

	double to_double() const {
		return double(upper()) * pow(2, 64) + double(lower());
	}

};


} //mmx


namespace vnx {

void read(vnx::TypeInput& in, mmx::uint128& value, const vnx::TypeCode* type_code, const uint16_t* code);

void write(vnx::TypeOutput& out, const mmx::uint128& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr);

inline
void read(std::istream& in, mmx::uint128& value)
{
	std::string tmp;
	vnx::read(in, tmp);
	value = mmx::uint128(tmp);
}

inline
void write(std::ostream& out, const mmx::uint128& value) {
	vnx::write(out, value.to_string());
}

inline
void accept(vnx::Visitor& visitor, const mmx::uint128& value) {
	vnx::accept(visitor, value.to_string());
}

} // vnx

#endif /* INCLUDE_MMX_UINT128_HPP_ */
