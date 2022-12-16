/*
 * fixed128.cpp
 *
 *  Created on: Dec 15, 2022
 *      Author: mad
 */

#include <mmx/fixed128.hpp>

#include <vnx/Variant.hpp>

#include <cmath>


namespace mmx {

static uint64_t calc_divider(const int decimals)
{
	uint64_t res = 1;
	for(int i = 0; i < decimals; ++i) {
		res *= 10;
	}
	return res;
}

const uint64_t fixed128::divider = calc_divider(fixed128::decimals);


fixed128::fixed128(const double& v)
{
	fixed = uint64_t(v);
	fixed *= divider;
	fixed += uint64_t(fmod(v, 1) * pow(10, decimals));
}

fixed128::fixed128(const std::string& str)
{
	const auto dec_pos = str.find_first_of(".,");
	const auto exp_pos = str.find_first_of("eE");
	if(dec_pos != std::string::npos || exp_pos != std::string::npos) {
		if(dec_pos != std::string::npos) {
			fixed = uint128_t(str.substr(0, dec_pos), 10);
		} else {
			fixed = uint128_t(str.substr(0, exp_pos), 10);
		}
		fixed *= divider;

		if(dec_pos != std::string::npos) {
			std::string fract;
			if(exp_pos != std::string::npos && exp_pos > dec_pos) {
				fract = str.substr(dec_pos + 1, exp_pos - dec_pos - 1);
			} else {
				fract = str.substr(dec_pos + 1);
			}
			fract.resize(std::min<size_t>(fract.size(), decimals));

			int num_zero = 0;
			for(const auto c : fract) {
				if(c == '0') {
					num_zero++;
				} else if(!std::isspace(c)) {
					break;
				}
			}
			if(auto lower = uint128_t(fract, 10)) {
				for(int i = 1 + num_zero; i < decimals; ++i) {
					lower *= 10;
				}
				fixed += lower;
			}
		}
		if(exp_pos != std::string::npos) {
			const auto exp_10 = vnx::from_string<int>(str.substr(exp_pos + 1));
			if(std::abs(exp_10) > decimals) {
				throw std::logic_error("out of range exponent: " + str.substr(exp_pos));
			}
			if(exp_10 >= 0) {
				for(int i = 0; i < exp_10; ++i) {
					fixed *= 10;
				}
			} else {
				for(int i = exp_10; i < 0; ++i) {
					fixed /= 10;
				}
			}
		}
	} else {
		fixed = uint128_t(str, 10) * divider;
	}
}

std::string fixed128::to_string() const
{
	auto str = uint().str(10);
	if(auto lower = fractional()) {
		str += '.';
		auto fract = std::to_string(lower);
		while(fract.size() && fract.back() == '0') {
			fract.pop_back();
		}
		str += fract;
	}
	return str;
}

double fixed128::to_value() const
{
	return uint().to_double() + double(fractional()) * pow(10, -decimals);
}

uint128 fixed128::to_amount(const int decimals) const
{
	uint128 amount = fixed;
	if(decimals < fixed128::decimals) {
		for(int i = decimals; i < fixed128::decimals; ++i) {
			amount /= 10;
		}
	} else {
		for(int i = fixed128::decimals; i < decimals; ++i) {
			amount *= 10;
		}
	}
	return amount;
}


} // mmx


namespace vnx {

void read(vnx::TypeInput& in, mmx::fixed128& value, const vnx::TypeCode* type_code, const uint16_t* code)
{
	switch(code[0]) {
		case CODE_STRING:
		case CODE_ALT_STRING: {
			std::string str;
			vnx::read(in, str, type_code, code);
			try {
				value = mmx::fixed128(str);
			} catch(...) {
				value = mmx::fixed128();
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
			value = mmx::fixed128(tmp);
			break;
		}
		case CODE_FLOAT:
		case CODE_DOUBLE:
		case CODE_ALT_FLOAT:
		case CODE_ALT_DOUBLE: {
			double tmp = 0;
			vnx::read(in, tmp, type_code, code);
			value = mmx::fixed128(tmp);
			break;
		}
		case CODE_DYNAMIC:
		case CODE_ALT_DYNAMIC: {
			vnx::Variant tmp;
			vnx::read(in, tmp, type_code, code);
			value = tmp.to<mmx::fixed128>();
			break;
		}
		default:
			vnx::read(in, value.fixed, type_code, code);
	}
}

} // vnx
