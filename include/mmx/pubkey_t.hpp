/*
 * pubkey_t.hpp
 *
 *  Created on: Nov 25, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_PUBKEY_T_HPP_
#define INCLUDE_MMX_PUBKEY_T_HPP_

#include <mmx/hash_t.h>
#include <mmx/skey_t.hpp>
#include <mmx/secp256k1.hpp>

#include <vnx/Input.hpp>
#include <vnx/Output.hpp>

#include <bls.hpp>


namespace mmx {

struct pubkey_t {

	std::array<uint8_t, 33> bytes = {};

	pubkey_t() = default;


	pubkey_t(const secp256k1_pubkey& key);

	hash_t get_addr() const;

	std::string to_string() const;

	secp256k1_pubkey to_secp256k1() const;

	bool operator==(const pubkey_t& other) const {
		return bytes == other.bytes;
	}

	bool operator!=(const pubkey_t& other) const {
		return bytes != other.bytes;
	}

	static pubkey_t from_skey(const skey_t& key);

	static pubkey_t from_string(const std::string& str);

};


inline
pubkey_t::pubkey_t(const secp256k1_pubkey& key)
{
	size_t len = bytes.size();
	secp256k1_ec_pubkey_serialize(g_secp256k1, bytes.data(), &len, &key, SECP256K1_EC_COMPRESSED);
	if(len != 33) {
		throw std::logic_error("secp256k1_ec_pubkey_serialize(): length != 33");
	}
}

inline
std::string pubkey_t::to_string() const
{
	return bls::Util::HexStr(bytes.data(), bytes.size());
}

inline
secp256k1_pubkey pubkey_t::to_secp256k1() const
{
	secp256k1_pubkey res;
	if(!secp256k1_ec_pubkey_parse(g_secp256k1, &res, bytes.data(), bytes.size())) {
		throw std::logic_error("secp256k1_ec_pubkey_parse() failed");
	}
	return res;
}

inline
hash_t pubkey_t::get_addr() const
{
	return hash_t(bytes.data(), bytes.size());
}

inline
pubkey_t pubkey_t::from_skey(const skey_t& key)
{
	secp256k1_pubkey pubkey;
	if(!secp256k1_ec_pubkey_create(g_secp256k1, &pubkey, key.bytes.data())) {
		throw std::logic_error("secp256k1_ec_pubkey_create() failed");
	}
	return pubkey_t(pubkey);
}

inline
pubkey_t pubkey_t::from_string(const std::string& str)
{
	pubkey_t res;
	const auto bytes = bls::Util::HexToBytes(str);
	if(bytes.size() != res.bytes.size()) {
		throw std::logic_error("key size mismatch");
	}
	::memcpy(res.bytes.data(), bytes.data(), bytes.size());
	return res;
}

inline
std::ostream& operator<<(std::ostream& out, const pubkey_t& key) {
	return out << "0x" << key.to_string();
}

} // mmx


namespace vnx {

inline
void read(vnx::TypeInput& in, mmx::pubkey_t& value, const vnx::TypeCode* type_code, const uint16_t* code) {
	vnx::read(in, value.bytes, type_code, code);
}

inline
void write(vnx::TypeOutput& out, const mmx::pubkey_t& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr) {
	vnx::write(out, value.bytes, type_code, code);
}

inline
void read(std::istream& in, mmx::pubkey_t& value) {
	std::string tmp;
	vnx::read(in, tmp);
	value = mmx::pubkey_t::from_string(tmp);
}

inline
void write(std::ostream& out, const mmx::pubkey_t& value) {
	vnx::write(out, value.to_string());
}

inline
void accept(vnx::Visitor& visitor, const mmx::pubkey_t& value) {
	vnx::accept(visitor, value.bytes);
}


} // vnx


namespace std {
	template<> struct hash<mmx::pubkey_t> {
		size_t operator()(const mmx::pubkey_t& x) const {
			return *((const size_t*)x.bytes.data());
		}
	};
} // std

#endif /* INCLUDE_MMX_PUBKEY_T_HPP_ */
