/*
 * skey_t.hpp
 *
 *  Created on: Nov 25, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_SKEY_T_HPP_
#define INCLUDE_MMX_SKEY_T_HPP_

#include <mmx/bytes_t.hpp>

#include <bls.hpp>


namespace mmx {

struct skey_t : bytes_t<32> {

	typedef bytes_t<32> super_t;

	skey_t() = default;

	skey_t(const hash_t& hash);

	skey_t(const bls::PrivateKey& key);

};


inline
skey_t::skey_t(const hash_t& hash)
	:	super_t(hash)
{
}

inline
skey_t::skey_t(const bls::PrivateKey& key)
{
	if(bls::PrivateKey::PRIVATE_KEY_SIZE != size()) {
		throw std::logic_error("key size mismatch");
	}
	key.Serialize(data());
}


} //mmx


namespace vnx {

inline
void read(vnx::TypeInput& in, mmx::skey_t& value, const vnx::TypeCode* type_code, const uint16_t* code) {
	vnx::read(in, (mmx::skey_t::super_t&)value, type_code, code);
}

inline
void write(vnx::TypeOutput& out, const mmx::skey_t& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr) {
	vnx::write(out, (const mmx::skey_t::super_t&)value, type_code, code);
}

inline
void read(std::istream& in, mmx::skey_t& value) {
	vnx::read(in, (mmx::skey_t::super_t&)value);
}

inline
void write(std::ostream& out, const mmx::skey_t& value) {
	vnx::write(out, value.to_string());
}

inline
void accept(vnx::Visitor& visitor, const mmx::skey_t& value) {
	vnx::accept(visitor, (const mmx::skey_t::super_t&)value);
}

} // vnx

#endif /* INCLUDE_MMX_SKEY_T_HPP_ */
