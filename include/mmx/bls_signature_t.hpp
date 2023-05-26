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

#include <mutex>

namespace bls {
	class G2Element;
}


namespace mmx {

class bls_signature_t : public bytes_t<96> {
public:
	typedef bytes_t<96> super_t;

	bls_signature_t() = default;

	bls_signature_t(const bls::G2Element& sig);

	bool verify(const bls_pubkey_t& pubkey, const hash_t& hash) const;

	void to(bls::G2Element& sig) const;

	static bls_signature_t sign(const skey_t& skey, const hash_t& hash);

	static bls_signature_t sign(const bls::PrivateKey& skey, const hash_t& hash);

private:
	struct cache_t {
		bytes_t<96> sig;
		hash_t hash;
		bls_pubkey_t pubkey;
	};

	static std::mutex mutex;
	static const size_t hash_salt;
	static std::array<cache_t, 2048> sig_cache;
};

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
