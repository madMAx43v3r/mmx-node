/*
 * bls_pubkey_t.hpp
 *
 *  Created on: Nov 30, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_BLS_PUBKEY_T_HPP_
#define INCLUDE_MMX_BLS_PUBKEY_T_HPP_

#include <mmx/hash_t.hpp>
#include <mmx/skey_t.hpp>


namespace mmx {

struct bls_pubkey_t : public bytes_t<48> {

	bls_pubkey_t() = default;

	bls_pubkey_t(const bls::G1Element& key);

	hash_t get_addr() const;

	bls::G1Element to_bls() const;

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
	value.from_string(tmp);
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

#endif /* INCLUDE_MMX_BLS_PUBKEY_T_HPP_ */
