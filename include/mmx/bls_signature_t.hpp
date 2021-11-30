/*
 * bls_bls_signature_t.hpp
 *
 *  Created on: Nov 30, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_BLS_bls_signature_t_HPP_
#define INCLUDE_MMX_BLS_bls_signature_t_HPP_

#include <mmx/hash_t.hpp>
#include <mmx/bls_pubkey_t.hpp>

#include <vnx/Input.hpp>
#include <vnx/Output.hpp>

#include <bls.hpp>


namespace mmx {

struct bls_signature_t {

	std::array<uint8_t, 96> bytes = {};

	bls_signature_t() = default;

	bls_signature_t(const bls::G2Element& sig);

	bool verify(const bls_pubkey_t& pubkey, const hash_t& hash) const;

	std::string to_string() const;

	bls::G2Element to_bls() const;

	bool operator==(const bls_signature_t& other) const {
		return bytes == other.bytes;
	}

	bool operator!=(const bls_signature_t& other) const {
		return bytes != other.bytes;
	}

	static bls_signature_t from_string(const std::string& str);

	static bls_signature_t sign(const bls::PrivateKey& skey, const hash_t& hash);

};


inline
bls_signature_t::bls_signature_t(const bls::G2Element& sig)
{
	const auto tmp = sig.Serialize();
	if(tmp.size() != bytes.size()) {
		throw std::logic_error("signature size mismatch");
	}
	::memcpy(bytes.data(), tmp.data(), tmp.size());
}

inline
bool bls_signature_t::verify(const bls_pubkey_t& pubkey, const hash_t& hash) const
{
	bls::AugSchemeMPL MPL;
	return MPL.Verify(pubkey.to_bls(), bls::Bytes(hash.bytes.data(), hash.bytes.size()), to_bls());
}

inline
std::string bls_signature_t::to_string() const
{
	return bls::Util::HexStr(bytes.data(), bytes.size());
}

inline
bls::G2Element bls_signature_t::to_bls() const
{
	return bls::G2Element::FromBytes(bls::Bytes(bytes.data(), bytes.size()));
}

inline
bls_signature_t bls_signature_t::from_string(const std::string& str)
{
	bls_signature_t res;
	const auto bytes = bls::Util::HexToBytes(str);
	if(bytes.size() != res.bytes.size()) {
		throw std::logic_error("signature size mismatch");
	}
	::memcpy(res.bytes.data(), bytes.data(), bytes.size());
	return res;
}

inline
bls_signature_t bls_signature_t::sign(const bls::PrivateKey& skey, const hash_t& hash)
{
	bls::AugSchemeMPL MPL;
	return MPL.Sign(skey, bls::Bytes(hash.bytes.data(), hash.bytes.size()));
}

inline
std::ostream& operator<<(std::ostream& out, const bls_signature_t& sig) {
	return out << "0x" << sig.to_string();
}

} // mmx


namespace vnx {

inline
void read(vnx::TypeInput& in, mmx::bls_signature_t& value, const vnx::TypeCode* type_code, const uint16_t* code) {
	vnx::read(in, value.bytes, type_code, code);
}

inline
void write(vnx::TypeOutput& out, const mmx::bls_signature_t& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr) {
	vnx::write(out, value.bytes, type_code, code);
}

inline
void read(std::istream& in, mmx::bls_signature_t& value) {
	std::string tmp;
	vnx::read(in, tmp);
	value = mmx::bls_signature_t::from_string(tmp);
}

inline
void write(std::ostream& out, const mmx::bls_signature_t& value) {
	vnx::write(out, value.to_string());
}

inline
void accept(vnx::Visitor& visitor, const mmx::bls_signature_t& value) {
	vnx::accept(visitor, value.bytes);
}

} // vnx

#endif /* INCLUDE_MMX_BLS_bls_signature_t_HPP_ */
