/*
 * fixed128.hpp
 *
 *  Created on: Dec 15, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_FIXED128_HPP_
#define INCLUDE_MMX_FIXED128_HPP_

#include <mmx/uint128.hpp>

#include <vnx/Input.hpp>
#include <vnx/Output.hpp>
#include <vnx/Visitor.h>


namespace mmx {

class fixed128 {
public:
	uint128 value = 0;

	fixed128() = default;

	fixed128(const uint128_t& v) {
		value = v * divider;
	}

	fixed128(const double& v);

	fixed128(const std::string& str);

	uint128 uint() const {
		return value / divider;
	}

	uint64_t fractional() const {
		return value % divider;
	}

	uint128_t to_amount(const int decimals) const;

	std::string to_string() const;

	double to_double() const;

	operator bool() const {
		return bool(value);
	}

	static const uint64_t divider;

	static constexpr int decimals = 18;

};


} // mmx


namespace vnx {

void read(vnx::TypeInput& in, mmx::fixed128& value, const vnx::TypeCode* type_code, const uint16_t* code);

inline
void write(vnx::TypeOutput& out, const mmx::fixed128& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr) {
	write(out, value.value);
}

inline
void read(std::istream& in, mmx::fixed128& value)
{
	std::string str;
	vnx::read(in, str);
	value = mmx::fixed128(str);
}

inline
void write(std::ostream& out, const mmx::fixed128& value) {
	vnx::write(out, value.to_string());
}

inline
void accept(vnx::Visitor& visitor, const mmx::fixed128& value) {
	vnx::accept(visitor, value.to_string());
}

} // vnx

#endif /* INCLUDE_MMX_FIXED128_HPP_ */
