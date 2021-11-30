/*
 * signature_t.hpp
 *
 *  Created on: Nov 25, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_SIGNATURE_T_HPP_
#define INCLUDE_MMX_SIGNATURE_T_HPP_

#include <mmx/hash_t.hpp>
#include <mmx/skey_t.hpp>
#include <mmx/pubkey_t.hpp>
#include <mmx/secp256k1.hpp>

#include <vnx/Input.hpp>
#include <vnx/Output.hpp>

#include <bls.hpp>


namespace mmx {

struct signature_t {

	std::array<uint8_t, 64> bytes = {};

	signature_t() = default;

	signature_t(const secp256k1_ecdsa_signature& sig);

	bool verify(const pubkey_t& pubkey, const hash_t& hash) const;

	std::string to_string() const;

	secp256k1_ecdsa_signature to_secp256k1() const;

	bool operator==(const signature_t& other) const {
		return bytes == other.bytes;
	}

	bool operator!=(const signature_t& other) const {
		return bytes != other.bytes;
	}

	static signature_t from_string(const std::string& str);

	static signature_t sign(const skey_t& skey, const hash_t& hash);

};


inline
signature_t::signature_t(const secp256k1_ecdsa_signature& sig)
{
	secp256k1_ecdsa_signature_serialize_compact(g_secp256k1, bytes.data(), &sig);
}

inline
bool signature_t::verify(const pubkey_t& pubkey, const hash_t& hash) const
{
	const auto sig = to_secp256k1();
	const auto key = pubkey.to_secp256k1();
	return secp256k1_ecdsa_verify(g_secp256k1, &sig, hash.data(), &key);
}

inline
std::string signature_t::to_string() const
{
	return bls::Util::HexStr(bytes.data(), bytes.size());
}

inline
secp256k1_ecdsa_signature signature_t::to_secp256k1() const
{
	secp256k1_ecdsa_signature res;
	if(!secp256k1_ecdsa_signature_parse_compact(g_secp256k1, &res, bytes.data())) {
		throw std::logic_error("secp256k1_ecdsa_signature_parse_compact() failed");
	}
	return res;
}

inline
signature_t signature_t::from_string(const std::string& str)
{
	signature_t res;
	const auto bytes = bls::Util::HexToBytes(str);
	if(bytes.size() != res.bytes.size()) {
		throw std::logic_error("signature size mismatch");
	}
	::memcpy(res.bytes.data(), bytes.data(), bytes.size());
	return res;
}

inline
signature_t signature_t::sign(const skey_t& skey, const hash_t& hash)
{
	secp256k1_ecdsa_signature sig;
	if(!secp256k1_ecdsa_sign(g_secp256k1, &sig, hash.data(), skey.bytes.data(), NULL, NULL)) {
		throw std::logic_error("secp256k1_ecdsa_sign() failed");
	}
	return signature_t(sig);
}

inline
std::ostream& operator<<(std::ostream& out, const signature_t& sig) {
	return out << "0x" << sig.to_string();
}

} // mmx


namespace vnx {

inline
void read(vnx::TypeInput& in, mmx::signature_t& value, const vnx::TypeCode* type_code, const uint16_t* code) {
	vnx::read(in, value.bytes, type_code, code);
}

inline
void write(vnx::TypeOutput& out, const mmx::signature_t& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr) {
	vnx::write(out, value.bytes, type_code, code);
}

inline
void read(std::istream& in, mmx::signature_t& value) {
	std::string tmp;
	vnx::read(in, tmp);
	value = mmx::signature_t::from_string(tmp);
}

inline
void write(std::ostream& out, const mmx::signature_t& value) {
	vnx::write(out, value.to_string());
}

inline
void accept(vnx::Visitor& visitor, const mmx::signature_t& value) {
	vnx::accept(visitor, value.bytes);
}

} // vnx

#endif /* INCLUDE_MMX_SIGNATURE_T_HPP_ */
