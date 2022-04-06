/*
 * signature_t.cpp
 *
 *  Created on: Apr 6, 2022
 *      Author: mad
 */

#include <mmx/signature_t.hpp>


namespace mmx {

thread_local std::array<signature_t::cache_t, 16384> signature_t::sig_cache;

signature_t::signature_t(const secp256k1_ecdsa_signature& sig)
{
	secp256k1_ecdsa_signature_serialize_compact(g_secp256k1, bytes.data(), &sig);
}

secp256k1_ecdsa_signature signature_t::to_secp256k1() const
{
	secp256k1_ecdsa_signature res;
	if(!secp256k1_ecdsa_signature_parse_compact(g_secp256k1, &res, bytes.data())) {
		throw std::logic_error("secp256k1_ecdsa_signature_parse_compact() failed");
	}
	return res;
}

signature_t signature_t::sign(const skey_t& skey, const hash_t& hash)
{
	secp256k1_ecdsa_signature sig;
	if(!secp256k1_ecdsa_sign(g_secp256k1, &sig, hash.data(), skey.bytes.data(), NULL, NULL)) {
		throw std::logic_error("secp256k1_ecdsa_sign() failed");
	}
	return signature_t(sig);
}

bool signature_t::verify(const pubkey_t& pubkey, const hash_t& hash) const
{
	const auto sig_hash = std::hash<typename signature_t::super_t>{}(*this);

	auto& entry = sig_cache[sig_hash % sig_cache.size()];
	if(entry.sig == *this && entry.pubkey == pubkey && entry.hash == hash) {
		return true;
	}
	const auto sig = to_secp256k1();
	const auto key = pubkey.to_secp256k1();
	const bool res = secp256k1_ecdsa_verify(g_secp256k1, &sig, hash.data(), &key);
	if(res) {
		entry.sig = *this;
		entry.hash = hash;
		entry.pubkey = pubkey;
	}
	return res;
}


} // mmx
