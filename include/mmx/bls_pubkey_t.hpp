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

namespace bls {
	class G1Element;
}


namespace mmx {

class bls_pubkey_t : public bytes_t<48> {
public:
	typedef bytes_t<48> super_t;

	bls_pubkey_t() = default;

	bls_pubkey_t(const super_t& bytes);

	bls_pubkey_t(const bls::G1Element& key);

	hash_t get_addr() const;

	void to(bls::G1Element& key) const;

};

inline
bls_pubkey_t::bls_pubkey_t(const super_t& bytes)
	:	super_t(bytes)
{
}

} // mmx


namespace vnx {

inline
void read(vnx::TypeInput& in, mmx::bls_pubkey_t& value, const vnx::TypeCode* type_code, const uint16_t* code) {
	vnx::read(in, (mmx::bls_pubkey_t::super_t&)value, type_code, code);
}

inline
void write(vnx::TypeOutput& out, const mmx::bls_pubkey_t& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr) {
	vnx::write(out, (const mmx::bls_pubkey_t::super_t&)value, type_code, code);
}

inline
void read(std::istream& in, mmx::bls_pubkey_t& value) {
	vnx::read(in, (mmx::bls_pubkey_t::super_t&)value);
}

inline
void write(std::ostream& out, const mmx::bls_pubkey_t& value) {
	vnx::write(out, value.to_string());
}

inline
void accept(vnx::Visitor& visitor, const mmx::bls_pubkey_t& value) {
	vnx::accept(visitor, (const mmx::bls_pubkey_t::super_t&)value);
}

} // vnx


namespace std {
	template<>
	struct hash<typename mmx::bls_pubkey_t> {
		size_t operator()(const mmx::bls_pubkey_t& x) const {
			return std::hash<mmx::bls_pubkey_t::super_t>{}(x);
		}
	};
} // std

#endif /* INCLUDE_MMX_BLS_PUBKEY_T_HPP_ */
