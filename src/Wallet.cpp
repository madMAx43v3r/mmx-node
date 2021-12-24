/*
 * Wallet.cpp
 *
 *  Created on: Dec 11, 2021
 *      Author: mad
 */

#include <mmx/Wallet.h>
#include <mmx/utxo_t.hpp>
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

	for(const auto& file_path : key_files)
	{
		if(auto key_file = vnx::read_from_file<KeyFile>(file_path))
		{
			bls_wallets.push_back(std::make_shared<BLS_Wallet>(key_file, 11337));
		}
		else {
			bls_wallets.push_back(nullptr);
			log(ERROR) << "Failed to read wallet '" << file_path << "'";
		}
	}

	open_wallet(default_wallet, "");

	node = std::make_shared<NodeClient>(node_server);

	Super::main();
}

void Wallet::open_wallet(const uint32_t& index, const std::string& passwd)
{
	if(wallet_index == index) {
		return;
	}
	open_wallet_ex(index, num_addresses, passwd);
}

void Wallet::open_wallet_ex(const uint32_t& index, const uint32_t& num_addresses, const std::string& passwd)
{
	if(index >= key_files.size()) {
		throw std::logic_error("invalid wallet index");
	}
	close_wallet();

	if(auto key_file = vnx::read_from_file<KeyFile>(key_files[index]))
	{
		wallet = std::make_shared<ECDSA_Wallet>(key_file, num_addresses);
		wallet_index = index;
		log(INFO) << "Loaded wallet '" << key_files[index] << "' with " << num_addresses << " addresses";
	}
	else {
		throw std::runtime_error("failed to open wallet file");
	}
}

void Wallet::close_wallet()
{
	wallet = nullptr;
}

static
uint64_t gather_inputs(	std::shared_ptr<Transaction> tx,
						std::unordered_set<txio_key_t>& spent_txo,
						const std::vector<std::pair<txio_key_t, utxo_t>>& utxo_list,
						const uint64_t amount, const addr_t& contract)
{
	uint64_t output = amount;
	uint64_t change = 0;

	for(const auto& entry : utxo_list)
	{
		if(output == 0) {
			break;
		}
		const auto& out = entry.second;
		if(out.contract != contract) {
			continue;
		}
		const auto& key = entry.first;
		if(spent_txo.count(key)) {
			continue;
		}
		if(out.amount > output) {
			change += out.amount - output;
			output = 0;
		} else {
			output -= out.amount;
		}
		tx_in_t in;
		in.prev = key;
		tx->inputs.push_back(in);
		spent_txo.insert(key);
	}
	if(output != 0) {
		throw std::logic_error("not enough funds");
	}
	return change;
}

