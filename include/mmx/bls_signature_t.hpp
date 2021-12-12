/*
 * bls_bls_signature_t.hpp
 *
 *  Created on: Nov 30, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_BLS_bls_signature_t_HPP_
#define INCLUDE_MMX_BLS_bls_signature_t_HPP_

#include <mmx/hash_t.hpp>
#include <mmx/skey_t.hpp>
#include <mmx/bls_pubkey_t.hpp>


namespace mmx {

struct bls_signature_t : bytes_t<96> {

	typedef bytes_t<96> super_t;

	bls_signature_t() = default;

	bls_signature_t(const bls::G2Element& sig);

	bool verify(const bls_pubkey_t& pubkey, const hash_t& hash) const;

	bls::G2Element to_bls() const;

	static bls_signature_t sign(const skey_t& skey, const hash_t& hash);

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
bls::G2Element bls_signature_t::to_bls() const
{
	return bls::G2Element::FromBytes(bls::Bytes(bytes.data(), bytes.size()));
}

inline
bls_signature_t bls_signature_t::sign(const skey_t& skey, const hash_t& hash)
{
	const auto bls_skey = bls::PrivateKey::FromBytes(bls::Bytes(skey.data(), skey.size()));
	return sign(bls_skey, hash);
}

inline
bls_signature_t bls_signature_t::sign(const bls::PrivateKey& skey, const hash_t& hash)
{
	bls::AugSchemeMPL MPL;
	return MPL.Sign(skey, bls::Bytes(hash.bytes.data(), hash.bytes.size()));
}

} // mmx


namespace vnx {

inline
void read(vnx::TypeInput& in, mmx::bls_signature_t& value, const vnx::TypeCode* type_code, const uint16_t* code) {
	vnx::read(in, (mmx::bls_signature_t::super_t&)value, type_code, code);
}

inline
void write(vnx::TypeOutput& out, const mmx::bls_signature_t& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr) {
	vnx::write(out, (const mmx::bls_signature_t::super_t&)value, type_code, code);
}

inline
void read(std::istream& in, mmx::bls_signature_t& value) {
	vnx::read(in, (mmx::bls_signature_t::super_t&)value);
}

inline
void write(std::ostream& out, const mmx::bls_signature_t& value) {
	vnx::write(out, value.to_string());
}

inline
void accept(vnx::Visitor& visitor, const mmx::bls_signature_t& value) {
	vnx::accept(visitor, (const mmx::bls_signature_t::super_t&)value);
}

} // vnx

#endif /* INCLUDE_MMX_BLS_bls_signature_t_HPP_ */
