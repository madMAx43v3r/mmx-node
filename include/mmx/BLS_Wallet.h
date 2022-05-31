/*
 * BLS_Wallet.h
 *
 *  Created on: Dec 11, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_BLS_WALLET_H_
#define INCLUDE_MMX_BLS_WALLET_H_

#include <mmx/KeyFile.hxx>
#include <mmx/FarmerKeys.hxx>


namespace mmx {

class BLS_Wallet {
public:
	BLS_Wallet(std::shared_ptr<const KeyFile> key_file, const int port)
	{
		if(key_file->seed_value == hash_t()) {
			throw std::logic_error("seed == zero");
		}
		bls::AugSchemeMPL MPL;
		const bls::PrivateKey master_sk = MPL.KeyGen(key_file->seed_value.to_vector());
		this->master_sk = std::make_shared<bls::PrivateKey>(master_sk);

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

	std::shared_ptr<const FarmerKeys> get_farmer_keys() const {
		return farmer_keys;
	}

private:
	std::shared_ptr<bls::PrivateKey> master_sk;

	std::shared_ptr<FarmerKeys> farmer_keys;

};


} // mmx

#endif /* INCLUDE_MMX_BLS_WALLET_H_ */
