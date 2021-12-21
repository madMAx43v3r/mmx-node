/*
 * Wallet.h
 *
 *  Created on: Dec 11, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_WALLET_H_
#define INCLUDE_MMX_WALLET_H_

#include <mmx/WalletBase.hxx>
#include <mmx/NodeClient.hxx>
#include <mmx/ChainParams.hxx>
#include <mmx/BLS_Wallet.h>
#include <mmx/ECDSA_Wallet.h>
#include <mmx/txio_key_t.hpp>


namespace mmx {

class Wallet : public WalletBase {
public:
	Wallet(const std::string& _vnx_name);

protected:
	void init() override;

	void main() override;

	void open_wallet(const uint32_t& index, const std::string& passwd) override;

	void open_wallet_ex(const uint32_t& index, const uint32_t& num_addresses, const std::string& passwd) override;

	void close_wallet() override;

	hash_t send(const uint64_t& amount, const addr_t& dst_addr, const addr_t& contract) const override;

	std::vector<std::pair<txio_key_t, utxo_t>> get_utxo_list() const override;

	std::vector<std::pair<txio_key_t, utxo_t>> get_utxo_list_for(const addr_t& contract) const override;

	uint64_t get_balance(const addr_t& contract) const override;

	std::string get_address(const uint32_t& index) const override;

	void show_farmer_keys(const uint32_t& index) const override;

	std::shared_ptr<const FarmerKeys> get_farmer_keys(const uint32_t& index) const override;

	std::vector<std::shared_ptr<const FarmerKeys>> get_all_farmer_keys() const override;

private:
	std::shared_ptr<NodeClient> node;
	std::shared_ptr<ECDSA_Wallet> wallet;
	std::vector<std::shared_ptr<BLS_Wallet>> bls_wallets;

	mutable std::unordered_map<txio_key_t, hash_t> spent_utxo_map;

	std::shared_ptr<const ChainParams> params;

};


} // mmx

#endif /* INCLUDE_MMX_WALLET_H_ */
