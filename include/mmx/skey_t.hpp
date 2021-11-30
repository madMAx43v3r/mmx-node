/*
 * skey_t.hpp
 *
 *  Created on: Nov 25, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_SKEY_T_HPP_
#define INCLUDE_MMX_SKEY_T_HPP_

#include <vnx/Input.hpp>
#include <vnx/Output.hpp>

#include <bls.hpp>


namespace mmx {

struct skey_t {

	std::array<uint8_t, 32> bytes = {};

	skey_t() = default;

	std::string to_string() const;

	bool operator==(const skey_t& other) const {
		return bytes == other.bytes;
	}

	bool operator!=(const skey_t& other) const {
		return bytes != other.bytes;
	}

	static skey_t from_string(const std::string& str);

};


inline
std::string skey_t::to_string() const
{
	return bls::Util::HexStr(bytes.data(), bytes.size());
}

inline
skey_t skey_t::from_string(const std::string& str)
{
	skey_t res;
	const auto bytes = bls::Util::HexToBytes(str);
	if(bytes.size() != res.bytes.size()) {
		throw std::logic_error("key size mismatch");
	}
	::memcpy(res.bytes.data(), bytes.data(), bytes.size());
	return res;
}

inline
std::ostream& operator<<(std::ostream& out, const skey_t& key) {
	return out << "0x" << key.to_string();
}

} //mmx


namespace vnx {

inline
void read(vnx::TypeInput& in, mmx::skey_t& value, const vnx::TypeCode* type_code, const uint16_t* code) {
	vnx::read(in, value.bytes, type_code, code);
}

inline
void write(vnx::TypeOutput& out, const mmx::skey_t& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr) {
	vnx::write(out, value.bytes, type_code, code);
}

inline
void read(std::istream& in, mmx::skey_t& value) {
	std::string tmp;
	vnx::read(in, tmp);
	value = mmx::skey_t::from_string(tmp);
}

inline
void write(std::ostream& out, const mmx::skey_t& value) {
	vnx::write(out, value.to_string());
}

inline
void accept(vnx::Visitor& visitor, const mmx::skey_t& value) {
	vnx::accept(visitor, value.bytes);
}

} // vnx

#endif /* INCLUDE_MMX_SKEY_T_HPP_ */
