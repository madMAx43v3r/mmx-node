/*
 * bls_pubkey_t.hpp
 *
 *  Created on: Nov 30, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_BLS_PUBKEY_T_HPP_
#define INCLUDE_MMX_BLS_PUBKEY_T_HPP_

#include <mmx/hash_t.h>
#include <mmx/skey_t.hpp>

#include <vnx/Input.hpp>
#include <vnx/Output.hpp>

#include <bls.hpp>


namespace mmx {

struct bls_pubkey_t {

	std::array<uint8_t, 48> bytes = {};

	bls_pubkey_t() = default;


	bls_pubkey_t(const bls::G1Element& key);

	hash_t get_addr() const;

	std::string to_string() const;

	bls::G1Element to_bls() const;

	bool operator==(const bls_pubkey_t& other) const {
		return bytes == other.bytes;
	}

	bool operator!=(const bls_pubkey_t& other) const {
		return bytes != other.bytes;
	}

	static bls_pubkey_t from_string(const std::string& str);

};


inline
bls_pubkey_t::bls_pubkey_t(const bls::G1Element& key)
{
	const auto tmp = key.Serialize();
	if(tmp.size() != bytes.size()) {
		throw std::logic_error("key size mismatch");
	}
	::memcpy(bytes.data(), tmp.data(), tmp.size());
}

inline
bls::G1Element bls_pubkey_t::to_bls() const
{
	return bls::G1Element::FromBytes(bls::Bytes(bytes.data(), bytes.size()));
}

inline
hash_t bls_pubkey_t::get_addr() const
{
	return hash_t(bytes.data(), bytes.size());
}

inline
std::string bls_pubkey_t::to_string() const
{
	return bls::Util::HexStr(bytes.data(), bytes.size());
}

inline
bls_pubkey_t bls_pubkey_t::from_string(const std::string& str)
{
	bls_pubkey_t res;
	const auto bytes = bls::Util::HexToBytes(str);
	if(bytes.size() != res.bytes.size()) {
		throw std::logic_error("key size mismatch");
	}
	::memcpy(res.bytes.data(), bytes.data(), bytes.size());
	return res;
}

inline
std::ostream& operator<<(std::ostream& out, const bls_pubkey_t& key) {
	return out << "0x" << key.to_string();
}

} // mmx


namespace vnx {

inline
void read(vnx::TypeInput& in, mmx::bls_pubkey_t& value, const vnx::TypeCode* type_code, const uint16_t* code) {
	vnx::read(in, value.bytes, type_code, code);
}

inline
void write(vnx::TypeOutput& out, const mmx::bls_pubkey_t& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr) {
	vnx::write(out, value.bytes, type_code, code);
}

inline
void read(std::istream& in, mmx::bls_pubkey_t& value) {
	std::string tmp;
	vnx::read(in, tmp);
	value = mmx::bls_pubkey_t::from_string(tmp);
}

inline
void write(std::ostream& out, const mmx::bls_pubkey_t& value) {
	vnx::write(out, value.to_string());
}

inline
void accept(vnx::Visitor& visitor, const mmx::bls_pubkey_t& value) {
	vnx::accept(visitor, value.bytes);
}


} // vnx


namespace std {
	template<> struct hash<mmx::bls_pubkey_t> {
		size_t operator()(const mmx::bls_pubkey_t& x) const {
			return *((const size_t*)x.bytes.data());
		}
	};
} // std

#endif /* INCLUDE_MMX_BLS_PUBKEY_T_HPP_ */
