/*
 * addr_t.cpp
 *
 *  Created on: Apr 30, 2022
 *      Author: mad
 */

#include <mmx/addr_t.hpp>
#include <mmx/vm/var_t.h>

#include <libbech32.h>


namespace mmx {

std::string addr_t::to_string() const
{
	auto bits = to_uint256();
	std::vector<uint8_t> dp(52);
	dp[51] = (bits & 1) << 4;
	bits >>= 1;
	for(int i = 0; i < 51; ++i) {
		dp[50 - i] = bits & 0x1F;
		bits >>= 5;
	}
	return bech32::encode("mmx", dp);
}

void addr_t::from_string(const std::string& addr)
{
	if(addr == "MMX") {
		*this = addr_t();
		return;
	}
	const auto res = bech32::decode(addr);
	if(res.encoding != bech32::Bech32m) {
		throw std::logic_error("invalid address: " + addr);
	}
	if(res.dp.size() != 52) {
		throw std::logic_error("invalid address (size != 52): " + addr);
	}
	uint256_t bits = 0;
	for(int i = 0; i < 50; ++i) {
		bits |= res.dp[i] & 0x1F;
		bits <<= 5;
	}
	bits |= res.dp[50] & 0x1F;
	bits <<= 1;
	bits |= (res.dp[51] >> 4) & 1;
	::memcpy(data(), &bits, 32);
}


namespace vm {

addr_t to_addr(const var_t* var)
{
	if(!var) {
		return addr_t();
	}
	switch(var->type) {
		case TYPE_UINT:
			return addr_t(((const uint_t*)var)->value);
		case TYPE_STRING:
			return addr_t(((const binary_t*)var)->to_string());
		default:
			return addr_t();
	}
}

} // vm
} // mmx


namespace vnx {

void read(vnx::TypeInput& in, mmx::addr_t& value, const vnx::TypeCode* type_code, const uint16_t* code) {
	switch(code[0]) {
		case CODE_STRING:
		case CODE_ALT_STRING: {
			std::string tmp;
			vnx::read(in, tmp, type_code, code);
			try {
				value.from_string(tmp);
			} catch(...) {
				value = mmx::addr_t();
			}
			break;
		}
		case CODE_DYNAMIC:
		case CODE_ALT_DYNAMIC: {
			vnx::Variant tmp;
			vnx::read(in, tmp, type_code, code);
			if(tmp.is_string()) {
				try {
					value.from_string(tmp.to<std::string>());
				} catch(...) {
					value = mmx::addr_t();
				}
			} else {
				tmp.to(value.bytes);
			}
			break;
		}
		default:
			vnx::read(in, value.bytes, type_code, code);
	}
}

} // vnx
