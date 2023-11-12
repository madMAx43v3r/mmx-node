/*
 * uint128.cpp
 *
 *  Created on: May 1, 2022
 *      Author: mad
 */

#include <mmx/uint128.hpp>
#include <mmx/bytes_t.hpp>

#include <vnx/Variant.hpp>

#include <algorithm>


namespace vnx {

void read(vnx::TypeInput& in, mmx::uint128& value, const vnx::TypeCode* type_code, const uint16_t* code)
{
	switch(code[0]) {
		case CODE_STRING:
		case CODE_ALT_STRING: {
			std::string str;
			vnx::read(in, str, type_code, code);
			try {
				if(str.substr(0, 2) == "0x") {
					value = uint128_t(str.substr(2), 16);
				} else {
					value = uint128_t(str, 10);
				}
			} catch(...) {
				value = uint128_0;
			}
			break;
		}
		case CODE_UINT8:
		case CODE_UINT16:
		case CODE_UINT32:
		case CODE_UINT64:
		case CODE_ALT_UINT8:
		case CODE_ALT_UINT16:
		case CODE_ALT_UINT32:
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
			value = tmp.to<mmx::uint128>();
			break;
		}
		default: {
			mmx::bytes_t<16> tmp;
			vnx::read(in, tmp, type_code, code);
			value = tmp.to_uint<mmx::uint128>();
		}
	}
}

void write(vnx::TypeOutput& out, const mmx::uint128& value, const vnx::TypeCode* type_code, const uint16_t* code)
{
	mmx::bytes_t<16> tmp;
	tmp.from_uint(value);
	vnx::write(out, tmp, type_code, code);
}


} // vnx