hash_t Wallet::send(const uint64_t& amount, const addr_t& dst_addr, const addr_t& contract) const
{
	if(!wallet) {
		throw std::logic_error("have no wallet");
	}
	if(amount == 0) {
		throw std::logic_error("amount cannot be zero");
	}

	// get a list of all utxo we could use
	auto utxo_list = get_utxo_list();

	// create lookup map
	std::unordered_map<txio_key_t, addr_t> addr_map;
	for(const auto& entry : utxo_list) {
		addr_map[entry.first] = entry.second.address;
	}

	// sort by height, such as to use oldest coins first (which are more likely to be spend-able right now)
	std::sort(utxo_list.begin(), utxo_list.end(),
		[](const std::pair<txio_key_t, utxo_t>& lhs, const std::pair<txio_key_t, utxo_t>& rhs) -> bool {
			return lhs.second.height < rhs.second.height;
		});

	auto spent_txo = spent_txo_set;

	auto tx = Transaction::create();
	{
		// add primary output
		tx_out_t out;
		out.address = dst_addr;
		out.contract = contract;
		out.amount = amount;
		tx->outputs.push_back(out);
	}
	uint64_t change = gather_inputs(tx, spent_txo, utxo_list, amount, contract);

	if(contract != addr_t() && change > 0) {
		// token change cannot be used as tx fee
		tx_out_t out;
		out.address = wallet->get_address(0);
		out.contract = contract;
		out.amount = change;
		tx->outputs.push_back(out);
		change = 0;
	}

	// gather inputs for tx fee
	uint64_t tx_fees = 0;
	while(true) {
		// count number of solutions needed
		std::unordered_set<addr_t> used_addr;
		for(const auto& in : tx->inputs) {
			auto iter = addr_map.find(in.prev);
			if(iter == addr_map.end()) {
				throw std::logic_error("cannot sign input");
			}
			used_addr.insert(iter->second);
		}
		tx_fees = tx->calc_min_fee(params)
				+ params->min_txfee_io	// for change output
				+ used_addr.size() * params->min_txfee_sign;

		if(change > tx_fees) {
			// we got more than enough, add change output
			tx_out_t out;
			out.address = wallet->get_address(0);
			out.amount = change - tx_fees;
			tx->outputs.push_back(out);
			change = 0;
			break;
		}
		if(change == tx_fees) {
			// perfect match
			change = 0;
			break;
		}
		// gather more
		const auto left = tx_fees - change;
		change += gather_inputs(tx, spent_txo, utxo_list, left, addr_t());
		change += left;
	}
	tx->finalize();

	std::unordered_map<addr_t, uint32_t> solution_map;

	for(auto& in : tx->inputs)
	{
		// sign all inputs
		auto iter = addr_map.find(in.prev);
		if(iter == addr_map.end()) {
			throw std::logic_error("cannot sign input");
		}
		auto iter2 = solution_map.find(iter->second);
		if(iter2 != solution_map.end()) {
			// re-use solution
			in.solution = iter2->second;
			continue;
		}
		const auto& keys = wallet->get_keypair(iter->second);

		auto sol = solution::PubKey::create();
		sol->pubkey = keys.second;
		sol->signature = signature_t::sign(keys.first, tx->id);

		in.solution = tx->solutions.size();
		solution_map[iter->second] = in.solution;
		tx->solutions.push_back(sol);
	}
	{
		std::stringstream ss;
		vnx::PrettyPrinter printer(ss);
		tx->accept(printer);
		log(INFO) << "Sent: " << ss.str();
	}
	node->add_transaction(tx);

	log(INFO) << "Sent " << amount << " with fee " << tx_fees << " / " << tx->calc_min_fee(params)
			<< " to " << dst_addr << " (" << tx->id << ")";

	// store used utxos
	spent_txo_set = spent_txo;

	// remove spent change
	for(const auto& in : tx->inputs) {
		change_utxo_map.erase(in.prev);
	}
	// store change outputs
	for(size_t i = 0; i < tx->outputs.size(); ++i) {
		const auto& out = tx->outputs[i];
		if(wallet->find_address(out.address) >= 0) {
			change_utxo_map[txio_key_t::create_ex(tx->id, i)] = out;
		}
	}
	return tx->id;
}

std::vector<std::pair<txio_key_t, utxo_t>> Wallet::get_utxo_list() const
{
	const auto all_utxo = node->get_utxo_list(wallet->get_all_addresses());

	// remove any utxo we have already consumed
	std::vector<std::pair<txio_key_t, utxo_t>> list;
	for(const auto& entry : all_utxo) {
		if(!spent_txo_set.count(entry.first)) {
			list.push_back(entry);
		}
		// remove confirmed change
		change_utxo_map.erase(entry.first);
	}
	// add pending change outputs
	for(const auto& entry : change_utxo_map) {
		list.emplace_back(entry.first, utxo_t::create_ex(entry.second, -1));
	}
	return list;
}

std::vector<std::pair<txio_key_t, utxo_t>> Wallet::get_utxo_list_for(const addr_t& contract) const
{
	std::vector<std::pair<txio_key_t, utxo_t>> res;
	for(const auto& entry : get_utxo_list()) {
		if(entry.second.contract == contract) {
			res.push_back(entry);
		}
	}
	return res;
}

uint64_t Wallet::get_balance(const addr_t& contract) const
{
	uint64_t total = 0;
	for(const auto& entry : get_utxo_list()) {
		if(entry.second.contract == contract) {
			total += entry.second.amount;
		}
	}
	return total;
}

std::string Wallet::get_address(const uint32_t& index) const
{
	if(!wallet) {
		throw std::logic_error("have no wallet");
	}
	return wallet->get_address(index).to_string();
}

void Wallet::show_farmer_keys(const uint32_t& index) const
{
	if(auto wallet = bls_wallets.at(index)) {
		if(auto keys = wallet->get_farmer_keys()) {
			std::stringstream ss;
			vnx::PrettyPrinter printer(ss);
			keys->accept(printer);
			log(INFO) << ss.str();
		}
	} else {
		log(ERROR) << "No such wallet";
	}
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


} // mmx
