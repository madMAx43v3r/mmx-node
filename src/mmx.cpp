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
//#include <mmx/exchange/ClientClient.hxx>
#include <mmx/Contract.hxx>
#include <mmx/contract/NFT.hxx>
#include <mmx/contract/PlotNFT.hxx>
#include <mmx/contract/TokenBase.hxx>
#include <mmx/contract/Executable.hxx>
#include <mmx/contract/VirtualPlot.hxx>
#include <mmx/KeyFile.hxx>
#include <mmx/secp256k1.hpp>
#include <mmx/hash_t.hpp>
#include <mmx/mnemonic.h>
#include <mmx/utils.h>
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
	options["bid"] = "123.456";
	options["outputs"] = "count";
	options["source"] = "address";
	options["target"] = "address";
	options["contract"] = "address";
	options["ask-currency"] = "address";
	options["fee-ratio"] = ">= 1";
	options["gas-limit"] = "MMX";
	options["user"] = "mmx-admin";
	options["passwd"] = "PASSWD";
	options["mnemonic"] = "mnemonic";

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
	double amount = 0;
	double ask_amount = 0;
	double fee_ratio = 1;
	double gas_limit = 1;
	bool pre_accept = false;
	vnx::read_config("$1", module);
	vnx::read_config("$2", command);
	vnx::read_config("yes", pre_accept);
	vnx::read_config("file", file_name);
	vnx::read_config("index", index);
	vnx::read_config("offset", offset);
	vnx::read_config("amount", amount);
	vnx::read_config("ask-amount", ask_amount);
	vnx::read_config("count", num_count);
	vnx::read_config("source", source_addr);
	vnx::read_config("target", target_addr);
	vnx::read_config("contract", contract_addr);
	vnx::read_config("ask-currency", ask_currency_addr);
	vnx::read_config("fee-ratio", fee_ratio);
	vnx::read_config("gas-limit", gas_limit);
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

			if(command != "create") {
				vnx::Handle<vnx::Proxy> proxy = new vnx::Proxy("Proxy", vnx::Endpoint::from_url(node_url));
				proxy->forward_list = {"Wallet", "Node"};
				proxy.start_detached();
				try {
					params = node.get_params();
				} catch(...) {
					// ignore
				}
			}
			mmx::WalletClient wallet("Wallet");

			// TODO: default to lowest available wallet index, not zero

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
						if(std::dynamic_pointer_cast<const mmx::contract::NFT>(contract)) {
							nfts.push_back(entry.first);
						}
						else if(auto token = get_token(node, entry.first, false)) {
							std::cout << "Balance: " << to_value_128(balance.total, token->decimals) << " " << token->symbol
									<< (balance.is_validated ? "" : "?") << " (" << balance.total << ")";
							if(entry.first != mmx::addr_t()) {
								std::cout << " [" << entry.first << "]";
							}
							std::cout << std::endl;
							is_empty = false;
						}
					}
					if(is_empty) {
						std::cout << "Balance: 0 MMX" << std::endl;
					}
					for(const auto& addr : nfts) {
						std::cout << "NFT: " << addr << std::endl;
					}
				}
				if(subject.empty() || subject == "contracts")
				{
					for(const auto& entry : wallet.get_contracts(index))
					{
						const auto& address = entry.first;
						const auto& contract = entry.second;
						std::cout << "Contract: " << address << " (" << contract->get_type_name();
						if(auto token = std::dynamic_pointer_cast<const mmx::contract::TokenBase>(contract)) {
							std::cout << ", " << token->symbol << ", " << token->name;
						}
						else if(auto exec = std::dynamic_pointer_cast<const mmx::contract::Executable>(contract)) {
							if(exec->binary == params->offer_binary) {
								const auto data = node.get_offer(address);
								if(data.state == "REVOKED") {
									std::cout << ", revoked";
								} else if(data.state == "CLOSED") {
									std::cout << ", accepted at height " << data.close_height;
								} else if(data.state == "OPEN") {
									std::cout << ", open";
								}
							}
						}
						else if(auto plot = std::dynamic_pointer_cast<const mmx::contract::VirtualPlot>(contract)) {
							const auto balance = node.get_virtual_plot_balance(entry.first);
							std::cout << ", " << balance / pow(10, params->decimals) << " MMX";
							std::cout << ", " << mmx::calc_virtual_plot_size(params, balance) / pow(1000, 4) << " TB";
						}
						else if(auto nft = std::dynamic_pointer_cast<const mmx::contract::PlotNFT>(contract)) {
							std::cout << ", name = " << nft->name << ", ";
							if(nft->unlock_height) {
								std::cout << "unlocked at " << *nft->unlock_height;
							} else {
								std::cout << "locked";
							}
							std::cout << ", server = " << vnx::to_string(nft->server_url);
						}
						std::cout << ")" << std::endl;

						for(const auto& entry : wallet.get_total_balances_for({address}))
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
					}
				}
				for(int i = 0; i < 1; ++i) {
					std::cout << "Address[" << i << "]: " << wallet.get_address(index, i) << std::endl;
				}
				if(subject.empty()) {
					std::cout << "Help: mmx wallet show [balance | contracts]" << std::endl;
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
					std::cout << "Pool Public Key:   " << keys->pool_public_key << std::endl;
					std::cout << "Farmer Public Key: " << keys->farmer_public_key << std::endl;
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

				const int64_t mojo = amount * pow(10, token->decimals);
				if(amount <= 0 || mojo <= 0) {
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
				std::cout << "Sent " << mojo / pow(10, token->decimals) << " " << token->symbol << " (" << mojo << ") to " << target << std::endl;
				std::cout << "Transaction ID: " << tx->id << std::endl;
			}
			else if(command == "send_from")
			{
				const auto token = get_token(node, contract);

				const int64_t mojo = amount * pow(10, token->decimals);
				if(amount <= 0 || mojo <= 0) {
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
				std::cout << "Sent " << mojo / pow(10, token->decimals) << " " << token->symbol << " (" << mojo << ") to " << target << std::endl;
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

				const int64_t mojo = amount * pow(10, token->decimals);
				if(amount <= 0 || mojo <= 0) {
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
				const auto tx = wallet.mint(index, mojo, target, contract, spend_options);
				std::cout << "Minted " << mojo / pow(10, token->decimals) << " (" << mojo << ") " << token->symbol << " to " << target << std::endl;
				std::cout << "Transaction ID: " << tx->id << std::endl;
			}
			else if(command == "deploy")
			{
				std::string file_path;
				vnx::read_config("$3", file_path);

				auto payload = vnx::read_from_file<mmx::Contract>(file_path);
				if(!payload) {
					vnx::log_error() << "Failed to read contract from file: " << file_path;
					goto failed;
				}
				if(wallet.is_locked(index)) {
					spend_options.passphrase = vnx::input_password("Passphrase: ");
				}
				const auto tx = wallet.deploy(index, payload, spend_options);
				std::cout << "Deployed " << payload->get_type_name() << " as [" << mmx::addr_t(tx->id) << "]" << std::endl;
				std::cout << "Transaction ID: " << tx->id << std::endl;
			}
			else if(command == "mutate")
			{
				std::string method;
				vnx::Object args;
				vnx::read_config("$3", method);
				vnx::read_config("$4", args);

				if(wallet.is_locked(index)) {
					spend_options.passphrase = vnx::input_password("Passphrase: ");
				}
				args["__type"] = method;
				const auto tx = wallet.mutate(index, contract, args, spend_options);
				std::cout << "Executed " << method << " with:" << std::endl;
				vnx::PrettyPrinter printer(std::cout);
				args.accept(printer);
				std::cout << std::endl << "Transaction ID: " << tx->id << std::endl;
			}
			else if(command == "exec" || command == "deposit")
			{
				std::string method;
				std::vector<vnx::Variant> args;
				vnx::read_config("$3", method);
				vnx::read_config("$4", args);

				if(wallet.is_locked(index)) {
					spend_options.passphrase = vnx::input_password("Passphrase: ");
				}
				std::shared_ptr<const mmx::Transaction> tx;
				if(command == "exec") {
					if(!spend_options.user) {
						spend_options.user = wallet.get_address(index, offset);
					}
					tx = wallet.execute(index, contract, method, args, spend_options);
					std::cout << "Executed " << method << "() with ";
				} else {
					if(method.empty()) {
						method = "deposit";
					}
					const auto token = get_token(node, contract);
					const int64_t mojo = amount * pow(10, token->decimals);
					tx = wallet.deposit(index, target, method, args, mojo, contract, spend_options);
					std::cout << "Deposited " << amount << " " << token->symbol << " via " << method << "() with ";
				}
				vnx::PrettyPrinter printer(std::cout);
				vnx::accept(printer, args);
				std::cout << std::endl << "Transaction ID: " << tx->id << std::endl;
			}
			else if(command == "offer")
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

				const uint64_t bid_value = amount * pow(10, bid_token->decimals);
				const uint64_t ask_value = ask_amount * pow(10, ask_token->decimals);
				if(bid_value == 0 || ask_value == 0) {
					vnx::log_error() << "Invalid amount! (-a | -b)";
					goto failed;
				}
				if(wallet.is_locked(index)) {
					spend_options.passphrase = vnx::input_password("Passphrase: ");
				}
				std::cout << "Offering " << amount << " " << bid_token->symbol
						<< " for " << ask_amount << " " << ask_token->symbol << std::endl;

				auto tx = wallet.make_offer(index, offset, bid_value, bid, ask_value, ask, spend_options);
				std::cout << "Contract: " << mmx::addr_t(tx->id) << std::endl;
				std::cout << "Transaction ID: " << tx->id << std::endl;
			}
			else if(command == "accept")
			{
				mmx::addr_t address;
				vnx::read_config("$3", address);

				const auto data = node.get_offer(address);

				std::cout << "  You receive:   ";
				if(auto token = get_token(node, data.bid_currency)) {
					std::cout << mmx::to_value(data.bid_amount, token->decimals) << " " << token->symbol << " [" << data.bid_currency << "]";
				} else {
					std::cout << data.bid_amount << " [" << data.bid_currency << "]";
				}
				std::cout << std::endl;

				std::cout << "  They ask for: ";
				if(auto token = get_token(node, data.ask_currency)) {
					std::cout << mmx::to_value(data.ask_amount, token->decimals) << " " << token->symbol << " [" << data.ask_currency << "]";
				} else {
					std::cout << data.ask_amount << " [" << data.ask_currency << "]";
				}
				std::cout << std::endl;

				if(pre_accept || accept_prompt()) {
					if(wallet.is_locked(index)) {
						spend_options.passphrase = vnx::input_password("Passphrase: ");
					}
					auto tx = wallet.accept_offer(index, address, offset, spend_options);
					std::cout << "Transaction ID: " << tx->id << std::endl;
				}
			}
			else if(command == "cancel")
			{
				mmx::addr_t address;
				vnx::read_config("$3", address);

				auto offer = std::dynamic_pointer_cast<const mmx::contract::Executable>(node.get_contract(address));
				if(!offer || offer->binary != params->offer_binary) {
					vnx::log_error() << "No such offer: " << address; goto failed;
				}
				spend_options.user = mmx::vm::to_addr(node.read_storage_field(address, "owner").first.get());

				if(wallet.is_locked(index)) {
					spend_options.passphrase = vnx::input_password("Passphrase: ");
				}
				auto tx = wallet.execute(index, address, "cancel", {}, spend_options);
				std::cout << "Transaction ID: " << tx->id << std::endl;
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
				std::cerr << "Help: mmx wallet [show | get | log | send | send_from | transfer | offer | accept | cancel | mint | deploy | mutate | exec | create | accounts | keys | lock | unlock]" << std::endl;
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
				std::cout << "Netspace:   " << info->total_space / pow(1000, 5) << " PB" << std::endl;
				std::cout << "VDF Speed:  " << info->time_diff * params->time_diff_constant / params->block_time / 1e6 << " MH/s" << std::endl;
				std::cout << "Reward:     " << info->block_reward / pow(10, params->decimals) << " MMX" << std::endl;
				std::cout << "Supply:     " << info->total_supply / pow(10, params->decimals) << " MMX" << std::endl;
				std::cout << "Block size: " << info->block_size * 100 << " %" << std::endl;
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
					const auto recv = (peer.bytes_recv * 1000) / peer.connect_time_ms;
					const auto send = (peer.bytes_send * 1000) / peer.connect_time_ms;
					std::cout << ", " << recv / 1024 << "." << (recv * 10 / 1024) % 10 << " KB/s recv";
					std::cout << ", " << send / 1024 << "." << (send * 10 / 1024) % 10 << " KB/s send";
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
				for(const auto& in : tx->get_all_inputs()) {
					std::cout << "Input[" << i++ << "]: ";
					if(auto token = get_token(node, in.contract, false)) {
						std::cout << in.amount / pow(10, token->decimals) << " " << token->symbol << " (" << in.amount << ") <- " << in.address << std::endl;
					} else {
						std::cout << in.amount << " [" << in.contract << "]" << std::endl;
					}
				}
				i = 0;
				for(const auto& out : tx->get_all_outputs()) {
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
						std::cout << "  \"" << entry.first << "\": " << entry.second.to_string() << "," << std::endl;
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
									std::cout << entry.to_string();
								}
								std::cout << ']' << std::endl;
								break;
							}
							case mmx::vm::TYPE_MAP:
								std::cout << '{' << std::endl;
								for(const auto& entry : node.read_storage_map(contract, addr)) {
									std::cout << "  " << entry.first.to_string() << ": " << entry.second.to_string() << ',' << std::endl;
								}
								std::cout << '}' << std::endl;
								break;
							default:
								std::cout << var.to_string() << std::endl;
						}
					} else {
						std::cout << var.to_string() << std::endl;
					}
				}
			}
			else if(command == "dump")
			{
				for(const auto& entry : node.dump_storage(contract)) {
					std::cout << "[0x" << std::hex << entry.first << std::dec << "] " << entry.second.to_string() << std::endl;
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
					std::map<size_t, mmx::contract::method_t> method_table;
					for(const auto& entry : bin->methods) {
						method_table[entry.second.entry_point] = entry.second;
					}
					std::vector<mmx::vm::instr_t> code;
					const auto length = mmx::vm::deserialize(code, bin->binary.data(), bin->binary.size());
					size_t i = 0;
					if(!method.empty()) {
						auto iter = bin->methods.find(method);
						if(iter == bin->methods.end()) {
							vnx::log_error() << "No such method: " << method;
							goto failed;
						}
						i = iter->second.entry_point;
					}
					for(; i < code.size(); ++i) {
						auto iter = method_table.find(i);
						if(iter != method_table.end()) {
							std::cout << iter->second.name << " (";
							int k = 0;
							for(const auto& arg : iter->second.args) {
								if(k++) {
									std::cout << ", ";
								}
								std::cout << arg;
							}
							std::cout << ")" << std::endl;
						}
						std::cout << "  [0x" << vnx::to_hex_string(i) << "] " << mmx::vm::to_string(code[i]) << std::endl;

						if(!method.empty() && code[i].code == mmx::vm::OP_RET) {
							break;
						}
					}
					if(method.empty()) {
						std::cout << "Total size: " << length << " bytes" << std::endl;
						std::cout << "Total instructions: " << code.size() << std::endl;
					}
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
						vnx::log_error() << "Missing peer argument: node fetch [block | header] <peer> [height]";
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
					if(data.state == "REVOKED") {
						if(!filter.empty() && filter != "revoked") {
							continue;
						}
						std::cout << "REVOKED";
					} else if(data.state == "OPEN") {
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
					if(auto token = get_token(node, data.bid_currency)) {
						std::cout << mmx::to_value(data.bid_amount, token->decimals) << " " << token->symbol << " [" << data.bid_currency << "]";
					} else {
						std::cout << data.bid_amount << " [" << data.bid_currency << "]";
					}
					std::cout << std::endl;

					std::cout << "  They ask for: ";
					if(auto token = get_token(node, data.ask_currency)) {
						std::cout << mmx::to_value(data.ask_amount, token->decimals) << " " << token->symbol << " [" << data.ask_currency << "]";
					} else {
						std::cout << data.ask_amount << " [" << data.ask_currency << "]";
					}
					std::cout << std::endl;
				}
			}
			else {
				std::cerr << "Help: mmx node [info | peers | tx | get | call | read | dump | dump_code | fetch | balance | history | offers | sync]" << std::endl;
			}
		}
		else if(module == "farm")
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
			try {
				info = farmer.get_farm_info();
			} catch(...) {
				// ignore
			}
			if(!info) {
				info = harvester.get_farm_info();
			}

			if(command == "info")
			{
				std::cout << "Physical size: " << info->total_bytes / pow(1000, 4) << " TB" << std::endl;
				const auto virtual_bytes = mmx::calc_virtual_plot_size(params, info->total_balance);
				std::cout << "Virtual size:  " << info->total_balance / pow(10, params->decimals) << " MMX ("
						<< virtual_bytes / pow(1000, 4) << " TB)" << std::endl;
				std::cout << "Total size:    " << (info->total_bytes + virtual_bytes) / pow(1000, 4) << " TB" << std::endl;
				for(const auto& entry : info->plot_count) {
					std::cout << "K" << int(entry.first) << ": " << entry.second << " plots" << std::endl;
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
			else {
				std::cerr << "Help: mmx farm [info | get | reload]" << std::endl;
			}
		}
		/*else if(module == "exch") {
			std::string node_url = ":11331";
			vnx::read_config("node", node_url);

			vnx::Handle<vnx::Proxy> proxy = new vnx::Proxy("Proxy", vnx::Endpoint::from_url(node_url));
			proxy->forward_list = {"ExchClient", "Node"};
			proxy.start_detached();
			try {
				params = node.get_params();
			} catch(...) {
				// ignore
			}
			mmx::exchange::ClientClient client("ExchClient");

			const auto servers = client.get_servers();

			std::string server;
			if(size_t(server_index) < servers.size()) {
				server = servers[server_index];
			}

			if(command == "offer")
			{
				mmx::exchange::trade_pair_t pair;
				pair.ask = contract;
				vnx::read_config("$3", pair.bid);

				if(pair.bid == pair.ask) {
					vnx::log_error() << "Invalid trade!";
					goto failed;
				}
				const auto bid_token = get_token(node, pair.bid);
				const auto ask_token = get_token(node, pair.ask);
				const auto bid_symbol = bid_token->symbol;
				const auto ask_symbol = ask_token->symbol;
				const auto bid_factor = pow(10, bid_token->decimals);
				const auto ask_factor = pow(10, ask_token->decimals);

				const uint64_t ask = amount * ask_factor;
				const uint64_t bid = bid_amount * bid_factor;
				if(bid == 0 || ask == 0) {
					vnx::log_error() << "Invalid amount! (-a | -b)";
					goto failed;
				}
				auto offer = client.make_offer(index, pair, bid, ask);
				if(!offer) {
					vnx::log_error() << "Unable to make an offer! (insufficient funds, or not split down enough)";
					goto failed;
				}
				std::cout << "ID: " << offer->id << std::endl;
				std::cout << "Bids: ";
				int i = 0;
				for(const auto& entry : offer->orders) {
					std::cout << (i++ ? " + " : "") << entry.second.bid.amount / bid_factor;
				}
				std::cout << std::endl;
				std::cout << "Total Bid: " << offer->bid / bid_factor << " (" << offer->bid << ") " << bid_symbol << std::endl;
				std::cout << "Total Ask: " << offer->ask / ask_factor << " (" << offer->ask << ") " << ask_symbol << std::endl;
				std::cout << "Price: " << ask / double(bid) << " " << ask_symbol << " / " << bid_symbol << std::endl;
				if(pre_accept) {
					client.place(offer);
				} else {
					std::cout << "Accept (y): ";
					std::string input;
					std::getline(std::cin, input);
					if(input == "y") {
						client.place(offer);
					} else {
						std::cout << "Aborted" << std::endl;
					}
				}
			}
			else if(command == "trade")
			{
				mmx::exchange::trade_pair_t pair;
				pair.bid = contract;
				vnx::read_config("$3", pair.ask);

				if(pair.bid == pair.ask) {
					vnx::log_error() << "Invalid trade!";
					goto failed;
				}
				const auto bid_token = get_token(node, pair.bid);
				const auto ask_token = get_token(node, pair.ask);
				const auto bid_symbol = bid_token->symbol;
				const auto ask_symbol = ask_token->symbol;
				const auto bid_factor = pow(10, bid_token->decimals);
				const auto ask_factor = pow(10, ask_token->decimals);

				const uint64_t ask = amount * ask_factor;
				const uint64_t bid = bid_amount * bid_factor;
				if(bid == 0) {
					vnx::log_error() << "Invalid bid amount! (-b)";
					goto failed;
				}
				vnx::optional<uint64_t> trade_ask;
				if(ask > 0) {
					trade_ask = ask;
				}
				auto trade = client.make_trade(index, pair, bid, trade_ask);
				std::cout << "Server: " << server << std::endl;
				std::cout << "Total Bid: " << trade.bid / bid_factor << " (" << trade.bid << ") " << bid_symbol << std::endl;
				if(trade.ask) {
					std::cout << "Total Ask: " << *trade.ask / ask_factor << " (" << *trade.ask << ") " << ask_symbol << std::endl;
					std::cout << "Price: " << *trade.ask / double(trade.bid) << " " << ask_symbol << " / " << bid_symbol << std::endl;
				}
				bool accepted = pre_accept;
				if(!accepted) {
					std::cout << "Accept (y): ";
					std::string input;
					std::getline(std::cin, input);
					if(input == "y") {
						accepted = true;
					} else {
						std::cout << "Aborted" << std::endl;
					}
				}
				if(accepted) {
					const auto matched = client.match(server, trade);
					std::cout << "Matched: " << matched.ask / ask_factor << " " << ask_symbol << " for " << matched.bid / bid_factor
							<< " " << bid_symbol << " [" << double(matched.ask) / matched.bid << " " << ask_symbol << " / " << bid_symbol << "]" << std::endl;

					bool accepted = pre_accept;
					if(!accepted) {
						std::cout << "Accept (y): ";
						std::string input;
						std::getline(std::cin, input);
						if(input == "y") {
							accepted = true;
						} else {
							std::cout << "Aborted" << std::endl;
						}
					}
					if(accepted) {
						try {
							const auto id = client.execute(server, index, matched);
							std::cout << "Trade: " << id << std::endl;
						} catch(std::exception& ex) {
							std::cout << "Trade failed with: " << ex.what() << std::endl;
						}
					}
				}
			}
			else if(command == "offers")
			{
				for(const auto offer : client.get_all_offers())
				{
					const auto bid_token = get_token(node, offer->pair.bid);
					const auto ask_token = get_token(node, offer->pair.ask);
					const auto bid_symbol = bid_token->symbol;
					const auto ask_symbol = ask_token->symbol;
					const auto bid_factor = pow(10, bid_token->decimals);
					const auto ask_factor = pow(10, ask_token->decimals);
					std::cout << "[" << offer->id << "] Offering " << offer->bid / bid_factor << " " << bid_symbol
							<< " for " << offer->ask / ask_factor << " " << ask_symbol
							<< " [" << (offer->ask / double(offer->bid)) << " " << ask_symbol << " / " << bid_symbol
							<< "] (" << 100 * (offer->bid_sold / double(offer->bid)) << " % executed)" << std::endl;
				}
			}
			else if(command == "orders")
			{
				mmx::exchange::trade_pair_t pair;
				pair.ask = contract;
				vnx::read_config("$3", pair.bid);

				const auto bid_token = get_token(node, pair.bid);
				const auto ask_token = get_token(node, pair.ask);
				const auto bid_symbol = bid_token->symbol;
				const auto ask_symbol = ask_token->symbol;
				const auto bid_factor = pow(10, bid_token->decimals);

				std::cout << "Server: " << server << std::endl;
				std::cout << "Pair: " << ask_symbol << " / " << bid_symbol << std::endl;
				auto sell_orders = client.get_orders(server, pair);
				std::reverse(sell_orders.begin(), sell_orders.end());
				for(const auto& order : sell_orders) {
					std::cout << "Sell: " << order.ask / double(order.bid) << " " << ask_symbol << " / " << bid_symbol
							<< " => " << order.bid / bid_factor << " " << bid_symbol << std::endl;
				}
				auto buy_orders = client.get_orders(server, pair.reverse());
				std::reverse(buy_orders.begin(), buy_orders.end());
				for(const auto& order : buy_orders) {
					std::cout << "Buy:  " << order.bid / double(order.ask) << " " << ask_symbol << " / " << bid_symbol
							<< " => " << order.ask / bid_factor << " " << bid_symbol << std::endl;
				}
			}
			else if(command == "price")
			{
				mmx::exchange::trade_pair_t pair;
				pair.bid = contract;
				vnx::read_config("$3", pair.ask);

				const auto bid_token = get_token(node, pair.bid);
				const auto ask_token = get_token(node, pair.ask);

				mmx::exchange::amount_t have;
				have.amount = (amount > 0 ? amount : 1) * pow(10, bid_token->decimals);
				have.currency = pair.bid;
				const auto price = client.get_price(server, pair.ask, have);
				if(price.value > 0) {
					std::cout << price.inverse / double(price.value) << " " << bid_token->symbol << " / " << ask_token->symbol << std::endl;
					std::cout << price.value / double(price.inverse) << " " << ask_token->symbol << " / " << bid_token->symbol << std::endl;
					std::cout << "Amount:  " << price.inverse / pow(10, bid_token->decimals) << " " << bid_token->symbol << std::endl;
					std::cout << "Receive: " << price.value / pow(10, ask_token->decimals) << " " << ask_token->symbol << std::endl;
				} else {
					std::cout << "No orders matched!" << std::endl;
				}
			}
			else if(command == "cancel")
			{
				std::string offer;
				vnx::read_config("$3", offer);

				if(offer == "all") {
					client.cancel_all();
					std::cout << "Canceled all offers." << std::endl;
				} else {
					const auto id = vnx::from_string<int64_t>(offer);
					if(client.get_offer(id)) {
						client.cancel_offer(id);
						std::cout << "Canceled offer " << id << std::endl;
					} else {
						vnx::log_error() << "No such offer: " << id;
						goto failed;
					}
				}
			}
			else if(command == "servers")
			{
				const auto servers = client.get_servers();
				for(const auto& name : servers) {
					std::cout << "[" << name << "]" << std::endl;
				}
			}
			else if(command == "log")
			{
				auto history = client.get_local_history();
				std::reverse(history.begin(), history.end());
				for(const auto& entry : history) {
					auto bid_token = get_token(node, entry->pair.bid);
					auto ask_token = get_token(node, entry->pair.ask);
					std::cout << "[" << (entry->height ? std::to_string(*entry->height) : (entry->failed ? "failed" : "pending")) << "] "
							<< (entry->offer_id ? "SOLD  " : "TRADE ")
							<< entry->bid / pow(10, bid_token->decimals) << " [" << bid_token->symbol << "] FOR "
							<< entry->ask / pow(10, ask_token->decimals) << " [" << ask_token->symbol << "]";
					if(entry->offer_id) {
						std::cout << " (ID " << *entry->offer_id << ")";
					}
					if(entry->message) {
						std::cout << " (" << *entry->message << ")";
					}
					std::cout << " (" << entry->id << ")" << std::endl;
				}
			}
			else {
				std::cerr << "Help: mmx exch [offer | trade | offers | orders | log | price | servers | cancel]" << std::endl;
			}
		}*/
		else {
			std::cerr << "Help: mmx [node | wallet | farm | exch]" << std::endl;
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


