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

#include <vnx/addons/HttpInterface.h>


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

	std::vector<utxo_entry_t> get_utxo_list() const override;

	std::vector<utxo_entry_t> get_utxo_list_for(const addr_t& contract) const override;

	std::vector<stxo_entry_t> get_stxo_list() const override;

	std::vector<stxo_entry_t> get_stxo_list_for(const addr_t& contract) const override;

	std::vector<tx_entry_t> get_history(const uint32_t& min_height) const override;

	uint64_t get_balance(const addr_t& contract) const override;

	addr_t get_address(const uint32_t& index) const override;

	void show_farmer_keys(const uint32_t& index) const override;

	std::shared_ptr<const FarmerKeys> get_farmer_keys(const uint32_t& index) const override;

	std::vector<std::shared_ptr<const FarmerKeys>> get_all_farmer_keys() const override;

	hash_t get_master_seed(const uint32_t& index) const override;

	void http_request_async(std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
							const vnx::request_id_t& request_id) const override;

	void http_request_chunk_async(	std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
									const int64_t& offset, const int64_t& max_bytes, const vnx::request_id_t& request_id) const override;

private:
	std::shared_ptr<NodeClient> node;

	ssize_t wallet_index = -1;
	std::shared_ptr<ECDSA_Wallet> wallet;
	std::vector<std::shared_ptr<BLS_Wallet>> bls_wallets;

	mutable int64_t last_utxo_update = 0;
	mutable std::vector<utxo_entry_t> utxo_cache;
	mutable std::unordered_map<txio_key_t, addr_t> addr_map;
	mutable std::unordered_set<txio_key_t> spent_txo_set;
	mutable std::unordered_map<txio_key_t, tx_out_t> utxo_change_cache;

	std::shared_ptr<const ChainParams> params;
	std::shared_ptr<vnx::addons::HttpInterface<Wallet>> http;

	friend class vnx::addons::HttpInterface<Wallet>;

};


} // mmx

#endif /* INCLUDE_MMX_WALLET_H_ */
