/*
 * pubkey_t.hpp
 *
 *  Created on: Nov 25, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_PUBKEY_T_HPP_
#define INCLUDE_MMX_PUBKEY_T_HPP_

#include <mmx/hash_t.hpp>
#include <mmx/skey_t.hpp>
#include <mmx/secp256k1.hpp>


namespace mmx {

class pubkey_t : public bytes_t<33> {
public:
	typedef bytes_t<33> super_t;

	pubkey_t() = default;

	pubkey_t(const secp256k1_pubkey& key);

	hash_t get_addr() const;

	secp256k1_pubkey to_secp256k1() const;

	static pubkey_t from_skey(const skey_t& key);

};


inline
pubkey_t::pubkey_t(const secp256k1_pubkey& key)
{
	size_t len = size();
	secp256k1_ec_pubkey_serialize(g_secp256k1, data(), &len, &key, SECP256K1_EC_COMPRESSED);
	if(len != 33) {
		throw std::logic_error("secp256k1_ec_pubkey_serialize(): length != 33");
	}
}

inline
secp256k1_pubkey pubkey_t::to_secp256k1() const
{
	secp256k1_pubkey res;
	if(!secp256k1_ec_pubkey_parse(g_secp256k1, &res, data(), size())) {
		throw std::logic_error("secp256k1_ec_pubkey_parse() failed");
	}
	return res;
}

inline
hash_t pubkey_t::get_addr() const
{
	return hash_t(bytes);
}

inline
pubkey_t pubkey_t::from_skey(const skey_t& key)
{
	secp256k1_pubkey pubkey;
	if(!secp256k1_ec_pubkey_create(g_secp256k1, &pubkey, key.data())) {
		throw std::logic_error("secp256k1_ec_pubkey_create() failed");
	}
	return pubkey_t(pubkey);
}


} // mmx


namespace vnx {

inline
void read(vnx::TypeInput& in, mmx::pubkey_t& value, const vnx::TypeCode* type_code, const uint16_t* code) {
	vnx::read(in, (mmx::pubkey_t::super_t&)value, type_code, code);
}

inline
void write(vnx::TypeOutput& out, const mmx::pubkey_t& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr) {
	vnx::write(out, (const mmx::pubkey_t::super_t&)value, type_code, code);
}

inline
void read(std::istream& in, mmx::pubkey_t& value) {
	vnx::read(in, (mmx::pubkey_t::super_t&)value);
}

inline
void write(std::ostream& out, const mmx::pubkey_t& value) {
	vnx::write(out, value.to_string());
}

inline
void accept(vnx::Visitor& visitor, const mmx::pubkey_t& value) {
	vnx::accept(visitor, (const mmx::pubkey_t::super_t&)value);
}

} // vnx


namespace std {
	template<>
	struct hash<typename mmx::pubkey_t> {
		size_t operator()(const mmx::pubkey_t& x) const {
			return std::hash<mmx::pubkey_t::super_t>{}(x);
		}
	};
} // std

#endif /* INCLUDE_MMX_PUBKEY_T_HPP_ */
