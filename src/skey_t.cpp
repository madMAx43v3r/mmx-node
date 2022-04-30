/*
 * skey_t.cpp
 *
 *  Created on: Apr 29, 2022
 *      Author: mad
 */

#include <mmx/skey_t.hpp>

#include <bls.hpp>


namespace mmx {

skey_t::skey_t(const bls::PrivateKey& key)
{
	if(bls::PrivateKey::PRIVATE_KEY_SIZE != size()) {
		throw std::logic_error("key size mismatch");
	}
	key.Serialize(data());
}


} // mmx
