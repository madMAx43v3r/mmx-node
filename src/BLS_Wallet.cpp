/*
 * BLS_Wallet.cpp
 *
 *  Created on: May 26, 2023
 *      Author: mad
 */

#include <mmx/BLS_Wallet.h>

#include <bls.hpp>


namespace mmx {

BLS_Wallet::BLS_Wallet(const hash_t& seed_value, const int port)
{
	if(seed_value == hash_t()) {
		throw std::logic_error("seed == zero");
	}
	bls::AugSchemeMPL MPL;
	const bls::PrivateKey master_sk = MPL.KeyGen(seed_value.to_vector());

	bls::PrivateKey pool_sk = master_sk;
	bls::PrivateKey farmer_sk = master_sk;

	for(int i : {12381, port, 1, 0}) {
		pool_sk = MPL.DeriveChildSk(pool_sk, i);
	}
	for(int i : {12381, port, 0, 0}) {
		farmer_sk = MPL.DeriveChildSk(farmer_sk, i);
	}

	farmer_keys = FarmerKeys::create();
	farmer_keys->pool_private_key = pool_sk;
	farmer_keys->farmer_private_key = farmer_sk;
	farmer_keys->pool_public_key = pool_sk.GetG1Element();
	farmer_keys->farmer_public_key = farmer_sk.GetG1Element();
}


} // mmx
