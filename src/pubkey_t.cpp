/*
 * pubkey_t.cpp
 *
 *  Created on: Mar 30, 2023
 *      Author: mad
 */

#include <mmx/pubkey_t.hpp>
#include <mmx/skey_t.hpp>


namespace mmx {

pubkey_t::pubkey_t(const skey_t& key)
{
	secp256k1_pubkey pubkey;
	if(!secp256k1_ec_pubkey_create(g_secp256k1, &pubkey, key.data())) {
		throw std::logic_error("secp256k1_ec_pubkey_create() failed");
	}
	*this = pubkey_t(pubkey);
}

pubkey_t::pubkey_t(const secp256k1_pubkey& key)
{
	size_t len = size();
	secp256k1_ec_pubkey_serialize(g_secp256k1, data(), &len, &key, SECP256K1_EC_COMPRESSED);
	if(len != 33) {
		throw std::logic_error("secp256k1_ec_pubkey_serialize(): length != 33");
	}
}

secp256k1_pubkey pubkey_t::to_secp256k1() const
{
	secp256k1_pubkey res;
	if(!secp256k1_ec_pubkey_parse(g_secp256k1, &res, data(), size())) {
		throw std::logic_error("secp256k1_ec_pubkey_parse() failed");
	}
	return res;
}


} // mmx
