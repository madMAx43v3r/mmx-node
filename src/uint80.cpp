/*
 * uint80.cpp
 *
 *  Created on: Oct 5, 2024
 *      Author: mad
 */

#include <mmx/uint80.hpp>
#include <mmx/bytes_t.hpp>


namespace vnx {

void read(vnx::TypeInput& in, mmx::uint80& value, const vnx::TypeCode* type_code, const uint16_t* code)
{
	switch(code[0]) {
		case CODE_ARRAY:
		case CODE_ALT_ARRAY: {
			mmx::bytes_t<10> tmp;
			vnx::read(in, tmp, type_code, code);
			value = tmp.to_uint<mmx::uint80>();
			break;
		}
		default: {
			mmx::uint128 tmp;
			vnx::read(in, tmp, type_code, code);
			value = mmx::uint80(tmp);
		}
	}
}

void write(vnx::TypeOutput& out, const mmx::uint80& value, const vnx::TypeCode* type_code, const uint16_t* code)
{
	mmx::bytes_t<10> tmp;
	tmp.from_uint(value);
	vnx::write(out, tmp, type_code, code);
}


} // vnx
