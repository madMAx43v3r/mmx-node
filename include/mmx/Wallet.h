/*
 * Wallet.h
 *
 *  Created on: Dec 11, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_WALLET_H_
#define INCLUDE_MMX_WALLET_H_

#include <mmx/WalletBase.hxx>
#include <mmx/BLS_Wallet.h>
#include <mmx/ECDSA_Wallet.h>


namespace mmx {

class Wallet : public WalletBase {
public:
	Wallet(const std::string& _vnx_name);

protected:
	void init() override;

	void main() override;

	std::string get_address(const uint32_t& index, const uint32_t& offset) const override;

	void show_farmer_keys(const uint32_t& index) const override;

	std::shared_ptr<const FarmerKeys> get_farmer_keys(const uint32_t& index) const override;

	std::vector<std::shared_ptr<const FarmerKeys>> get_all_farmer_keys() const override;

private:
	std::vector<std::shared_ptr<BLS_Wallet>> bls_wallets;
	std::vector<std::shared_ptr<ECDSA_Wallet>> ecdsa_wallets;

};


} // mmx

#endif /* INCLUDE_MMX_WALLET_H_ */
