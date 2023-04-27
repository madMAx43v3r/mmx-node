/*
 * mmx.cpp
 *
 *  Created on: Dec 21, 2021
 *      Author: mad
 */

#include <mmx/NodeClient.hxx>
#include <mmx/RouterClient.hxx>
#include <mmx/WalletClient.hxx>
#include <mmx/FarmerClient.hxx>
#include <mmx/HarvesterClient.hxx>
#include <mmx/Contract.hxx>
#include <mmx/contract/NFT.hxx>
#include <mmx/contract/TokenBase.hxx>
#include <mmx/contract/Executable.hxx>
#include <mmx/contract/VirtualPlot.hxx>
#include <mmx/KeyFile.hxx>
#include <mmx/secp256k1.hpp>
#include <mmx/hash_t.hpp>
#include <mmx/fixed128.hpp>
#include <mmx/mnemonic.h>
#include <mmx/utils.h>
#include <mmx/vm/instr_t.h>
#include <mmx/vm_interface.h>

#include <vnx/vnx.h>
#include <vnx/Proxy.h>
#include <vnx/ProxyClient.hxx>

#include <cmath>
#include <filesystem>


std::unordered_map<mmx::addr_t, std::shared_ptr<const mmx::Contract>> contract_cache;

std::shared_ptr<const mmx::Contract> get_contract(mmx::NodeClient& node, const mmx::addr_t& address)
{
	auto iter = contract_cache.find(address);
	if(iter == contract_cache.end()) {
		iter = contract_cache.emplace(address, node.get_contract(address)).first;
	}
	return iter->second;
}

std::shared_ptr<const mmx::contract::TokenBase> get_token(mmx::NodeClient& node, const mmx::addr_t& address, bool fail = true)
{
	auto token = std::dynamic_pointer_cast<const mmx::contract::TokenBase>(get_contract(node, address));
	if(!token && fail) {
		throw std::logic_error("no such token: " + address.to_string());
	}
	return token;
}

void show_history(const std::vector<mmx::tx_entry_t>& history, mmx::NodeClient& node, std::shared_ptr<const mmx::ChainParams> params)
{
	for(const auto& entry : history) {
		std::cout << "[" << entry.height << "] ";
		std::string arrow = "->";
		switch(entry.type) {
			case mmx::tx_type_e::SPEND:   std::cout << "SPEND   - "; arrow = "<-"; break;
			case mmx::tx_type_e::TXFEE:   std::cout << "TXFEE   - "; arrow = "<-"; break;
			case mmx::tx_type_e::RECEIVE: std::cout << "RECEIVE + "; break;
			case mmx::tx_type_e::REWARD:  std::cout << "REWARD  + "; break;
			case mmx::tx_type_e::VDF_REWARD:  std::cout << "VDF_REWARD + "; break;
			case mmx::tx_type_e::PROJECT_REWARD:  std::cout << "PROJECT_REWARD + "; break;
			default: std::cout << "????    "; break;
		}
		const auto contract = get_contract(node, entry.contract);
		if(auto nft = std::dynamic_pointer_cast<const mmx::contract::NFT>(contract)) {
			std::cout << entry.contract << " " << arrow << " " << entry.address << " (" << entry.txid << ")" << std::endl;
		} else {
			const auto token = std::dynamic_pointer_cast<const mmx::contract::TokenBase>(contract);
			const auto decimals = token ? token->decimals : params->decimals;
			std::cout << to_value_128(entry.amount, decimals) << " " << (token ? token->symbol : "MMX")
					<< " (" << entry.amount << ") " << arrow << " " << entry.address << " (" << entry.txid << ")" << std::endl;
		}
	}
}

bool accept_prompt()
{
	std::cout << "Accept (y): ";
	std::string input;
	std::getline(std::cin, input);
	if(input == "y") {
		return true;
	}
	std::cout << "Aborted" << std::endl;
	return false;
}


