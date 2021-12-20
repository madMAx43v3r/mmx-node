/*
 * Wallet.cpp
 *
 *  Created on: Dec 11, 2021
 *      Author: mad
 */

#include <mmx/Wallet.h>
#include <mmx/solution/PubKey.hxx>
#include <mmx/utils.h>

#include <vnx/vnx.h>


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
						std::unordered_map<txio_key_t, utxo_t>& utxo_map,
						const uint64_t amount, const addr_t& contract)
{
	uint64_t output = amount;
	uint64_t change = 0;

	for(auto iter = utxo_map.begin(); iter != utxo_map.end();)
	{
		if(output == 0) {
			break;
		}
		const auto& out = iter->second;
		if(out.contract != contract) {
			iter++;
			continue;
		}
		if(out.amount > output) {
			change += out.amount - output;
			output = 0;
		} else {
			output -= out.amount;
		}
		tx_in_t in;
		in.prev = iter->first;
		tx->inputs.push_back(in);
		iter = utxo_map.erase(iter);
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
	const auto all_utxos = node->get_utxo_list(wallet->get_all_addresses());

	std::unordered_map<txio_key_t, addr_t> addr_map;
	std::unordered_map<txio_key_t, utxo_t> utxo_map;

	// remove any utxo we have already consumed (but are possibly not finalized yet)
	for(const auto& entry : all_utxos) {
		if(!spent_utxo_map.count(entry.first)) {
			utxo_map.insert(entry);
			addr_map[entry.first] = entry.second.address;
		}
	}

	auto tx = Transaction::create();
	{
		// add primary output
		tx_out_t out;
		out.address = dst_addr;
		out.contract = contract;
		out.amount = amount;
		tx->outputs.push_back(out);
	}
	uint64_t change = gather_inputs(tx, utxo_map, amount, contract);

	if(contract != addr_t() && change > 0)
	{
		// token change cannot be used as tx fee
		tx_out_t out;
		out.address = wallet->get_address(0);
		out.contract = contract;
		out.amount = change;
		tx->outputs.push_back(out);
		change = 0;
	}
	uint64_t tx_fees = tx->calc_min_fee(params);

	while(change < tx_fees + params->min_txfee_io)
	{
		// gather inputs for tx fee
		change += gather_inputs(tx, utxo_map, (tx_fees + params->min_txfee_io) - change, addr_t());
		tx_fees = tx->calc_min_fee(params);
	}
	if(change > tx_fees + params->min_txfee_io)
	{
		// add change output
		tx_out_t out;
		out.address = wallet->get_address(0);
		out.amount = change - (tx_fees + params->min_txfee_io);
		tx->outputs.push_back(out);
		change = 0;
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

	// store used utxos
	for(const auto& in : tx->inputs) {
		spent_utxo_map[in.prev] = tx->id;
	}
	return tx->id;
}

uint64_t Wallet::get_balance(const addr_t& contract) const
{
	return node->get_total_balance(wallet->get_all_addresses(), contract);
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
