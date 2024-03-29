/*
 * bls_signature_t.cpp
 *
 *  Created on: Apr 6, 2022
 *      Author: mad
 */

#include <mmx/bls_signature_t.hpp>


namespace mmx {

std::mutex bls_signature_t::mutex;
const size_t bls_signature_t::hash_salt = vnx::rand64();
std::array<bls_signature_t::cache_t, 2048> bls_signature_t::sig_cache;

bls_signature_t::bls_signature_t(const bls::G2Element& sig)
{
	const auto tmp = sig.Serialize();
	if(tmp.size() != bytes.size()) {
		throw std::logic_error("signature size mismatch");
	}
	::memcpy(bytes.data(), tmp.data(), tmp.size());
}

bool bls_signature_t::verify(const bls_pubkey_t& pubkey, const hash_t& hash) const
{
	auto sig_hash = std::hash<typename bls_signature_t::super_t>{}(*this) ^ hash_salt;
	sig_hash = vnx::Hash64(&sig_hash, sizeof(sig_hash));

	auto& entry = sig_cache[sig_hash % sig_cache.size()];
	{
		std::lock_guard lock(mutex);
		if(entry.sig == *this && entry.pubkey == pubkey && entry.hash == hash) {
			return true;
		}
	}
	bls::AugSchemeMPL MPL;
	const bool res = MPL.Verify(pubkey.to_bls(), bls::Bytes(hash.bytes.data(), hash.bytes.size()), to_bls());
	if(res) {
		std::lock_guard lock(mutex);
		entry.sig = *this;
		entry.hash = hash;
		entry.pubkey = pubkey;
	}
	return res;
}

bls::G2Element bls_signature_t::to_bls() const
{
	return bls::G2Element::FromBytes(bls::Bytes(bytes.data(), bytes.size()));
}

bls_signature_t bls_signature_t::sign(const skey_t& skey, const hash_t& hash)
{
	return sign(skey.to_bls(), hash);
}

bls_signature_t bls_signature_t::sign(const bls::PrivateKey& skey, const hash_t& hash)
{
	bls::AugSchemeMPL MPL;
	return MPL.Sign(skey, bls::Bytes(hash.bytes.data(), hash.bytes.size()));
}


} // mmx
