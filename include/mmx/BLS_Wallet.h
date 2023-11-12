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
	BLS_Wallet(const hash_t& seed_value, const int port);

	std::shared_ptr<const FarmerKeys> get_farmer_keys() const {
		return farmer_keys;
	}

private:
	std::shared_ptr<FarmerKeys> farmer_keys;

};


} // mmx

#endif /* INCLUDE_MMX_BLS_WALLET_H_ */
