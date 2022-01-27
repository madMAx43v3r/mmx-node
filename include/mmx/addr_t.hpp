/*
 * addr_t.hpp
 *
 *  Created on: Dec 12, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_ADDR_T_HPP_
#define INCLUDE_MMX_ADDR_T_HPP_

#include <mmx/hash_t.hpp>
#include <mmx/pubkey_t.hpp>

#include <libbech32.h>


namespace mmx {

struct addr_t : hash_t {

	typedef hash_t super_t;

	addr_t() = default;

	addr_t(const hash_t& hash);

	addr_t(const pubkey_t& key);

	addr_t(const std::string& addr);

	std::string to_string() const;

	void from_string(const std::string& addr);

};


inline
addr_t::addr_t(const hash_t& hash)
	:	super_t(hash)
{
}

inline
addr_t::addr_t(const pubkey_t& key)
	:	super_t(key.get_addr())
{
}

inline
addr_t::addr_t(const std::string& addr)
{
	from_string(addr);
}

inline
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

inline
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

inline
std::ostream& operator<<(std::ostream& out, const addr_t& addr) {
	return out << addr.to_string();
}


} //mmx


namespace vnx {

inline
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
			tmp.to(value);
			break;
		}
		default:
			vnx::read(in, value.bytes, type_code, code);
	}
}

inline
void write(vnx::TypeOutput& out, const mmx::addr_t& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr) {
	vnx::write(out, value.bytes, type_code, code);
}

inline
void read(std::istream& in, mmx::addr_t& value) {
	std::string tmp;
	vnx::read(in, tmp);
	value.from_string(tmp);
}

inline
void write(std::ostream& out, const mmx::addr_t& value) {
	vnx::write(out, value.to_string());
}

inline
void accept(vnx::Visitor& visitor, const mmx::addr_t& value) {
	vnx::accept(visitor, value.to_string());
}

} // vnx


namespace std {
	template<>
	struct hash<typename mmx::addr_t> {
		size_t operator()(const mmx::addr_t& x) const {
			return std::hash<mmx::hash_t>{}(x);
		}
	};
} // std

#endif /* INCLUDE_MMX_ADDR_T_HPP_ */
