/*
 * bls_pubkey_t.cpp
 *
 *  Created on: May 26, 2023
 *      Author: mad
 */

#include <mmx/bls_pubkey_t.hpp>

#include <bls.hpp>


namespace mmx {

bls_pubkey_t::bls_pubkey_t(const bls::G1Element& key)
{
	const auto tmp = key.Serialize();
	if(tmp.size() != bytes.size()) {
		throw std::logic_error("key size mismatch");
	}
	::memcpy(bytes.data(), tmp.data(), tmp.size());
}

void bls_pubkey_t::to(bls::G1Element& key) const
{
	key = bls::G1Element::FromBytes(bls::Bytes(bytes.data(), bytes.size()));
}

hash_t bls_pubkey_t::get_addr() const
{
	return hash_t(bytes);
}


} // mmx
