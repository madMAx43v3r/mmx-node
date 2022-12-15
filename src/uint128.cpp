/*
 * uint128.cpp
 *
 *  Created on: May 1, 2022
 *      Author: mad
 */

#include <mmx/uint128.hpp>

#include <vnx/Variant.hpp>

#include <algorithm>


namespace vnx {

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
				if(code[0] == CODE_ALT_DYNAMIC) {
					std::reverse(array.begin(), array.end());
				}
				::memcpy(&value, array.data(), array.size());
			}
			break;
		}
		default: {
			std::array<uint8_t, 16> array = {};
			vnx::read(in, array, type_code, code);
			switch(code[0]) {
				case CODE_ALT_LIST:
				case CODE_ALT_ARRAY:
					std::reverse(array.begin(), array.end());
			}
			::memcpy(&value, array.data(), array.size());
		}
	}
}

} // vnx
