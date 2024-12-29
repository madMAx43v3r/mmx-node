/*
 * signature_t.cpp
 *
 *  Created on: Apr 6, 2022
 *      Author: mad
 */

#include <mmx/signature_t.hpp>


namespace mmx {

std::mutex signature_t::mutex;
const vnx::Hash64 signature_t::hash_salt = vnx::Hash64::rand();
std::array<signature_t::cache_t, 16384> signature_t::sig_cache;

signature_t::signature_t(const secp256k1_ecdsa_signature& sig)
{
	secp256k1_ecdsa_signature_serialize_compact(g_secp256k1, data(), &sig);
}

secp256k1_ecdsa_signature signature_t::to_secp256k1() const
{
	secp256k1_ecdsa_signature res;
	if(!secp256k1_ecdsa_signature_parse_compact(g_secp256k1, &res, data())) {
		throw std::logic_error("secp256k1_ecdsa_signature_parse_compact() failed");
	}
	return res;
}

signature_t signature_t::sign(const skey_t& skey, const hash_t& hash)
{
	secp256k1_ecdsa_signature sig;
	if(!secp256k1_ecdsa_sign(g_secp256k1, &sig, hash.data(), skey.data(), NULL, NULL)) {
		throw std::logic_error("secp256k1_ecdsa_sign() failed");
	}
	return signature_t(sig);
}

signature_t signature_t::normalized() const
{
	secp256k1_ecdsa_signature out;
	const auto sig = to_secp256k1();
	secp256k1_ecdsa_signature_normalize(g_secp256k1, &out, &sig);
	return signature_t(out);
}

bool signature_t::verify(const pubkey_t& pubkey, const hash_t& hash) const
{
	const size_t sig_hash = vnx::Hash64(crc64(), hash_salt);

	auto& entry = sig_cache[sig_hash % sig_cache.size()];
	{
		std::lock_guard lock(mutex);
		if(entry.sig == *this && entry.pubkey == pubkey && entry.hash == hash) {
			return true;
		}
	}
	const auto sig = to_secp256k1();
	const auto key = pubkey.to_secp256k1();
	const bool res = secp256k1_ecdsa_verify(g_secp256k1, &sig, hash.data(), &key);
	if(res) {
		std::lock_guard lock(mutex);
		entry.sig = *this;
		entry.hash = hash;
		entry.pubkey = pubkey;
	}
	return res;
}


} // mmx
