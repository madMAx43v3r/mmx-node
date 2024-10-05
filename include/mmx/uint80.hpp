/*
 * uint80.hpp
 *
 *  Created on: Oct 5, 2024
 *      Author: mad
 */

#ifndef INCLUDE_MMX_UINT80_HPP_
#define INCLUDE_MMX_UINT80_HPP_

#include <mmx/uint128.hpp>


namespace mmx {

class uint80 : public uint128 {
public:
	uint80() = default;

	uint80(const uint80&) = default;

	uint80(const uint64_t& value) : uint128(value) {}

	uint80(const uint128_t& value) : uint128(value) {}

	uint80(const uint256_t& value) : uint128(value) {}

	uint80(const std::string& str) : uint128(str) {}

};


} //mmx


namespace vnx {

void read(vnx::TypeInput& in, mmx::uint80& value, const vnx::TypeCode* type_code, const uint16_t* code);

void write(vnx::TypeOutput& out, const mmx::uint80& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr);

inline
void read(std::istream& in, mmx::uint80& value)
{
	std::string tmp;
	vnx::read(in, tmp);
	value = mmx::uint80(tmp);
}

inline
void write(std::ostream& out, const mmx::uint80& value) {
	vnx::write(out, value.to_string());
}

inline
void accept(vnx::Visitor& visitor, const mmx::uint80& value) {
	vnx::accept(visitor, value.to_string());
}

} // vnx

#endif /* INCLUDE_MMX_UINT80_HPP_ */
