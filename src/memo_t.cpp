/*
 * memo_t.cpp
 *
 *  Created on: May 19, 2023
 *      Author: mad
 */

#include <mmx/memo_t.hpp>


namespace mmx {

memo_t::memo_t(const hash_t& hash)
{
	::memcpy(data(), hash.data(), size());
}

memo_t::memo_t(const uint256_t& value)
{
	auto tmp = value;
	for(size_t i = 0; i < size(); ++i) {
		bytes[size() - i - 1] = tmp & 0xFF;
		tmp >>= 8;
	}
}

memo_t::memo_t(const std::string& str)
{
	if(str.size() == 2 + 2 * size() && str.substr(0, 2) == "0x") {
		const auto tmp = vnx::from_hex_string(str);
		if(tmp.size() != size()) {
			throw std::logic_error("invalid binary memo");
		}
		::memcpy(data(), tmp.data(), size());
	} else {
		if(str.size() > size() - 1) {
			throw std::logic_error("memo string length > " + std::to_string(size() - 1));
		}
		::memcpy(data(), str.c_str(), str.size());
	}
}

uint256_t memo_t::to_uint() const
{
	uint256_t out = 0;
	for(size_t i = 0; i < size(); ++i) {
		out <<= 8;
		out |= bytes[i];
	}
	return out;
}

std::string memo_t::to_string() const
{
	if(is_zero()) {
		return std::string();
	}
	bool is_uint = true;
	for(size_t i = 0; i < 12; ++i) {
		if(bytes[i]) {
			is_uint = false;
			break;
		}
	}
	if(is_uint) {
		return to_uint().str(10);
	}
	bool is_ascii = true;
	bool is_terminated = false;
	for(size_t i = 0; i < size(); ++i) {
		if(bytes[i] == 0) {
			is_terminated = true;
		} else if(bytes[i] < 32 || bytes[i] >= 127 || is_terminated) {
			is_ascii = false;
			break;
		}
	}
	if(is_ascii && is_terminated) {
		return std::string((const char*)data());
	}
	return "0x" + super_t::to_string();
}


} // mmx
