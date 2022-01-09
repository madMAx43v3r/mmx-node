/*
 * Wallet.cpp
 *
 *  Created on: Dec 11, 2021
 *      Author: mad
 */

#include <mmx/Wallet.h>
#include <mmx/utxo_t.hpp>
#include <mmx/utxo_entry_t.hpp>
#include <mmx/stxo_entry_t.hpp>
#include <mmx/solution/PubKey.hxx>
#include <mmx/utils.h>

#include <vnx/vnx.h>
#include <algorithm>


namespace mmx {

Wallet::Wallet(const std::string& _vnx_name)
	:	WalletBase(_vnx_name)
{
}

void Wallet::init()
{
	vnx::open_pipe(vnx_name, this, 1000);
}

void Wallet::main()
{
	params = get_params();

	http = std::make_shared<vnx::addons::HttpInterface<Wallet>>(this, vnx_name);
	add_async_client(http);

	for(auto file_path : key_files)
	{
		file_path = storage_path + file_path;

		if(auto key_file = vnx::read_from_file<KeyFile>(file_path))
		{
			wallets.push_back(std::make_shared<ECDSA_Wallet>(key_file, params, num_addresses));
			bls_wallets.push_back(std::make_shared<BLS_Wallet>(key_file, 11337));
		}
		else {
			wallets.push_back(nullptr);
			bls_wallets.push_back(nullptr);
			log(ERROR) << "Failed to read wallet '" << file_path << "'";
		}
	}

	node = std::make_shared<NodeClient>(node_server);

	Super::main();
}

std::shared_ptr<ECDSA_Wallet> Wallet::get_wallet(const uint32_t& index) const
{
	if(index >= wallets.size()) {
		throw std::logic_error("invalid wallet index: " + std::to_string(index));
	}
	if(auto wallet = wallets[index]) {
		return wallet;
	}
	throw std::logic_error("no such wallet");
}

hash_t Wallet::send(const uint32_t& index, const uint64_t& amount, const addr_t& dst_addr, const addr_t& contract) const
{
	if(amount == 0) {
		throw std::logic_error("amount cannot be zero");
	}
	if(dst_addr == addr_t()) {
		throw std::logic_error("dst_addr cannot be zero");
	}
	const auto wallet = get_wallet(index);

	// update utxo_cache
	get_utxo_list(index);

	auto tx = wallet->send(amount, dst_addr, contract);

	node->add_transaction(tx);
	wallet->update_from(tx);

	log(INFO) << "Sent " << amount << " with fee " << tx->calc_min_fee(params) << " to " << dst_addr << " (" << tx->id << ")";
	return tx->id;
}

std::vector<utxo_entry_t> Wallet::get_utxo_list(const uint32_t& index) const
{
	const auto wallet = get_wallet(index);
	const auto now = vnx::get_wall_time_millis();

	if(!wallet->last_utxo_update || now - wallet->last_utxo_update > utxo_timeout_ms)
	{
		const auto all_utxo = node->get_utxo_list(wallet->get_all_addresses());
		wallet->update_cache(all_utxo);
		wallet->last_utxo_update = now;

		log(INFO) << "Updated UTXO cache: " << wallet->utxo_cache.size() << " entries, " << wallet->utxo_change_cache.size() << " pending change";
	}
	return wallet->utxo_cache;
}

std::vector<utxo_entry_t> Wallet::get_utxo_list_for(const uint32_t& index, const addr_t& contract) const
{
	std::vector<utxo_entry_t> res;
	for(const auto& entry : get_utxo_list(index)) {
		if(entry.output.contract == contract) {
			res.push_back(entry);
		}
	}
	return res;
}

std::vector<stxo_entry_t> Wallet::get_stxo_list(const uint32_t& index) const
{
	const auto wallet = get_wallet(index);
	return node->get_stxo_list(wallet->get_all_addresses());
}

std::vector<stxo_entry_t> Wallet::get_stxo_list_for(const uint32_t& index, const addr_t& contract) const
{
	std::vector<stxo_entry_t> res;
	for(const auto& entry : get_stxo_list(index)) {
		if(entry.output.contract == contract) {
			res.push_back(entry);
		}
	}
	return res;
}

std::vector<tx_entry_t> Wallet::get_history(const uint32_t& index, const int32_t& since) const
{
	const auto wallet = get_wallet(index);
	return node->get_history_for(wallet->get_all_addresses(), since);
}

uint64_t Wallet::get_balance(const uint32_t& index, const addr_t& contract) const
{
	uint64_t total = 0;
	for(const auto& entry : get_utxo_list(index)) {
		if(entry.output.contract == contract) {
			total += entry.output.amount;
		}
	}
	return total;
}

addr_t Wallet::get_address(const uint32_t& index, const uint32_t& offset) const
{
	const auto wallet = get_wallet(index);
	return wallet->get_address(offset);
}

std::vector<addr_t> Wallet::get_all_addresses(const int32_t& index) const
{
	if(index >= 0) {
		return get_wallet(index)->get_all_addresses();
	}
	std::vector<addr_t> list;
	for(const auto wallet : wallets) {
		if(wallet) {
			const auto set = wallet->get_all_addresses();
			list.insert(list.end(), set.begin(), set.end());
		}
	}
	return list;
}

std::shared_ptr<const FarmerKeys> Wallet::get_farmer_keys(const uint32_t& index) const
{
	if(auto wallet = bls_wallets.at(index)) {
		return wallet->get_farmer_keys();
	}
	return nullptr;
}

std::vector<std::shared_ptr<const FarmerKeys>> Wallet::get_all_farmer_keys() const
{
	std::vector<std::shared_ptr<const FarmerKeys>> res;
	for(auto wallet : bls_wallets) {
		if(wallet) {
			res.push_back(wallet->get_farmer_keys());
		}
	}
	return res;
}

hash_t Wallet::get_master_seed(const uint32_t& index) const
{
	if(index >= key_files.size()) {
		throw std::logic_error("invalid wallet index");
	}
	if(auto key_file = vnx::read_from_file<KeyFile>(storage_path + key_files[index])) {
		return key_file->seed_value;
	}
	throw std::logic_error("failed to read key file");
}

void Wallet::http_request_async(std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
								const vnx::request_id_t& request_id) const
{
	http->http_request(request, sub_path, request_id);
}

void Wallet::http_request_chunk_async(	std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
										const int64_t& offset, const int64_t& max_bytes, const vnx::request_id_t& request_id) const
{
	throw std::logic_error("not implemented");
}


} // mmx
