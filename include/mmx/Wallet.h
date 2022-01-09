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

	hash_t send(const uint32_t& index, const uint64_t& amount, const addr_t& dst_addr, const addr_t& contract) const override;

	std::vector<utxo_entry_t> get_utxo_list(const uint32_t& index) const override;

	std::vector<utxo_entry_t> get_utxo_list_for(const uint32_t& index, const addr_t& contract) const override;

	std::vector<stxo_entry_t> get_stxo_list(const uint32_t& index) const override;

	std::vector<stxo_entry_t> get_stxo_list_for(const uint32_t& index, const addr_t& contract) const override;

	std::vector<tx_entry_t> get_history(const uint32_t& index, const int32_t& since) const override;

	uint64_t get_balance(const uint32_t& index, const addr_t& contract) const override;

	addr_t get_address(const uint32_t& index, const uint32_t& offset) const override;

	std::vector<addr_t> get_all_addresses(const int32_t& index) const override;

	std::shared_ptr<const FarmerKeys> get_farmer_keys(const uint32_t& index) const override;

	std::vector<std::shared_ptr<const FarmerKeys>> get_all_farmer_keys() const override;

	hash_t get_master_seed(const uint32_t& index) const override;

	void http_request_async(std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
							const vnx::request_id_t& request_id) const override;

	void http_request_chunk_async(	std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
									const int64_t& offset, const int64_t& max_bytes, const vnx::request_id_t& request_id) const override;

private:
	std::shared_ptr<ECDSA_Wallet> get_wallet(const uint32_t& index) const;

private:
	std::shared_ptr<NodeClient> node;

	std::vector<std::shared_ptr<ECDSA_Wallet>> wallets;
	std::vector<std::shared_ptr<BLS_Wallet>> bls_wallets;

	std::shared_ptr<const ChainParams> params;
	std::shared_ptr<vnx::addons::HttpInterface<Wallet>> http;

	friend class vnx::addons::HttpInterface<Wallet>;

};


} // mmx

#endif /* INCLUDE_MMX_WALLET_H_ */
