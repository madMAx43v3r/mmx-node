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

#include <mutex>


namespace mmx {

struct signature_t : bytes_t<64> {

	typedef bytes_t<64> super_t;

	signature_t() = default;

	signature_t(const secp256k1_ecdsa_signature& sig);

	bool verify(const pubkey_t& pubkey, const hash_t& hash) const;

	secp256k1_ecdsa_signature to_secp256k1() const;

	static signature_t sign(const skey_t& skey, const hash_t& hash);

private:
	struct cache_t {
		bytes_t<64> sig;
		hash_t hash;
		pubkey_t pubkey;
	};

	static std::mutex mutex;
	static std::array<cache_t, 16384> sig_cache;
};

} // mmx


namespace vnx {

inline
void read(vnx::TypeInput& in, mmx::signature_t& value, const vnx::TypeCode* type_code, const uint16_t* code) {
	vnx::read(in, (mmx::signature_t::super_t&)value, type_code, code);
}

inline
void write(vnx::TypeOutput& out, const mmx::signature_t& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr) {
	vnx::write(out, (const mmx::signature_t::super_t&)value, type_code, code);
}

inline
void read(std::istream& in, mmx::signature_t& value) {
	vnx::read(in, (mmx::signature_t::super_t&)value);
}

inline
void write(std::ostream& out, const mmx::signature_t& value) {
	vnx::write(out, value.to_string());
}

inline
void accept(vnx::Visitor& visitor, const mmx::signature_t& value) {
	vnx::accept(visitor, (const mmx::signature_t::super_t&)value);
}

} // vnx


namespace std {
	template<>
	struct hash<typename mmx::signature_t> {
		size_t operator()(const mmx::signature_t& x) const {
			return std::hash<mmx::signature_t::super_t>{}(x);
		}
	};
} // std

#endif /* INCLUDE_MMX_SIGNATURE_T_HPP_ */