int main(int argc, char** argv)
{
	mmx::secp256k1_init();

	std::string mmx_home;
	if(auto path = ::getenv("MMX_HOME")) {
		mmx_home = path;
	}

	std::map<std::string, std::string> options;
	options["n"] = "node";
	options["f"] = "file";
	options["j"] = "index";
	options["k"] = "offset";
	options["a"] = "amount";
	options["b"] = "ask-amount";
	options["m"] = "count";
	options["s"] = "source";
	options["t"] = "target";
	options["x"] = "contract";
	options["z"] = "ask-currency";
	options["y"] = "yes";
	options["r"] = "fee-ratio";
	options["l"] = "gas-limit";
	options["node"] = "address";
	options["file"] = "path";
	options["index"] = "0..?";
	options["offset"] = "0..?";
	options["amount"] = "123.456";
	options["source"] = "address";
	options["target"] = "address";
	options["contract"] = "address";
	options["ask-currency"] = "address";
	options["fee-ratio"] = "1";
	options["gas-limit"] = "MMX";
	options["min-amount"] = "0..1";
	options["user"] = "mmx-admin";
	options["passwd"] = "PASSWD";
	options["mnemonic"] = "words";

	vnx::write_config("log_level", 2);

	vnx::init("mmx", argc, argv, options);

	std::string module;
	std::string command;
	std::string file_name;
	std::string source_addr;
	std::string target_addr;
	std::string contract_addr;
	std::string ask_currency_addr;
	std::string user = "mmx-admin";
	std::string passwd;
	int64_t index = 0;
	int64_t offset = 0;
	int64_t num_count = 0;
	mmx::fixed128 amount;
	mmx::fixed128 ask_amount;
	double fee_ratio = 1;
	double gas_limit = 1;
	double min_amount = 0.95;
	bool pre_accept = false;
	vnx::read_config("$1", module);
	vnx::read_config("$2", command);
	vnx::read_config("yes", pre_accept);
	vnx::read_config("file", file_name);
	vnx::read_config("index", index);
	vnx::read_config("offset", offset);
	const auto have_amount = vnx::read_config("amount", amount);
	const auto have_ask_amount = vnx::read_config("ask-amount", ask_amount);
	vnx::read_config("count", num_count);
	vnx::read_config("source", source_addr);
	vnx::read_config("target", target_addr);
	vnx::read_config("contract", contract_addr);
	vnx::read_config("ask-currency", ask_currency_addr);
	vnx::read_config("fee-ratio", fee_ratio);
	vnx::read_config("gas-limit", gas_limit);
	vnx::read_config("min-amount", min_amount);
	vnx::read_config("user", user);
	vnx::read_config("passwd", passwd);

	bool did_fail = false;
	auto params = mmx::get_params();
	{
		auto token = mmx::contract::TokenBase::create();
		token->decimals = params->decimals;
		token->symbol = "MMX";
		contract_cache[mmx::addr_t()] = token;
	}
	if(passwd.empty()) {
		std::ifstream stream(mmx_home + "PASSWD");
		stream >> passwd;
	}

	mmx::spend_options_t spend_options;
	spend_options.fee_ratio = fee_ratio * 1024;
	spend_options.max_extra_cost = mmx::to_amount(gas_limit / fee_ratio, params);

	mmx::NodeClient node("Node");

	mmx::addr_t source;
	mmx::addr_t target;
	mmx::addr_t contract;
	mmx::addr_t ask_currency;
	try {
		if(!source_addr.empty()) {
			source.from_string(source_addr);
		}
	} catch(std::exception& ex) {
		vnx::log_error() << "Invalid address: " << ex.what();
		goto failed;
	}
	try {
		if(!target_addr.empty()) {
			target.from_string(target_addr);
		}
	} catch(std::exception& ex) {
		vnx::log_error() << "Invalid address: " << ex.what();
		goto failed;
	}
	try {
		if(!contract_addr.empty()) {
			contract.from_string(contract_addr);
		}
	} catch(std::exception& ex) {
		vnx::log_error() << "Invalid address: " << ex.what();
		goto failed;
	}
	try {
		if(!ask_currency_addr.empty()) {
			ask_currency.from_string(ask_currency_addr);
		}
	} catch(std::exception& ex) {
		vnx::log_error() << "Invalid address: " << ex.what();
		goto failed;
	}

	try {
		if(module == "wallet")
		{
			std::string node_url = ":11335";
			vnx::read_config("node", node_url);

			mmx::WalletClient wallet("Wallet");

			if(command != "create") {
				vnx::Handle<vnx::Proxy> proxy = new vnx::Proxy("Proxy", vnx::Endpoint::from_url(node_url));
				proxy->forward_list = {"Wallet", "Node"};
				proxy.start_detached();
				try {
					params = node.get_params();
				} catch(...) {
					// ignore
				}
				const auto accounts = wallet.get_all_accounts();
				if(index == 0 && !accounts.empty() && accounts.find(index) == accounts.end()) {
					index = accounts.begin()->first;
				}
			}

			if(command == "show")
			{
				std::string subject;
				vnx::read_config("$3", subject);

				const auto config = wallet.get_account(index);
				if(config.with_passphrase) {
					std::cout << "Locked: " << (wallet.is_locked(index) ? "Yes" : "No") << std::endl;
				}
				if(subject.empty() || subject == "balance")
				{
					bool is_empty = true;
					std::vector<mmx::addr_t> nfts;
					for(const auto& entry : wallet.get_balances(index))
					{
						const auto& balance = entry.second;
						const auto contract = get_contract(node, entry.first);
						if(auto token = std::dynamic_pointer_cast<const mmx::contract::TokenBase>(contract)) {
							std::cout << "Balance: " << to_value_128(balance.total, token->decimals)
									<< " " << token->symbol << (balance.is_validated ? "" : "?")
									<< " (Spendable: " << to_value_128(balance.spendable, token->decimals)
									<< " " << token->symbol << (balance.is_validated ? "" : "?") << ")";
							if(entry.first != mmx::addr_t()) {
								std::cout << " [" << entry.first << "]";
							}
							std::cout << std::endl;
							is_empty = false;
						}
						else if(std::dynamic_pointer_cast<const mmx::contract::NFT>(contract)) {
							nfts.push_back(entry.first);
						}
					}
					if(is_empty) {
						std::cout << "Balance: 0 MMX" << std::endl;
					}
					for(const auto& addr : nfts) {
						std::cout << "NFT: " << addr << std::endl;
					}
				}
				if(subject.empty() || subject == "contracts" || subject == "offers")
				{
					for(const auto& entry : wallet.get_contracts(index))
					{
						const auto& address = entry.first;
						const auto& contract = entry.second;
						vnx::optional<mmx::offer_data_t> offer;
						if(auto exec = std::dynamic_pointer_cast<const mmx::contract::Executable>(contract)) {
							if(exec->binary == params->offer_binary) {
								offer = node.get_offer(address);
								if(subject.empty() && offer && !offer->is_open()) {
									continue;
								}
							} else if(subject == "offers") {
								continue;
							}
						} else if(subject == "offers") {
							continue;
						}
						std::cout << "Contract: " << address << " (" << contract->get_type_name();

						if(auto token = std::dynamic_pointer_cast<const mmx::contract::TokenBase>(contract)) {
							if(!token->symbol.empty()) {
								std::cout << ", " << token->symbol;
							}
							if(!token->name.empty()) {
								std::cout << ", " << token->name;
							}
						}
						if(auto exec = std::dynamic_pointer_cast<const mmx::contract::Executable>(contract)) {
							if(offer) {
								if(offer->is_open()) {
									std::cout << ", open offer";
								} else {
									std::cout << ", closed offer";
								}
							}
						}
						if(auto plot = std::dynamic_pointer_cast<const mmx::contract::VirtualPlot>(contract)) {
							const auto balance = node.get_virtual_plot_balance(entry.first);
							std::cout << ", " << balance / pow(10, params->decimals) << " MMX";
							std::cout << ", " << mmx::calc_virtual_plot_size(params, balance) / pow(1000, 4) << " TB";
						}
						std::cout << ")" << std::endl;

						for(const auto& entry : wallet.get_total_balances({address}))
						{
							const auto& balance = entry.second;
							if(auto token = get_token(node, entry.first, false)) {
								std::cout << "  Balance: " << to_value_128(balance.total, token->decimals) << " " << token->symbol
										<< (balance.is_validated ? "" : "?") << " (" << balance.total << ")";
								if(entry.first != mmx::addr_t()) {
									std::cout << " [" << entry.first << "]";
								}
								std::cout << std::endl;
							}
						}
						if(offer && (offer->is_open() || subject == "offers")) {
							if(auto bid_token = get_token(node, offer->bid_currency, false)) {
								if(auto ask_token = get_token(node, offer->ask_currency, false)) {
									std::cout << "  Price: " << offer->price
										<< " " << ask_token->symbol << " / " << bid_token->symbol << std::endl;
								}
							}
						}
					}
				}
				for(int i = 0; i < 1; ++i) {
					std::cout << "Address[" << i << "]: " << wallet.get_address(index, i) << std::endl;
				}
				if(subject.empty()) {
					std::cout << "Help: mmx wallet show [balance | contracts | offers]" << std::endl;
				}
			}
			else if(command == "accounts")
			{
				for(const auto& entry : wallet.get_all_accounts()) {
					const auto& config = entry.second;
					std::cout << "[" << entry.first << "] name = '" << config.name << "', index = " << config.index
							<< ", num_addresses = " << config.num_addresses << ", key_file = '" << config.key_file << "'" << std::endl;
				}
			}
			else if(command == "keys")
			{
				if(auto keys = wallet.get_farmer_keys(index)) {
					std::cout << "Farmer Public Key: " << keys->farmer_public_key << std::endl;
					std::cout << "Pool Public Key:   " << keys->pool_public_key << std::endl;
				} else {
					vnx::log_error() << "Got no wallet!";
				}
			}
			else if(command == "get")
			{
				std::string subject;
				vnx::read_config("$3", subject);

				if(subject == "address")
				{
					int64_t offset = 0;
					vnx::read_config("$4", offset);

					std::cout << wallet.get_address(index, offset) << std::endl;
				}
				else if(subject == "amount")
				{
					std::cout << wallet.get_balance(index, contract).total << std::endl;
				}
				else if(subject == "balance")
				{
					const auto token = get_token(node, contract);
					std::cout << to_value_128(wallet.get_balance(index, contract).total, token ? token->decimals : params->decimals) << std::endl;
				}
				else if(subject == "contracts")
				{
					for(const auto& entry : wallet.get_contracts(index)) {
						std::cout << entry.first << std::endl;
					}
				}
				else if(subject == "seed")
				{
					std::cout << mmx::mnemonic::words_to_string(wallet.get_mnemonic_seed(index)) << std::endl;
				}
				else {
					std::cerr << "Help: mmx wallet get [address | balance | amount | contracts | seed]" << std::endl;
				}
			}
			else if(command == "send")
			{
				const auto token = get_token(node, contract);

				const auto mojo = mmx::to_amount(amount, token->decimals);
				if(!mojo) {
					vnx::log_error() << "Invalid amount: " << amount << " (-a | --amount)";
					goto failed;
				}
				if(target == mmx::addr_t()) {
					vnx::log_error() << "Missing destination address argument: -t | --target";
					goto failed;
				}
				if(wallet.is_locked(index)) {
					spend_options.passphrase = vnx::input_password("Passphrase: ");
				}
				const auto tx = wallet.send(index, mojo, target, contract, spend_options);
				std::cout << "Sent " << mmx::to_value(mojo, token->decimals) << " " << token->symbol << " (" << mojo << ") to " << target << std::endl;
				std::cout << "Transaction ID: " << tx->id << std::endl;
			}
			else if(command == "send_from")
			{
				const auto token = get_token(node, contract);

				const auto mojo = mmx::to_amount(amount, token->decimals);
				if(!mojo) {
					vnx::log_error() << "Invalid amount: " << amount << " (-a | --amount)";
					goto failed;
				}
				if(source == mmx::addr_t()) {
					vnx::log_error() << "Missing source address argument: -s | --source";
					goto failed;
				}
				if(target == mmx::addr_t()) {
					vnx::log_error() << "Missing destination address argument: -t | --target";
					goto failed;
				}
				if(wallet.is_locked(index)) {
					spend_options.passphrase = vnx::input_password("Passphrase: ");
				}
				const auto tx = wallet.send_from(index, mojo, target, source, contract, spend_options);
				std::cout << "Sent " << mmx::to_value(mojo, token->decimals) << " " << token->symbol << " (" << mojo << ") to " << target << std::endl;
				std::cout << "Transaction ID: " << tx->id << std::endl;
			}
			else if(command == "transfer")
			{
				if(target == mmx::addr_t()) {
					vnx::log_error() << "Missing destination address argument: -t | --target";
					goto failed;
				}
				if(wallet.is_locked(index)) {
					spend_options.passphrase = vnx::input_password("Passphrase: ");
				}
				const auto tx = wallet.send(index, 1, target, contract, spend_options);
				std::cout << "Sent " << contract << " to " << target << std::endl;
				std::cout << "Transaction ID: " << tx->id << std::endl;
			}
			else if(command == "mint")
			{
				const auto token = get_token(node, contract);

				const auto mojo = mmx::to_amount(amount, token->decimals);
				if(!mojo) {
					vnx::log_error() << "Invalid amount: " << amount << " (-a | --amount)";
					goto failed;
				}
				if(target == mmx::addr_t()) {
					vnx::log_error() << "Missing destination address argument: -t | --target";
					goto failed;
				}
				if(!spend_options.user) {
					spend_options.user = to_addr(node.read_storage_field(contract, "owner").first);
				}
				if(wallet.is_locked(index)) {
					spend_options.passphrase = vnx::input_password("Passphrase: ");
				}
				const auto tx = wallet.execute(index, contract, "mint_to", {vnx::Variant(target.to_string()), vnx::Variant(mojo)}, nullptr, spend_options);
				std::cout << "Minted " << mmx::to_value(mojo, token->decimals) << " (" << mojo << ") " << token->symbol << " to " << target << std::endl;
				std::cout << "Transaction ID: " << tx->id << std::endl;
			}
			else if(command == "deploy")
			{
				std::string file_path;
				std::vector<vnx::Variant> init_args;
				vnx::read_config("$3", file_path);
				vnx::read_config("$4", init_args);

				std::shared_ptr<mmx::Contract> payload;
				if(contract_addr.empty()) {
					payload = vnx::read_from_file<mmx::Contract>(file_path);
					if(!payload) {
						vnx::log_error() << "Failed to read contract from file: " << file_path;
						goto failed;
					}
				} else {
					auto exec = mmx::contract::Executable::create();
					exec->binary = mmx::addr_t(contract_addr);
					exec->init_method = file_path;
					exec->init_args = init_args;
					payload = exec;
				}
				if(wallet.is_locked(index)) {
					spend_options.passphrase = vnx::input_password("Passphrase: ");
				}
				const auto tx = wallet.deploy(index, payload, spend_options);
				std::cout << "Deployed " << payload->get_type_name() << " as [" << mmx::addr_t(tx->id) << "]" << std::endl;
				std::cout << "Transaction ID: " << tx->id << std::endl;
			}
			else if(command == "exec" || command == "deposit")
			{
				std::string method;
				std::vector<vnx::Variant> args;
				vnx::read_config("$3", method);
				{
					vnx::Variant tmp;
					for(int i = 4; vnx::read_config("$" + std::to_string(i), tmp); ++i) {
						args.push_back(tmp);
					}
				}
				if(!spend_options.user && offset >= 0) {
					spend_options.user = wallet.get_address(index, offset);
				}
				if(wallet.is_locked(index)) {
					spend_options.passphrase = vnx::input_password("Passphrase: ");
				}
				std::shared_ptr<const mmx::Transaction> tx;
				if(command == "exec") {
					tx = wallet.execute(index, contract, method, args, nullptr, spend_options);
					std::cout << "Executed " << method << "() with ";
				} else {
					if(method.empty()) {
						method = "deposit";
					}
					const auto token = get_token(node, contract);
					const auto mojo = mmx::to_amount(amount, token->decimals);
					tx = wallet.deposit(index, target, method, args, mojo, contract, spend_options);
					std::cout << "Deposited " << mmx::to_value(mojo, token->decimals) << " " << token->symbol << " via " << method << "() with ";
				}
				vnx::PrettyPrinter printer(std::cout);
				vnx::accept(printer, args);
				std::cout << std::endl;
				if(tx) {
					std::cout << "Transaction ID: " << tx->id << std::endl;
				}
			}
			else if(command == "offer")
			{
				std::string subject;
				if(!vnx::read_config("$3", subject))
				{
					const auto bid = contract;
					const auto ask = ask_currency;
					if(bid == ask) {
						vnx::log_error() << "Ask == Bid!";
						goto failed;
					}
					const auto bid_token = get_token(node, bid);
					const auto ask_token = get_token(node, ask);
					const auto bid_symbol = bid_token->symbol;

					const uint64_t bid_value = mmx::to_amount(amount, bid_token->decimals);
					const uint64_t ask_value = mmx::to_amount(ask_amount, ask_token->decimals);
					if(bid_value == 0 || ask_value == 0) {
						vnx::log_error() << "Invalid amount! (-a | -b)";
						goto failed;
					}
					if(wallet.is_locked(index)) {
						spend_options.passphrase = vnx::input_password("Passphrase: ");
					}
					std::cout << "Offering " << amount << " " << bid_token->symbol
							<< " for " << ask_amount << " " << ask_token->symbol << std::endl;

					if(auto tx = wallet.make_offer(index, offset, bid_value, bid, ask_value, ask, spend_options)) {
						std::cout << "Contract: " << mmx::addr_t(tx->id) << std::endl;
						std::cout << "Transaction ID: " << tx->id << std::endl;
					}
				}
				else if(subject == "cancel") {
					mmx::addr_t address;
					vnx::read_config("$4", address);

					auto offer = std::dynamic_pointer_cast<const mmx::contract::Executable>(node.get_contract(address));
					if(!offer || offer->binary != params->offer_binary) {
						vnx::log_error() << "No such offer: " << address; goto failed;
					}
					if(wallet.is_locked(index)) {
						spend_options.passphrase = vnx::input_password("Passphrase: ");
					}
					if(auto tx = wallet.cancel_offer(index, address, spend_options)) {
						std::cout << "Transaction ID: " << tx->id << std::endl;
					}
				}
				else if(subject == "withdraw")
				{
					mmx::addr_t address;
					vnx::read_config("$4", address);

					auto offer = std::dynamic_pointer_cast<const mmx::contract::Executable>(node.get_contract(address));
					if(!offer || offer->binary != params->offer_binary) {
						vnx::log_error() << "No such offer: " << address; goto failed;
					}
					if(wallet.is_locked(index)) {
						spend_options.passphrase = vnx::input_password("Passphrase: ");
					}
					if(auto tx = wallet.offer_withdraw(index, address, spend_options)) {
						std::cout << "Transaction ID: " << tx->id << std::endl;
					}
				}
				else {
					std::cerr << "mmx wallet offer [cancel | withdraw]" << std::endl;
				}
			}
			else if(command == "trade")
			{
				mmx::addr_t address;
				vnx::read_config("$3", address);

				const auto data = node.get_offer(address);
				const auto bid_token = get_token(node, data.bid_currency);
				const auto ask_token = get_token(node, data.ask_currency);
				const uint64_t ask_amount = mmx::to_amount(amount, ask_token->decimals);
				const uint64_t bid_amount = (uint256_t(ask_amount) * data.inv_price) >> 64;

				std::cout << "You pay:     "
						<< mmx::to_value(ask_amount, ask_token->decimals) << " " << ask_token->symbol << " [" << data.ask_currency << "]" << std::endl;
				std::cout << "You receive: "
						<< mmx::to_value(bid_amount, bid_token->decimals) << " " << bid_token->symbol << " [" << data.bid_currency << "]" << std::endl;

				if(pre_accept || accept_prompt()) {
					if(wallet.is_locked(index)) {
						spend_options.passphrase = vnx::input_password("Passphrase: ");
					}
					if(auto tx = wallet.offer_trade(index, address, ask_amount, offset, spend_options)) {
						std::cout << "Transaction ID: " << tx->id << std::endl;
					}
				}
			}
			else if(command == "accept")
			{
				mmx::addr_t address;
				vnx::read_config("$3", address);

				const auto data = node.get_offer(address);
				const auto bid_token = get_token(node, data.bid_currency);
				const auto ask_token = get_token(node, data.ask_currency);

				std::cout << "You send:    "
						<< mmx::to_value(data.ask_amount, ask_token->decimals) << " " << ask_token->symbol << " [" << data.ask_currency << "]" << std::endl;
				std::cout << "You receive: "
						<< mmx::to_value(data.bid_balance, bid_token->decimals) << " " << bid_token->symbol << " [" << data.bid_currency << "]" << std::endl;

				if(pre_accept || accept_prompt()) {
					if(wallet.is_locked(index)) {
						spend_options.passphrase = vnx::input_password("Passphrase: ");
					}
					if(auto tx = wallet.accept_offer(index, address, offset, spend_options)) {
						std::cout << "Transaction ID: " << tx->id << std::endl;
					}
				}
			}
			else if(command == "buy" || command == "sell")
			{
				if(min_amount < 0 || min_amount > 1) {
					std::cerr << "Invalid min amount: " << min_amount << " (needs to be between 0 and 1, ie. between 0% and 100%)" << std::endl;
					goto failed;
				}
				const auto i = (command == "sell" ? 0 : 1);
				const auto k = (i + 1) % 2;
				const auto info = node.get_swap_info(contract);
				const auto token_i = get_token(node, info.tokens[i]);
				const auto token_k = get_token(node, info.tokens[k]);
				const auto deposit_amount = mmx::to_amount(amount, token_i->decimals);
				const auto trade_estimate = node.get_swap_trade_estimate(contract, i, deposit_amount);
				const auto expected_amount = trade_estimate[0];

				if(expected_amount.upper()) {
					throw std::logic_error("amount overflow");
				}
				const uint64_t min_trade_amount = expected_amount.lower() * min_amount;

				std::cout << "You send: " << mmx::to_value(deposit_amount, token_i->decimals) << " " << token_i->symbol << std::endl;
				std::cout << "You receive at least:  " << mmx::to_value(min_trade_amount, token_k->decimals) << " " << token_k->symbol << std::endl;
				std::cout << "You expect to receive: " << mmx::to_value(expected_amount, token_k->decimals) << " " << token_k->symbol << " (estimated)" << std::endl;
				std::cout << "Fee:   " << mmx::to_value(trade_estimate[1], token_k->decimals) << " " << token_k->symbol << std::endl;
				std::cout << "Price: " << (command == "sell" ? double(expected_amount.lower()) / deposit_amount : double(deposit_amount) / expected_amount.lower())
						<< " " << (command == "sell" ? token_k : token_i)->symbol << " / " << (command == "sell" ? token_i : token_k)->symbol << std::endl;

				if(pre_accept || accept_prompt()) {
					if(wallet.is_locked(index)) {
						spend_options.passphrase = vnx::input_password("Passphrase: ");
					}
					if(auto tx = wallet.swap_trade(index, contract, deposit_amount, info.tokens[i], min_trade_amount, spend_options)) {
						std::cout << "Transaction ID: " << tx->id << std::endl;
					}
				}
			}
			else if(command == "swap")
			{
				std::string action;
				vnx::read_config("$3", action);

				if(action == "add")
				{
					const auto usage = "mmx wallet swap add -a <amount> -b <amount> -x <contract>";
					std::array<uint64_t, 2> add_amount = {};
					const auto info = node.get_swap_info(contract);
					const auto token_0 = get_token(node, info.tokens[0]);
					const auto token_1 = get_token(node, info.tokens[1]);

					if(offset < 0 || size_t(offset) >= info.fee_rates.size()) {
						std::cerr << "invalid pool offset: " << offset << std::endl;
						goto failed;
					}
					if(have_amount) {
						add_amount[0] = mmx::to_amount(amount, token_0->decimals);
					} else {
						if(!have_ask_amount) {
							std::cerr << usage << std::endl;
							goto failed;
						}
						if(info.balance[1]) {
							add_amount[0] = (uint256_t(mmx::to_amount(ask_amount, token_1->decimals)) * info.balance[0]) / info.balance[1];
						}
					}
					if(have_ask_amount) {
						add_amount[1] = mmx::to_amount(ask_amount, token_1->decimals);
					} else {
						if(!have_amount) {
							std::cerr << usage << std::endl;
							goto failed;
						}
						if(info.balance[0]) {
							add_amount[1] = (uint256_t(mmx::to_amount(amount, token_0->decimals)) * info.balance[1]) / info.balance[0];
						}
					}
					if(add_amount[0]) {
						std::cout << "You deposit: " << mmx::to_value(add_amount[0], token_0->decimals) << " " << token_0->symbol;
						if(!add_amount[1]) {
							std::cout << " (sell order)";
						}
						std::cout << std::endl;
					}
					if(add_amount[1]) {
						std::cout << "You deposit: " << mmx::to_value(add_amount[1], token_1->decimals) << " " << token_1->symbol;
						if(!add_amount[0]) {
							std::cout << " (buy order)";
						}
						std::cout << std::endl;
					}
					std::cout << "Pool Fee: " << info.fee_rates[offset] * 100 << " %" << std::endl;

					if(!add_amount[0] && !add_amount[1]) {
						std::cerr << usage << std::endl;
						goto failed;
					}
					if(pre_accept || accept_prompt()) {
						if(wallet.is_locked(index)) {
							spend_options.passphrase = vnx::input_password("Passphrase: ");
						}
						if(auto tx = wallet.swap_add_liquid(index, contract, add_amount, offset, spend_options)) {
							std::cout << "Transaction ID: " << tx->id << std::endl;
						}
					}
				}
				else if(action == "remove")
				{
					std::array<uint64_t, 2> rem_amount = {};
					const auto info = node.get_swap_info(contract);
					const auto user_info = node.get_swap_user_info(contract, wallet.get_address(index, offset));
					const auto token_0 = get_token(node, info.tokens[0]);
					const auto token_1 = get_token(node, info.tokens[1]);
					if(have_amount) {
						rem_amount[0] = mmx::to_amount(amount, token_0->decimals);
					}
					if(have_ask_amount) {
						rem_amount[1] = mmx::to_amount(ask_amount, token_1->decimals);
					}
					if(!have_amount && !have_ask_amount) {
						for(int i = 0; i < 2; ++i) {
							rem_amount[i] = user_info.balance[i];
						}
					}
					for(int i = 0; i < 2; ++i) {
						if(!user_info.balance[i].upper()) {
							rem_amount[i] = std::min(rem_amount[i], user_info.balance[i].lower());
						}
					}
					if(!rem_amount[0] && !rem_amount[1]) {
						std::cerr << "Nothing to remove." << std::endl;
						goto failed;
					}
					for(int i = 0; i < 2; ++i) {
						const auto token = i ? token_1 : token_0;
						if(rem_amount[i]) {
							std::cout << "You remove:  " << mmx::to_value(rem_amount[i], token->decimals) << " " << token->symbol << std::endl;
						}
					}
					for(int i = 0; i < 2; ++i) {
						const auto token = i ? token_1 : token_0;
						if(user_info.equivalent_liquidity[i]) {
							std::cout << "You receive: " << mmx::to_value(user_info.equivalent_liquidity[i], token->decimals) << " " << token->symbol << " (approximate)" << std::endl;
						}
					}
					if(pre_accept || accept_prompt()) {
						if(wallet.is_locked(index)) {
							spend_options.passphrase = vnx::input_password("Passphrase: ");
						}
						if(auto tx = wallet.swap_rem_liquid(index, contract, rem_amount, spend_options)) {
							std::cout << "Transaction ID: " << tx->id << std::endl;
						}
					}
				}
				else if(action == "payout")
				{
					if(wallet.is_locked(index)) {
						spend_options.passphrase = vnx::input_password("Passphrase: ");
					}
					spend_options.user = wallet.get_address(index, offset);

					if(auto tx = wallet.execute(index, contract, "payout", {}, nullptr, spend_options)) {
						std::cout << "Transaction ID: " << tx->id << std::endl;
					}
				}
				else if(action == "info")
				{
					const auto info = node.get_swap_info(contract);
					const auto user_info = node.get_swap_user_info(contract, wallet.get_address(index, offset));
					const auto token_0 = get_token(node, info.tokens[0]);
					const auto token_1 = get_token(node, info.tokens[1]);
					for(int i = 0; i < 2; ++i) {
						const auto token = i ? token_1 : token_0;
						std::cout << "Balance: " << mmx::to_value(user_info.balance[i], token->decimals) << " " << token->symbol << std::endl;
					}
					for(int i = 0; i < 2; ++i) {
						const auto token = i ? token_1 : token_0;
						if(user_info.balance[i]) {
							std::cout << "Earned fees: " << mmx::to_value(user_info.fees_earned[i], token->decimals) << " " << token->symbol << " (approximate)" << std::endl;
						}
					}
					std::cout << "Unlock height: " << user_info.unlock_height << std::endl;
				}
				else {
					std::cerr << "mmx wallet swap [info | add | remove | payout] [-a <amount>] [-b <amount>] -x <contract>" << std::endl;
				}
			}
			else if(command == "log")
			{
				int64_t since = 0;
				vnx::read_config("$3", since);

				show_history(wallet.get_history(index, since), node, params);
			}
			else if(command == "lock")
			{
				wallet.lock(index);
			}
			else if(command == "unlock")
			{
				wallet.unlock(index, vnx::input_password("Passphrase: "));
			}
			else if(command == "create")
			{
				if(file_name.empty()) {
					file_name = "wallet.dat";
				}
				file_name = mmx_home + file_name;

				if(vnx::File(file_name).exists()) {
					vnx::log_error() << "Wallet file '" << file_name << "' already exists!";
					goto failed;
				}
				std::string seed_str;
				std::vector<std::string> seed_words;
				vnx::read_config("$3", seed_str);
				vnx::read_config("mnemonic", seed_words);

				mmx::KeyFile wallet;
				if(seed_str.empty()) {
					if(seed_words.empty()) {
						wallet.seed_value = mmx::hash_t::random();
					} else {
						wallet.seed_value = mmx::mnemonic::words_to_seed(seed_words);
					}
				} else {
					if(seed_str.size() == 64) {
						wallet.seed_value.from_string(seed_str);
					} else {
						vnx::log_error() << "Invalid seed value: '" << seed_str << "'";
						goto failed;
					}
				}
				vnx::write_to_file(file_name, wallet);
				std::filesystem::permissions(file_name, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);

				// TODO: add key file in config/local/Wallet.json

				std::cout << "Created wallet '" << file_name << "' with seed: "
						<< std::endl << wallet.seed_value << std::endl;
			}
			else {
				std::cerr << "Help: mmx wallet [show | get | log | send | send_from | offer | trade | accept | buy | sell | swap | mint | deploy | exec | transfer | create | accounts | keys | lock | unlock]" << std::endl;
			}
		}
		else if(module == "node")
		{
			std::string node_url = ":11331";
			vnx::read_config("node", node_url);

			vnx::Handle<vnx::Proxy> proxy = new vnx::Proxy("Proxy", vnx::Endpoint::from_url(node_url));
			proxy->forward_list = {"Router", "Node"};
			proxy.start_detached();
			try {
				params = node.get_params();
			} catch(...) {
				// ignore
			}
			mmx::RouterClient router("Router");

			if(command == "balance")
			{
				mmx::addr_t address;
				if(!vnx::read_config("$3", address)) {
					vnx::log_error() << "Missing address argument! (node balances <address>)";
					goto failed;
				}
				for(const auto& entry : node.get_total_balances({address}))
				{
					const auto contract = get_contract(node, entry.first);
					if(!std::dynamic_pointer_cast<const mmx::contract::NFT>(contract)) {
						const auto token = std::dynamic_pointer_cast<const mmx::contract::TokenBase>(contract);
						const auto decimals = token ? token->decimals : params->decimals;
						std::cout << "Balance: " << to_value_128(entry.second, decimals) << " " << (token ? token->symbol : "MMX") << " (" << entry.second << ")" << std::endl;
					}
				}
			}
			else if(command == "info")
			{
				const auto info = node.get_network_info();
				std::cout << "Synced:     " << (node.get_synced_height() ? "Yes" : "No") << std::endl;
				std::cout << "Height:     " << info->height << std::endl;
				std::cout << "Netspace:   " << info->total_space / pow(1000, 5) << " PB (" << info->netspace_ratio * 100 << " % physical)" << std::endl;
				std::cout << "VDF Speed:  " << info->time_diff * params->time_diff_constant / params->block_time / 1e6 << " MH/s" << std::endl;
				std::cout << "Reward:     " << info->block_reward / pow(10, params->decimals) << " MMX" << std::endl;
				std::cout << "Supply:     " << info->total_supply / pow(10, params->decimals) << " MMX" << std::endl;
				std::cout << "Block Size: " << info->block_size * 100 << " %" << std::endl;
				std::cout << "N(Address): " << info->address_count << std::endl;
				for(uint32_t i = 0; i < 10 && i < info->height; ++i) {
					const auto hash = node.get_block_hash(info->height - i);
					std::cout << "Block[" << (info->height - i) << "] " << (hash ? *hash : mmx::hash_t()) << std::endl;
				}
			}
			else if(command == "history")
			{
				mmx::addr_t address;
				if(!vnx::read_config("$3", address)) {
					vnx::log_error() << "Missing address argument! (node history <address> [since])";
					goto failed;
				}
				int64_t since = 0;
				vnx::read_config("$4", since);

				show_history(node.get_history({address}, since), node, params);
			}
			else if(command == "peers")
			{
				auto info = router.get_peer_info();
				uint32_t max_height = 0;
				for(const auto& peer : info->peers) {
					max_height = std::max(max_height, peer.height);
				}
				for(const auto& peer : info->peers) {
					std::cout << "[" << peer.address << "]";
					for(size_t i = peer.address.size(); i < 15; ++i) {
						std::cout << " ";
					}
					std::cout << " height = ";
					const auto height = std::to_string(peer.height);
					for(size_t i = height.size(); i < std::to_string(max_height).size(); ++i) {
						std::cout << " ";
					}
					if(peer.is_synced) {
						std::cout << " ";
					} else {
						std::cout << "!";
					}
					std::cout << height;
					std::cout << ", " << vnx::to_string_value(peer.type) << " (" << peer.version / 100 << "." << peer.version % 100 << ")";
					std::cout << ", " << peer.bytes_recv / 1024 / 1024 << " MB recv";
					std::cout << ", " << peer.bytes_send / 1024 / 1024 << " MB sent";
					std::cout << ", since " << (peer.connect_time_ms / 60000) << " min";
					std::cout << ", " << peer.ping_ms << " ms ping";
					std::cout << ", " << peer.credits << " credits";
					std::cout << ", " << int64_t(1e3 * peer.pending_cost) / 1e3 << " pending";
					std::cout << ", " << (peer.recv_timeout_ms / 100) / 10. << " sec timeout";
					if(peer.is_outbound) {
						std::cout << ", outbound";
					}
					if(peer.is_blocked) {
						std::cout << ", blocked";
					}
					if(peer.is_paused) {
						std::cout << ", paused";
					}
					std::cout << std::endl;
				}
			}
			else if(command == "sync")
			{
				node.start_sync(true);
				std::cout << "Started sync ..." << std::endl;
				while(true) {
					if(auto height = node.get_synced_height()) {
						std::cout << "Finished sync at height: " << *height << std::endl;
						break;
					}
					std::cout << node.get_height() << std::endl;
					std::this_thread::sleep_for(std::chrono::milliseconds(2000));
				}
			}
			else if(command == "revert")
			{
				uint32_t height = 0;
				if(!vnx::read_config("$3", height)) {
					std::cout << "mmx node revert <height>" << std::endl;
					goto failed;
				}
				std::cout << "Reverting to height " << height << " ..." << std::endl;
				node.revert_sync(height);
			}
			else if(command == "discover")
			{
				router.discover();
				std::this_thread::sleep_for(std::chrono::milliseconds(2000));

				const auto peers = router.get_known_peers();
				std::cout << "Got " << peers.size() << " known peers" << std::endl;
			}
			else if(command == "tx")
			{
				mmx::hash_t txid;
				vnx::read_config("$3", txid);

				const auto tx = node.get_transaction(txid, true);
				if(!tx) {
					vnx::log_error() << "No such transaction: " << txid;
					goto failed;
				}
				const auto height = node.get_height();
				const auto tx_height = node.get_tx_height(txid);
				if(tx_height) {
					std::cout << "Confirmations: " << height - (*tx_height) + 1 << std::endl;
				} else {
					std::cout << "Confirmations: none yet" << std::endl;
				}
				size_t i = 0;
				for(const auto& in : tx->get_inputs()) {
					std::cout << "Input[" << i++ << "]: ";
					if(auto token = get_token(node, in.contract, false)) {
						std::cout << in.amount / pow(10, token->decimals) << " " << token->symbol << " (" << in.amount << ") <- " << in.address << std::endl;
					} else {
						std::cout << in.amount << " [" << in.contract << "]" << std::endl;
					}
				}
				i = 0;
				for(const auto& out : tx->get_outputs()) {
					std::cout << "Output[" << i++ << "]: ";
					if(auto token = get_token(node, out.contract, false)) {
						std::cout << out.amount / pow(10, token->decimals) << " " << token->symbol << " (" << out.amount << ") -> " << out.address << std::endl;
					} else {
						std::cout << out.amount << " [" << out.contract << "]" << std::endl;
					}
				}
			}
			else if(command == "get")
			{
				std::string subject;
				vnx::read_config("$3", subject);

				if(subject == "balance")
				{
					mmx::addr_t address;
					vnx::read_config("$4", address);

					const auto token = get_token(node, contract);
					std::cout << to_value_128(node.get_balance(address, contract), token->decimals) << std::endl;
				}
				else if(subject == "amount")
				{
					mmx::addr_t address;
					vnx::read_config("$4", address);

					std::cout << node.get_balance(address, contract) << std::endl;
				}
				else if(subject == "height")
				{
					std::cout << node.get_height() << std::endl;
				}
				else if(subject == "block")
				{
					std::string arg;
					vnx::read_config("$4", arg);

					std::shared_ptr<const mmx::Block> block;
					if(arg.size() == 64) {
						mmx::hash_t hash;
						hash.from_string(arg);
						block = node.get_block(hash);
					} else {
						block = node.get_block_at(std::strtoul(arg.c_str(), nullptr, 10));
					}
					vnx::PrettyPrinter printer(std::cout);
					vnx::accept(printer, block);
					std::cout << std::endl;
				}
				else if(subject == "header")
				{
					std::string arg;
					vnx::read_config("$4", arg);

					std::shared_ptr<const mmx::BlockHeader> block;
					if(arg.size() == 64) {
						mmx::hash_t hash;
						hash.from_string(arg);
						block = node.get_header(hash);
					} else {
						block = node.get_header_at(std::strtoul(arg.c_str(), nullptr, 10));
					}
					vnx::PrettyPrinter printer(std::cout);
					vnx::accept(printer, block);
					std::cout << std::endl;
				}
				else if(subject == "tx")
				{
					mmx::hash_t txid;
					vnx::read_config("$4", txid);

					const auto tx = node.get_transaction(txid, true);
					{
						std::stringstream ss;
						vnx::PrettyPrinter printer(ss);
						vnx::accept(printer, tx);
						std::cout << ss.str() << std::endl;
					}
				}
				else if(subject == "contract")
				{
					mmx::addr_t address;
					vnx::read_config("$4", address);

					const auto contract = node.get_contract(address);
					{
						std::stringstream ss;
						vnx::PrettyPrinter printer(ss);
						vnx::accept(printer, contract);
						std::cout << ss.str() << std::endl;
					}
				}
				else if(subject == "peers")
				{
					int64_t max_count = 10;
					vnx::read_config("$4", max_count);

					for(const auto& peer : router.get_connected_peers()) {
						std::cout << peer << std::endl;
					}
				}
				else if(subject == "netspace")
				{
					const auto height = node.get_height();
					const auto peak = node.get_header_at(height);
					std::cout << mmx::calc_total_netspace(params, peak ? peak->space_diff : 0) << std::endl;
				}
				else if(subject == "supply")
				{
					std::cout << node.get_total_supply(contract) << std::endl;
				}
				else {
					std::cerr << "Help: mmx node get [height | tx | contract | balance | amount | block | header | peers | netspace | supply]" << std::endl;
				}
			}
			else if(command == "read")
			{
				std::string address;
				vnx::read_config("$3", address);

				if(address.empty()) {
					std::cout << "{" << std::endl;
					for(const auto& entry : node.read_storage(contract)) {
						std::cout << "  \"" << entry.first << "\": " << to_string(entry.second) << "," << std::endl;
					}
					std::cout << "}" << std::endl;
				} else {
					vnx::Variant tmp;
					vnx::from_string(address, tmp);

					uint64_t addr = 0;
					mmx::vm::varptr_t var;
					if(tmp.is_string()) {
						const auto res = node.read_storage_field(contract, address);
						var = res.first;
						addr = res.second;
					} else {
						addr = tmp.to<uint64_t>();
						var = node.read_storage_var(contract, addr);
					}
					if(var) {
						switch(var->type) {
							case mmx::vm::TYPE_ARRAY: {
								std::cout << '[';
								int i = 0;
								for(const auto& entry : node.read_storage_array(contract, addr)) {
									if(i++) {
										std::cout << ", ";
									}
									std::cout << to_string(entry);
								}
								std::cout << ']' << std::endl;
								break;
							}
							case mmx::vm::TYPE_MAP:
								std::cout << '{' << std::endl;
								for(const auto& entry : node.read_storage_map(contract, addr)) {
									std::cout << "  " << to_string(entry.first) << ": " << to_string(entry.second) << ',' << std::endl;
								}
								std::cout << '}' << std::endl;
								break;
							default:
								std::cout << to_string(var) << std::endl;
						}
					} else {
						std::cout << to_string(var) << std::endl;
					}
				}
			}
			else if(command == "dump")
			{
				for(const auto& entry : node.dump_storage(contract)) {
					std::cout << "[0x" << std::hex << entry.first << std::dec << "] " << to_string(entry.second) << std::endl;
				}
			}
			else if(command == "call")
			{
				std::string method;
				std::vector<vnx::Variant> args;
				vnx::read_config("$3", method);
				vnx::read_config("$4", args);

				const auto res = node.call_contract(contract, method, args);
				vnx::PrettyPrinter printer(std::cout);
				vnx::accept(printer, res);
				std::cout << std::endl;
			}
			else if(command == "dump_code")
			{
				std::string method;
				vnx::read_config("$3", method);

				auto value = node.get_contract(contract);
				auto bin = std::dynamic_pointer_cast<const mmx::contract::Binary>(value);
				if(!bin) {
					if(auto exec = std::dynamic_pointer_cast<const mmx::contract::Executable>(value)) {
						bin = std::dynamic_pointer_cast<const mmx::contract::Binary>(node.get_contract(exec->binary));
						if(!bin) {
							vnx::log_error() << "No such binary: " << exec->binary;
							goto failed;
						}
					} else {
						vnx::log_error() << "No such executable: " << contract;
						goto failed;
					}
				}
				if(bin) {
					mmx::vm::dump_code(std::cout, bin, method.empty() ? vnx::optional<std::string>() : method);
				}
			}
			else if(command == "fetch")
			{
				std::string subject;
				vnx::read_config("$3", subject);

				if(subject == "block" || subject == "header")
				{
					int64_t height = 0;
					std::string from_peer;
					vnx::read_config("$4", from_peer);
					vnx::read_config("$5", height);

					if(from_peer.empty()) {
						vnx::log_error() << "Missing peer argument: node fetch [block | header] <peer> <height>";
						goto failed;
					}
					if(height < 0) {
						vnx::log_error() << "Invalid height: " << height;
						goto failed;
					}
					const auto block = router.fetch_block_at(height, from_peer);
					{
						std::stringstream ss;
						vnx::PrettyPrinter printer(ss);
						if(subject == "header") {
							vnx::accept(printer, block ? block->get_header() : nullptr);
						} else {
							vnx::accept(printer, block);
						}
						std::cout << ss.str() << std::endl;
					}
				}
				else {
					std::cerr << "Help: mmx node fetch [block | header]" << std::endl;
				}
			}
			else if(command == "offers")
			{
				std::string filter;
				vnx::read_config("$3", filter);

				for(const auto& data : node.get_offers())
				{
					if(data.is_open()) {
						if(!filter.empty() && filter != "open") {
							continue;
						}
						std::cout << "OPEN";
					} else {
						if(!filter.empty() && filter != "closed") {
							continue;
						}
						std::cout << "CLOSED";
					}
					std::cout << " [" << data.address << "] [" << data.height << "] " << std::endl;

					std::cout << "  They offer:   ";
					const auto bid_token = get_token(node, data.bid_currency);
					if(bid_token) {
						std::cout << mmx::to_value(data.bid_balance, bid_token->decimals) << " " << bid_token->symbol << " [" << data.bid_currency << "]";
					} else {
						std::cout << data.bid_balance << " [" << data.bid_currency << "]";
					}
					std::cout << std::endl;

					std::cout << "  They ask for: ";
					const auto ask_token = get_token(node, data.ask_currency);
					const uint64_t ask_amount = (uint256_t(data.bid_balance) * data.inv_price) >> 64;
					if(ask_token) {
						std::cout << mmx::to_value(ask_amount, ask_token->decimals) << " " << ask_token->symbol << " [" << data.ask_currency << "]";
					} else {
						std::cout << ask_amount << " [" << data.ask_currency << "]";
					}
					std::cout << std::endl;

					if(bid_token && ask_token) {
						std::cout << "  Price: " << data.price << " " << ask_token->symbol << " / " << bid_token->symbol << std::endl;
					}
				}
			}
			else if(command == "swaps")
			{
				vnx::optional<mmx::addr_t> token;
				vnx::read_config("$3", token);

				for(const auto& data : node.get_swaps(0, token))
				{
					std::cout << "[" << data.address << "] ";
					std::array<std::string, 2> symbols;
					for(int i = 0; i < 2; ++i) {
						if(auto token = get_token(node, data.tokens[i])) {
							symbols[i] = token->symbol;
							std::cout << mmx::to_value(data.balance[i], token->decimals) << " " << token->symbol << (i == 0 ? " / " : "");
						}
					}
					std::cout << " (" << data.get_price() << " " << symbols[1] << " / " << symbols[0] << ")" << std::endl;
				}
			}
			else if(command == "swap")
			{
				mmx::addr_t address;
				if(!vnx::read_config("$3", address)) {
					std::cerr << "node swap <address>" << std::endl;
					goto failed;
				}
				const auto data = node.get_swap_info(address);

				std::array<std::string, 2> symbols;
				for(int i = 0; i < 2; ++i) {
					if(auto token = get_token(node, data.tokens[i])) {
						symbols[i] = token->symbol;
						std::cout << (i ? "Currency" : "Token") << ": " << data.tokens[i] << std::endl;
						std::cout << "  Pool Balance:   " << mmx::to_value(data.balance[i], token->decimals) << " " << token->symbol << std::endl;
						std::cout << "  User Balance:   " << mmx::to_value(data.user_total[i], token->decimals) << " " << token->symbol << std::endl;
						std::cout << "  Wallet Balance: " << mmx::to_value(data.wallet[i], token->decimals) << " " << token->symbol << std::endl;
						std::cout << "  Total Fees:     " << mmx::to_value(data.fees_paid[i], token->decimals) << " " << token->symbol << std::endl;
					}
				}
				std::cout << "Price: " << data.get_price() << " " << symbols[1] << " / " << symbols[0] << std::endl;
			}
			else {
				std::cerr << "Help: mmx node [info | peers | tx | get | fetch | balance | history | offers | swaps | swap | sync | revert | call | read | dump | dump_code]" << std::endl;
			}
		}
		else if(module == "farm" || module == "harvester")
		{
			std::string node_url = ":11333";
			vnx::read_config("node", node_url);

			vnx::Handle<vnx::Proxy> proxy = new vnx::Proxy("Proxy", vnx::Endpoint::from_url(node_url));
			proxy->forward_list = {"Farmer", "Harvester", "Node"};
			proxy.start_detached();
			{
				vnx::ProxyClient client(proxy.get_name());
				client.login(user, passwd);
			}
			try {
				params = node.get_params();
			} catch(...) {
				// ignore
			}
			mmx::FarmerClient farmer("Farmer");
			mmx::HarvesterClient harvester("Harvester");

			std::shared_ptr<const mmx::FarmInfo> info;
			if(module == "farm") {
				info = farmer.get_farm_info();
			} else if(module == "harvester") {
				info = harvester.get_farm_info();
			}
			if(!info) {
				goto failed;
			}

			if(command == "info")
			{
				if(info->harvester) {
					std::cout << "[" << *info->harvester << "]" << std::endl;
				}
				std::cout << "Physical size: " << info->total_bytes / pow(1000, 4) << " TB" << std::endl;
				const auto virtual_bytes = mmx::calc_virtual_plot_size(params, info->total_balance);
				std::cout << "Virtual size:  " << info->total_balance / pow(10, params->decimals) << " MMX ("
						<< virtual_bytes / pow(1000, 4) << " TB)" << std::endl;
				std::cout << "Total size:    " << (info->total_bytes + virtual_bytes) / pow(1000, 4) << " TB" << std::endl;
				for(const auto& entry : info->plot_count) {
					std::cout << "K" << int(entry.first) << ": " << entry.second << " plots" << std::endl;
				}
				for(const auto& entry : info->harvester_bytes) {
					std::cout << "[" << entry.first << "] " << entry.second / pow(1000, 4) << " TB" << std::endl;
				}
			}
			else if(command == "reload")
			{
				harvester.reload();
			}
			else if(command == "get")
			{
				std::string subject;
				vnx::read_config("$3", subject);

				if(subject == "space")
				{
					std::cout << info->total_bytes << std::endl;
				}
				else if(subject == "dirs")
				{
					for(const auto& entry : info->plot_dirs) {
						std::cout << entry << std::endl;
					}
				}
				else {
					std::cerr << "Help: mmx farm get [space | dirs]" << std::endl;
				}
			}
			else if(command == "add")
			{
				std::string path;
				if(vnx::read_config("$3", path)) {
					harvester.add_plot_dir(path);
				} else {
					std::cout << "mmx farm add <path>" << std::endl;
				}
			}
			else if(command == "remove")
			{
				std::string path;
				if(vnx::read_config("$3", path)) {
					harvester.rem_plot_dir(path);
				} else {
					std::cout << "mmx farm remove <path>" << std::endl;
				}
			}
			else {
				std::cerr << "Help: mmx " << module << " [info | get | add | remove | reload]" << std::endl;
			}
		}
		else {
			std::cerr << "Help: mmx [node | wallet | farm | harvester]" << std::endl;
		}
	}
	catch(const std::exception& ex) {
		vnx::log_error() << "Failed with: " << ex.what();
		goto failed;
	}

	goto exit;
failed:
	did_fail = true;
exit:
	vnx::close();

	mmx::secp256k1_free();

	return did_fail ? -1 : 0;
}


