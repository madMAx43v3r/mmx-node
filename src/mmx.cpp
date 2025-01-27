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
#include <mmx/contract/TokenBase.hxx>
#include <mmx/contract/Executable.hxx>
#include <mmx/operation/Execute.hxx>
#include <mmx/operation/Deposit.hxx>
#include <mmx/KeyFile.hxx>
#include <mmx/secp256k1.hpp>
#include <mmx/hash_t.hpp>
#include <mmx/fixed128.hpp>
#include <mmx/mnemonic.h>
#include <mmx/utils.h>
#include <mmx/vm/instr_t.h>
#include <mmx/vm_interface.h>
#include <mmx/ECDSA_Wallet.h>

#include <vnx/vnx.h>
#include <vnx/Proxy.h>
#include <vnx/ProxyClient.hxx>
#include <vnx/addons/HttpClient.h>
#include <vnx/addons/HttpClientClient.hxx>

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

void show_history(std::vector<mmx::tx_entry_t> history, mmx::NodeClient& node, std::shared_ptr<const mmx::ChainParams> params)
{
	std::reverse(history.begin(), history.end());

	for(const auto& entry : history) {
		if(entry.is_pending) {
			std::cout << "[pending] ";
		} else {
			std::cout << "[" << entry.height << "] ";
		}
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
		const auto currency = entry.contract;
		const auto contract = get_contract(node, currency);
		if(auto exe = std::dynamic_pointer_cast<const mmx::contract::Executable>(contract)) {
			if(exe->binary == params->nft_binary) {
				std::cout << currency << " " << arrow << " " << entry.address << " (" << entry.txid << ")" << std::endl;
				continue;
			}
		}
		const auto token = std::dynamic_pointer_cast<const mmx::contract::TokenBase>(contract);
		const auto decimals = token ? token->decimals : currency == mmx::addr_t() ? params->decimals : 0;
		const auto symbol = token ? token->symbol : currency == mmx::addr_t() ? std::string("MMX") : std::string("???");
		std::cout << mmx::to_value(entry.amount, decimals) << " " << symbol
				<< " (" << entry.amount << ") " << arrow << " " << entry.address << " TX(" << entry.txid << ")";
		if(entry.memo) {
			std::cout << " M(" << *entry.memo << ")";
		}
		std::cout << std::endl;
	}
}

void print_tx(std::shared_ptr<const mmx::Transaction> tx, mmx::NodeClient& node, std::shared_ptr<const mmx::ChainParams> params)
{
	std::cout << "TX ID: " << tx->id << std::endl;
	std::cout << "Note: " << vnx::to_string_value(tx->note) << std::endl;
	std::cout << "Network: " << tx->network << std::endl;
	std::cout << "Expires: at height " << tx->expires << std::endl;
	std::cout << "Fee Ratio: " << tx->fee_ratio / 1024. << std::endl;
	std::cout << "Max Fee Amount: " << mmx::to_value(tx->max_fee_amount, params) << " MMX" << std::endl;
	std::cout << "Sender: " << vnx::to_string_value(tx->sender) << std::endl;
	{
		int i = 0;
		for(const auto& in : tx->inputs) {
			const auto token = get_token(node, in.contract);
			const auto decimals = token ? token->decimals : in.contract == mmx::addr_t() ? params->decimals : 0;
			const auto symbol = token ? token->symbol : in.contract == mmx::addr_t() ? std::string("MMX") : std::string("???");
			std::cout << "Input[" << i++ << "]: " << mmx::to_value(in.amount, decimals) << " " << symbol << " [" << in.contract << "] from " << in.address << std::endl;
			if(in.memo) {
				std::cout << "  Memo: " << vnx::to_string(in.memo) << std::endl;
			}
		}
	}
	{
		int i = 0;
		for(const auto& out : tx->outputs) {
			const auto token = get_token(node, out.contract);
			const auto decimals = token ? token->decimals : out.contract == mmx::addr_t() ? params->decimals : 0;
			const auto symbol = token ? token->symbol : out.contract == mmx::addr_t() ? std::string("MMX") : std::string("???");
			std::cout << "Output[" << i++ << "]: " << mmx::to_value(out.amount, decimals) << " " << symbol << " [" << out.contract << "] to " << out.address << std::endl;
			if(out.memo) {
				std::cout << "  Memo: " << vnx::to_string(out.memo) << std::endl;
			}
		}
	}
	{
		int i = 0;
		for(const auto& op : tx->execute) {
			std::cout << "Operation[" << i++ << "]: ";
			const auto exec = std::dynamic_pointer_cast<const mmx::operation::Execute>(op);
			const auto deposit = std::dynamic_pointer_cast<const mmx::operation::Deposit>(op);
			if(deposit) {
				std::cout << "Deposit via " << deposit->method << "()" << std::endl;
			} else if(exec) {
				std::cout << "Execute " << exec->method << "()" << std::endl;
			} else {
				std::cout << "null" << std::endl;
				continue;
			}
			if(deposit) {
				const auto token = get_token(node, deposit->currency);
				const auto decimals = token ? token->decimals : deposit->currency == mmx::addr_t() ? params->decimals : 0;
				const auto symbol = token ? token->symbol : deposit->currency == mmx::addr_t() ? std::string("MMX") : std::string("???");
				std::cout << "  Amount: " << mmx::to_value(deposit->amount, decimals) << " " << symbol << " [" << deposit->currency << "]" << std::endl;
			}
			std::cout << "  Contract: " << (op->address != mmx::addr_t() ? op->address.to_string() : std::string("<deployed>")) << std::endl;
			std::cout << "  User: " << vnx::to_string(exec->user) << std::endl;
			int k = 0;
			for(const auto& arg : exec->args) {
				std::cout << "  Argument[" << k++ << "]: " << vnx::to_string(arg) << std::endl;
			}
		}
	}
	if(auto contract = tx->deploy) {
		std::cout << "Deploy: (see below)" << std::endl;
		vnx::PrettyPrinter print(std::cout);
		vnx::accept(print, contract);
		std::cout << std::endl;
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
	} else if(auto path = ::getenv("HOME")) {
		mmx_home = path;
		mmx_home += "/.mmx/";
	}

	std::map<std::string, std::string> options;
	options["n"] = "node";
	options["f"] = "file";
	options["j"] = "index";
	options["k"] = "offset";
	options["a"] = "amount";
	options["b"] = "ask-amount";
	options["m"] = "memo";
	options["s"] = "source";
	options["t"] = "target";
	options["x"] = "contract";
	options["z"] = "ask-currency";
	options["y"] = "yes";
	options["r"] = "fee-ratio";
	options["l"] = "gas-limit";
	options["N"] = "limit";
	options["v"] = "verbose";
	options["o"] = "output";
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
	options["output"] = "file name";
	options["nonce"] = "value > 0";

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
	vnx::optional<std::string> memo;
	vnx::optional<std::string> output;
	vnx::optional<uint64_t> nonce;
	int64_t index = 0;
	int64_t offset = 0;
	int32_t limit = -1;
	mmx::fixed128 amount;
	mmx::fixed128 ask_amount;
	double fee_ratio = 1;
	double gas_limit = 5;
	double min_amount = 0.95;
	int verbose = 0;
	bool pre_accept = false;
	vnx::read_config("$1", module);
	vnx::read_config("$2", command);
	vnx::read_config("yes", pre_accept);
	vnx::read_config("file", file_name);
	vnx::read_config("index", index);
	vnx::read_config("offset", offset);
	const auto have_amount = vnx::read_config("amount", amount);
	const auto have_ask_amount = vnx::read_config("ask-amount", ask_amount);
	vnx::read_config("source", source_addr);
	vnx::read_config("target", target_addr);
	vnx::read_config("contract", contract_addr);
	vnx::read_config("ask-currency", ask_currency_addr);
	vnx::read_config("fee-ratio", fee_ratio);
	vnx::read_config("gas-limit", gas_limit);
	vnx::read_config("min-amount", min_amount);
	vnx::read_config("user", user);
	vnx::read_config("passwd", passwd);
	vnx::read_config("memo", memo);
	vnx::read_config("limit", limit);
	vnx::read_config("verbose", verbose);
	vnx::read_config("output", output);
	vnx::read_config("nonce", nonce);

	vnx::write_config("Proxy.no_retry", true);		// exit on connect fail

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
	spend_options.gas_limit = mmx::to_amount(gas_limit, params);
	spend_options.memo = memo;
	spend_options.nonce = nonce;
	if(output) {
		spend_options.auto_send = false;
		spend_options.expire_at = -1;
	}

	mmx::NodeClient node("Node");
	{
		vnx::Handle<vnx::addons::HttpClient> module = new vnx::addons::HttpClient("HttpClient");
		module->connection_timeout_ms = 5000;
		module.start_detached();
	}
	vnx::addons::HttpClientClient http("HttpClient");

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

			std::shared_ptr<const mmx::Transaction> tx;

			if(command != "create") {
				vnx::Handle<vnx::Proxy> proxy = new vnx::Proxy("Proxy", vnx::Endpoint::from_url(node_url));
				proxy->forward_list = {"Wallet", "Node"};
				proxy.start_detached();
				try {
					params = node.get_params();
				} catch(...) {
					// ignore
				}
				if(index == 0) {
					const auto accounts = wallet.get_all_accounts();
					if(!accounts.empty()) {
						index = accounts[0].account;
					}
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
						if(auto exec = std::dynamic_pointer_cast<const mmx::contract::Executable>(contract)) {
							if(exec->binary == params->nft_binary) {
								nfts.push_back(entry.first);
								continue;
							}
						}
						if(auto token = std::dynamic_pointer_cast<const mmx::contract::TokenBase>(contract)) {
							std::cout << "Balance: " << mmx::to_value(balance.total, token->decimals)
									<< " " << token->symbol << (balance.is_validated ? "" : "?")
									<< " (Spendable: " << mmx::to_value(balance.spendable, token->decimals)
									<< " " << token->symbol << (balance.is_validated ? "" : "?") << ")";
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
						std::cout << ")" << std::endl;

						for(const auto& entry : wallet.get_total_balances({address}))
						{
							const auto& balance = entry.second;
							if(auto token = get_token(node, entry.first, false)) {
								std::cout << "  Balance: " << to_value(balance.total, token->decimals) << " " << token->symbol
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
					std::cout << "[" << entry.account << "] name = '" << entry.name << "', index = " << entry.index
							<< ", passphrase = " << (entry.with_passphrase ? "yes" : "no")
							<< ", finger_print = " << entry.finger_print
							<< ", num_addresses = " << entry.num_addresses << ", key_file = " << entry.key_file
							<< " (" << vnx::to_string_value(entry.address) << ")" << std::endl;
				}
			}
			else if(command == "keys")
			{
				const auto keys = wallet.get_farmer_keys(index);
				std::cout << "Farmer Public Key: " << keys.second << std::endl;
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
					std::cout << mmx::to_value(wallet.get_balance(index, contract).total, token ? token->decimals : params->decimals) << std::endl;
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
				tx = wallet.send(index, mojo, target, contract, spend_options);
				std::cout << "Sent " << mmx::to_value(mojo, token->decimals) << " " << token->symbol << " (" << mojo << ") to " << target << std::endl;
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
				tx = wallet.send_from(index, mojo, target, source, contract, spend_options);
				std::cout << "Sent " << mmx::to_value(mojo, token->decimals) << " " << token->symbol << " (" << mojo << ") to " << target << std::endl;
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
				tx = wallet.send(index, 1, target, contract, spend_options);
				std::cout << "Sent " << contract << " to " << target << std::endl;
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
				tx = wallet.execute(index, contract, "mint_to",
						{vnx::Variant(target.to_string()), mojo.to_var_arg(), vnx::Variant(memo)}, nullptr, spend_options);
				std::cout << "Minted " << mmx::to_value(mojo, token->decimals) << " (" << mojo << ") " << token->symbol << " to " << target << std::endl;
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
				tx = wallet.deploy(index, payload, spend_options);
				std::cout << "Deployed " << payload->get_type_name() << " as [" << mmx::addr_t(tx->id) << "]" << std::endl;
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
				if(auto exec = std::dynamic_pointer_cast<const mmx::contract::Executable>(node.get_contract(contract))) {
					if(auto bin = std::dynamic_pointer_cast<const mmx::contract::Binary>(node.get_contract(exec->binary))) {
						if(auto meth = bin->find_method(method)) {
							args.resize(meth->args.size());
						}
					}
				}
				if(!spend_options.user && offset >= 0) {
					spend_options.user = wallet.get_address(index, offset);
				}
				if(wallet.is_locked(index)) {
					spend_options.passphrase = vnx::input_password("Passphrase: ");
				}
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

					tx = wallet.make_offer(index, offset, bid_value, bid, ask_value, ask, spend_options);
					if(tx) {
						std::cout << "Contract: " << mmx::addr_t(tx->id) << std::endl;
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
					tx = wallet.cancel_offer(index, address, spend_options);
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
					tx = wallet.offer_withdraw(index, address, spend_options);
				}
				else {
					std::cerr << "mmx wallet offer [cancel | withdraw]" << std::endl;
				}
			}
			else if(command == "trade")
			{
				mmx::addr_t address;
				vnx::read_config("$3", address);

				const auto height = node.get_height();
				const auto data = node.get_offer(address);
				const auto bid_token = get_token(node, data.bid_currency);
				const auto ask_token = get_token(node, data.ask_currency);
				const uint64_t ask_amount = mmx::to_amount(amount, ask_token->decimals);
				const uint64_t bid_amount = data.get_bid_amount(ask_amount);

				std::cout << "You send:    "
						<< mmx::to_value(ask_amount, ask_token->decimals) << " " << ask_token->symbol << " [" << data.ask_currency << "]" << std::endl;
				std::cout << "You receive: "
						<< mmx::to_value(bid_amount, bid_token->decimals) << " " << bid_token->symbol << " [" << data.bid_currency << "]" << std::endl;

				if(data.last_update > height || height - data.last_update < 100) {
					pre_accept = false;
					std::cout << "WARNING: Offer price has changed recently!" << std::endl;
				}
				if(pre_accept || accept_prompt()) {
					if(wallet.is_locked(index)) {
						spend_options.passphrase = vnx::input_password("Passphrase: ");
					}
					tx = wallet.offer_trade(index, address, ask_amount, offset, data.inv_price, spend_options);
				}
			}
			else if(command == "accept")
			{
				mmx::addr_t address;
				vnx::read_config("$3", address);

				const auto height = node.get_height();
				const auto data = node.get_offer(address);
				const auto bid_token = get_token(node, data.bid_currency);
				const auto ask_token = get_token(node, data.ask_currency);

				std::cout << "You send:    "
						<< mmx::to_value(data.ask_amount, ask_token->decimals) << " " << ask_token->symbol << " [" << data.ask_currency << "]" << std::endl;
				std::cout << "You receive: "
						<< mmx::to_value(data.bid_balance, bid_token->decimals) << " " << bid_token->symbol << " [" << data.bid_currency << "]" << std::endl;

				if(data.last_update > height || height - data.last_update < 100) {
					pre_accept = false;
					std::cout << "WARNING: Offer price has changed recently!" << std::endl;
				}
				if(pre_accept || accept_prompt()) {
					if(wallet.is_locked(index)) {
						spend_options.passphrase = vnx::input_password("Passphrase: ");
					}
					tx = wallet.accept_offer(index, address, offset, data.inv_price, spend_options);
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

				const mmx::uint128 min_trade_amount = mmx::to_amount(expected_amount.to_double() * min_amount, 0);

				std::cout << "You send: " << mmx::to_value(deposit_amount, token_i->decimals) << " " << token_i->symbol << std::endl;
				std::cout << "You receive at least:  " << mmx::to_value(min_trade_amount, token_k->decimals) << " " << token_k->symbol << std::endl;
				std::cout << "You expect to receive: " << mmx::to_value(expected_amount, token_k->decimals) << " " << token_k->symbol << " (estimated)" << std::endl;
				std::cout << "Fee:   " << mmx::to_value(trade_estimate[1], token_k->decimals) << " " << token_k->symbol << std::endl;
				std::cout << "Price: "
						<< (command == "sell" ? expected_amount.to_double() / deposit_amount.to_double() : deposit_amount.to_double() / expected_amount.to_double()) << " "
						<< (command == "sell" ? token_k : token_i)->symbol << " / " << (command == "sell" ? token_i : token_k)->symbol << std::endl;

				if(pre_accept || accept_prompt()) {
					if(wallet.is_locked(index)) {
						spend_options.passphrase = vnx::input_password("Passphrase: ");
					}
					tx = wallet.swap_trade(index, contract, deposit_amount, info.tokens[i], min_trade_amount, 20, spend_options);
				}
			}
			else if(command == "swap")
			{
				std::string action;
				vnx::read_config("$3", action);

				if(action == "add")
				{
					const auto usage = "mmx wallet swap add -a <amount> -b <amount> -x <contract>";
					std::array<mmx::uint128, 2> add_amount = {};
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
						tx = wallet.swap_add_liquid(index, contract, add_amount, offset, spend_options);
					}
				}
				else if(action == "remove")
				{
					std::array<mmx::uint128, 2> rem_amount = {};
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
						rem_amount[i] = std::min<uint128_t>(rem_amount[i], user_info.balance[i]);
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
						tx = wallet.swap_rem_liquid(index, contract, rem_amount, spend_options);
					}
				}
				else if(action == "payout")
				{
					if(wallet.is_locked(index)) {
						spend_options.passphrase = vnx::input_password("Passphrase: ");
					}
					spend_options.user = wallet.get_address(index, offset);

					tx = wallet.execute(index, contract, "payout", {}, nullptr, spend_options);
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
				mmx::query_filter_t filter;
				filter.limit = limit;
				filter.with_pending = true;
				vnx::read_config("$3", filter.since);

				show_history(wallet.get_history(index, filter), node, params);
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
				bool with_passphrase = false;
				vnx::read_config("$3", seed_str);
				vnx::read_config("mnemonic", seed_words);
				vnx::read_config("with-passphrase", with_passphrase);

				mmx::KeyFile key;
				if(seed_str.empty()) {
					if(seed_words.empty()) {
						key.seed_value = mmx::hash_t::secure_random();
					} else {
						key.seed_value = mmx::mnemonic::words_to_seed(seed_words);
					}
				} else if(seed_str.size() == 64) {
					key.seed_value.from_string(seed_str);
				} else {
					vnx::log_error() << "Invalid seed value: '" << seed_str << "'";
					goto failed;
				}

				vnx::optional<std::string> passphrase;
				if(with_passphrase) {
					passphrase = vnx::input_password("Passphrase: ");
				}
				key.finger_print = get_finger_print(key.seed_value, passphrase);

				vnx::write_to_file(file_name, key);
				std::filesystem::permissions(file_name, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);

				std::cout << "Created wallet '" << file_name << "'" << std::endl;
				std::cout << "  Mnemonic: " << mmx::mnemonic::words_to_string(mmx::mnemonic::seed_to_words(key.seed_value)) << std::endl;

				mmx::account_t config;
				config.num_addresses = 1;
				config.finger_print = *key.finger_print;
				config.with_passphrase = with_passphrase;

				mmx::ECDSA_Wallet wallet(key.seed_value, config, params);
				wallet.unlock(passphrase ? *passphrase : std::string());
				std::cout << "  Address: " << wallet.get_address(0) << std::endl;
			}
			else if(command == "import")
			{
				vnx::read_config("$3", file_name);
				if(file_name.empty()) {
					std::cerr << "Usage: mmx wallet import path/to/wallet*.dat" << std::endl;
					goto failed;
				}
				auto key_file = vnx::read_from_file<mmx::KeyFile>(file_name);
				if(!key_file) {
					vnx::log_error() << "Failed to read wallet file '" << file_name << "'";
					goto failed;
				}
				vnx::optional<std::string> passphrase;
				if(key_file->finger_print) {
					passphrase = vnx::input_password("Passphrase: ");
				}
				mmx::account_t config;
				config.num_addresses = 0;	// use default
				wallet.import_wallet(config, key_file, passphrase);
			}
			else if(command == "remove")
			{
				if(index < 100) {
					vnx::log_error() << "Wallet removal not supported for indices below 100!";
					goto failed;
				}
				wallet.remove_account(index, offset);
			}
			else if(command == "plotnft")
			{
				vnx::read_config("$3", command);
				try {
					if(command != "create") {
						index = wallet.find_wallet_by_addr(contract);
					}
				} catch(...) {
					// ignore
				}
				if(command == "join") {
					std::string url;
					vnx::read_config("$4", url);
					if(url.size() && !contract.is_zero()) {
						const auto info = http.get_json(url + "/pool/info", {}).to_object();
						std::cout << "[" << url << "]" << std::endl;
						std::cout << "Name: " << info["name"].to_string_value() << std::endl;
						std::cout << "Description: " << info["description"].to_string_value() << std::endl;
						std::cout << "Pool Fee: " << info["fee"].to<double>() * 100 << " %" << std::endl;
						std::cout << "Minimum difficulty: " << info["min_difficulty"] << std::endl;
						const auto target = info["pool_target"].to<mmx::addr_t>();
						if(target.is_zero()) {
							throw std::logic_error("pool returned invalid target address");
						}
						if(wallet.is_locked(index)) {
							spend_options.passphrase = vnx::input_password("Passphrase: ");
						}
						if(pre_accept || accept_prompt()) {
							tx = wallet.plotnft_exec(contract, "lock", {vnx::Variant(target.to_string()), vnx::Variant(url)}, spend_options);
						}
					} else {
						std::cerr << "Usage: mmx wallet plotnft join <pool_url> -x <plot_nft_address>" << std::endl;
						goto failed;
					}
				}
				else if(command == "lock") {
					std::string target;
					vnx::read_config("$4", target);
					if(target.size() && !contract.is_zero()) {
						if(wallet.is_locked(index)) {
							spend_options.passphrase = vnx::input_password("Passphrase: ");
						}
						tx = wallet.plotnft_exec(contract, "lock", {vnx::Variant(target), vnx::Variant()}, spend_options);
					} else {
						std::cerr << "Usage: mmx wallet plotnft lock <target_address> -x <plot_nft_address>" << std::endl;
						goto failed;
					}
				}
				else if(command == "unlock") {
					if(contract.is_zero()) {
						std::cerr << "Usage: mmx wallet plotnft unlock -x <plot_nft_address>" << std::endl;
						goto failed;
					}
					if(wallet.is_locked(index)) {
						spend_options.passphrase = vnx::input_password("Passphrase: ");
					}
					tx = wallet.plotnft_exec(contract, "unlock", {}, spend_options);
				}
				else if(command == "create") {
					std::string name;
					vnx::read_config("$4", name);
					if(name.size()) {
						if(wallet.is_locked(index)) {
							spend_options.passphrase = vnx::input_password("Passphrase: ");
						}
						tx = wallet.plotnft_create(index, name, offset, spend_options);
					} else {
						std::cerr << "Usage: mmx wallet plotnft create <name>" << std::endl;
						goto failed;
					}
				}
				else if(command == "show") {
					std::map<mmx::addr_t, std::shared_ptr<const mmx::Contract>> list;
					if(contract.is_zero()) {
						list = wallet.get_contracts_owned(index, nullptr, params->plot_nft_binary);
					} else {
						if(auto exec = std::dynamic_pointer_cast<const mmx::contract::Executable>(node.get_contract(contract))) {
							if(exec->binary == params->plot_nft_binary) {
								list[contract] = exec;
							}
						}
					}
					for(const auto& entry : list) {
						std::cout << "[" << entry.first.to_string() << "]" << std::endl;
						if(auto info = node.get_plot_nft_info(entry.first)) {
							std::cout << "  Name: " << info->name << std::endl;
							std::cout << "  Locked: " << vnx::to_string(info->is_locked) << std::endl;
							if(info->is_locked && info->unlock_height) {
								std::cout << "  Unlock Height: " << *info->unlock_height << std::endl;
							}
							std::cout << "  Server URL: " << (info->server_url ? *info->server_url : std::string("N/A (solo farming)")) << std::endl;
							if(verbose) {
								std::cout << "  Pool Target: " << (info->target ? info->target->to_string() : std::string("N/A")) << std::endl;
								std::cout << "  Owner: " << info->owner.to_string() << std::endl;
							}
						}
					}
					if(list.empty()) {
						std::cout << "No Plot NFTs found!" << std::endl;
					}
				}
				else {
					std::cerr << "Usage: mmx wallet plotnft [show | join | lock | unlock | create]" << std::endl;
					goto failed;
				}
			}
			else {
				std::cerr << "Help: mmx wallet [show | get | log | send | send_from | offer | trade | accept | buy | sell | swap | mint | deploy | exec | transfer | create | import | remove | accounts | keys | lock | unlock | plotnft]" << std::endl;
			}

			if(tx) {
				std::cout << "Transaction ID: " << tx->id << std::endl;
			}
			if(output) {
				auto out = vnx::clone(tx);
				out->reset(params);
				vnx::write_to_file(*output, out);
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
					auto contract = get_contract(node, entry.first);
					auto exe = std::dynamic_pointer_cast<const mmx::contract::Executable>(contract);
					if(!exe || exe->binary != params->nft_binary) {
						const auto token = std::dynamic_pointer_cast<const mmx::contract::TokenBase>(contract);
						const auto decimals = token ? token->decimals : params->decimals;
						std::cout << "Balance: " << mmx::to_value(entry.second, decimals) << " " << (token ? token->symbol : "MMX") << " (" << entry.second << ")" << std::endl;
					}
				}
			}
			else if(command == "info")
			{
				const auto info = node.get_network_info();
				std::cout << "Synced:     " << (node.get_synced_height() ? "Yes" : "No") << std::endl;
				std::cout << "Height:     " << info->height << std::endl;
				std::cout << "Netspace:   " << info->total_space / pow(1000, 2) << " PB" << std::endl;
				std::cout << "VDF Speed:  " << info->vdf_speed << " MH/s" << std::endl;
				std::cout << "Reward:     " << mmx::to_value(info->block_reward, params) << " MMX" << std::endl;
				std::cout << "Supply:     " << mmx::to_value(info->total_supply, params) << " MMX" << std::endl;
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
				mmx::query_filter_t filter;
				filter.limit = limit;
				vnx::read_config("$4", filter.since);

				show_history(node.get_history({address}, filter), node, params);
			}
			else if(command == "peers")
			{
				auto info = router.get_peer_info();
				size_t max_length = 0;
				uint32_t max_height = 0;
				for(const auto& peer : info->peers) {
					max_length = std::max(max_length, peer.address.size());
					max_height = std::max(max_height, peer.height);
				}
				for(const auto& peer : info->peers) {
					std::cout << "[" << peer.address << "]";
					for(size_t i = peer.address.size(); i < max_length + 1; ++i) {
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
					std::cout << ", " << peer.bytes_send / 1024 / 1024 << " MB sent (" << float(peer.compression_ratio) << ")";
					std::cout << ", since " << (peer.connect_time_ms / 60000) << " min";
					std::cout << ", " << peer.ping_ms << " ms ping";
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
			else if(command == "send")
			{
				std::string file_name;
				vnx::read_config("$3", file_name);

				if(auto tx = vnx::read_from_file<mmx::Transaction>(file_name)) {
					node.add_transaction(tx, true);
					std::cout << "Transaction ID: " << tx->id.to_string() << std::endl;
				} else {
					std::cout << "Failed to read transaction '" << file_name << "'" << std::endl;
					goto failed;
				}
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
						std::cout << mmx::to_value(in.amount, token->decimals) << " " << token->symbol << " (" << in.amount << ") <- " << in.address << std::endl;
					} else {
						std::cout << in.amount << " [" << in.contract << "]" << std::endl;
					}
				}
				i = 0;
				for(const auto& out : tx->get_outputs()) {
					std::cout << "Output[" << i++ << "]: ";
					if(auto token = get_token(node, out.contract, false)) {
						std::cout << mmx::to_value(out.amount, token->decimals) << " " << token->symbol << " (" << out.amount << ") -> " << out.address << std::endl;
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
					std::cout << mmx::to_value(node.get_balance(address, contract), token->decimals) << std::endl;
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
						const auto height = arg.size() ? vnx::from_string<uint32_t>(arg) : node.get_height();
						block = node.get_block_at(height);
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
						const auto height = arg.size() ? vnx::from_string<uint32_t>(arg) : node.get_height();
						block = node.get_header_at(height);
					}
					vnx::PrettyPrinter printer(std::cout);
					vnx::accept(printer, block);
					std::cout << std::endl;
				}
				else if(subject == "tx")
				{
					mmx::hash_t txid;
					std::string format = "json";
					vnx::read_config("$4", txid);
					vnx::read_config("$5", format);

					const auto tx = node.get_transaction(txid, true);
					if(format == "json") {
						std::cout << vnx::to_pretty_string(tx);
					} else if(format == "compact") {
						std::cout << vnx::to_string(tx) << std::endl;
					} else if(format == "hash") {
						const auto tmp = tx->hash_serialize(false);
						std::cout << vnx::to_hex_string(tmp.data(), tmp.size()) << std::endl;
					} else if(format == "full_hash") {
						const auto tmp = tx->hash_serialize(true);
						std::cout << vnx::to_hex_string(tmp.data(), tmp.size()) << std::endl;
					} else {
						vnx::log_error() << "invalid format: " << format << std::endl;
						goto failed;
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
				vnx::optional<mmx::addr_t> currency;
				vnx::read_config("$3", token);
				vnx::read_config("$4", currency);

				for(const auto& data : node.get_swaps(offset, token, currency, limit))
				{
					std::cout << "[" << data.address << "] ";
					std::array<int, 2> decimals;
					std::array<std::string, 2> symbols;
					for(int i = 0; i < 2; ++i) {
						if(auto token = get_token(node, data.tokens[i], false)) {
							decimals[i] = token->decimals;
							symbols[i] = token->symbol;
							std::cout << mmx::to_value(data.balance[i], token->decimals) << " " << token->symbol << (i == 0 ? " / " : "");
						}
					}
					const auto price = mmx::to_value(data.balance[1], decimals[1]) / mmx::to_value(data.balance[0], decimals[0]);
					std::cout << " (" << price << " " << symbols[1] << " / " << symbols[0] << ")" << std::endl;
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

				std::array<int, 2> decimals;
				std::array<std::string, 2> symbols;
				for(int i = 0; i < 2; ++i) {
					if(auto token = get_token(node, data.tokens[i])) {
						decimals[i] = token->decimals;
						symbols[i] = token->symbol;
						std::cout << (i ? "Currency" : "Token") << ": " << data.tokens[i] << std::endl;
						std::cout << "  Pool Balance:   " << mmx::to_value(data.balance[i], token->decimals) << " " << token->symbol << std::endl;
						std::cout << "  User Balance:   " << mmx::to_value(data.user_total[i], token->decimals) << " " << token->symbol << std::endl;
						std::cout << "  Wallet Balance: " << mmx::to_value(data.wallet[i], token->decimals) << " " << token->symbol << std::endl;
						std::cout << "  Total Fees:     " << mmx::to_value(data.fees_paid[i], token->decimals) << " " << token->symbol << std::endl;
					}
				}
				const auto price = mmx::to_value(data.balance[1], decimals[1]) / mmx::to_value(data.balance[0], decimals[0]);
				std::cout << "Price: " << price << " " << symbols[1] << " / " << symbols[0] << std::endl;
			}
			else {
				std::cerr << "Help: mmx node [info | peers | tx | get | fetch | balance | history | offers | swaps | swap | sync | revert | call | send | read | dump | dump_code]" << std::endl;
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
				if(module == "farm") {
					const auto info = node.get_farmed_block_summary(farmer.get_farmer_keys(), params->reward_activation);
					std::cout << "Farmed Blocks: " << info.num_blocks << std::endl;
					std::cout << "Total Rewards: " << mmx::to_value(info.total_rewards, params->decimals) << " MMX" << std::endl;
				}
				if(info->harvester) {
					std::cout << "[" << *info->harvester << "]" << std::endl;
				}
				std::cout << "Physical Size:  " << info->total_bytes / pow(1000, 4) << " TB" << std::endl;
				std::cout << "Effective Size: " << info->total_bytes_effective / pow(1000, 4) << " TBe" << std::endl;
				std::cout << "Plots:" << std::endl;
				uint64_t total_plots = 0;
				for(const auto& entry : info->plot_count) {
					total_plots += entry.second;
					std::cout << "  K" << int(entry.first) << ":   " << entry.second << std::endl;
				}
				std::cout << "  Total: " << total_plots << std::endl;
				if(!info->harvester_bytes.empty()) {
					std::cout << "Harvesters:" << std::endl;
				}
				for(const auto& entry : info->harvester_bytes) {
					std::cout << "  [" << entry.first << "] " << entry.second.first / pow(1000, 4) << " TB, " << entry.second.second / pow(1000, 4) << " TBe" << std::endl;
				}
				if(info->reward_addr) {
					std::cout << "Reward Address: " << info->reward_addr->to_string() << std::endl;
				}
				for(const auto& entry : info->pool_info) {
					const auto& data = entry.second;
					std::cout << std::endl << "Plot " << (data.is_plot_nft ? "NFT" : "Contract") << " [" << entry.first.to_string() << "]" << std::endl;
					if(data.name) {
						std::cout << "  Name: " << (*data.name) << std::endl;
					}
					std::cout << "  Server URL: " << (data.server_url ? *data.server_url : std::string("N/A (solo farming)")) << std::endl;
					std::string target = "N/A";
					if(data.pool_target) {
						target = data.pool_target->to_string();
					} else {
						if(data.is_plot_nft && info->reward_addr) {
							target = info->reward_addr->to_string();
						}
					}
					std::cout << "  Target Address: " << target << std::endl;
					std::cout << "  Plot Count: " << data.plot_count << std::endl;
					const auto iter = info->pool_stats.find(entry.first);
					if(iter != info->pool_stats.end()) {
						const auto& stats = iter->second;
						std::cout << "  Points: " << stats.valid_points << " OK / " << stats.failed_points << " FAIL" << std::endl;
						std::cout << "  Difficulty: " << stats.partial_diff << std::endl;
						if(stats.total_partials) {
							std::cout << "  Avg. Response: " << double(stats.total_response_time) / stats.total_partials / 1e3 << " sec" << std::endl;
						}
						if(stats.last_partial) {
							std::cout << "  Last Partial: " << vnx::get_date_string_ex("%Y-%m-%d %H:%M:%S", false, stats.last_partial) << std::endl;
						}
						for(const auto& entry : stats.error_count) {
							std::cout << "  Error[" << entry.first.to_string_value() << "]: " << entry.second << std::endl;
						}
					}
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
		else if(module == "pool")
		{
			std::string node_url = ":11333";
			vnx::read_config("node", node_url);

			vnx::Handle<vnx::Proxy> proxy = new vnx::Proxy("Proxy", vnx::Endpoint::from_url(node_url));
			proxy->forward_list = {"Farmer", "Node"};
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

			const auto info = farmer.get_farm_info();
			if(!info || !info->reward_addr) {
				std::cerr << "No reward address to query!" << std::endl;
				goto failed;
			}

			if(command == "info")
			{
				std::set<std::string> urls;
				for(const auto& entry : info->pool_info) {
					const auto& pool = entry.second;
					if(auto url = pool.server_url) {
						urls.insert(*url);
					}
				}
				for(const auto& url : urls) {
					std::cout << "Pool [" << url << "]" << std::endl;
					try {
						vnx::addons::http_request_options_t opt;
						opt.query["id"] = info->reward_addr->to_string();
						const auto data = http.get_json(url + "/account/info", opt).to_object();
						std::cout << "  Balance: " << data["balance"].to<double>() << " MMX" << std::endl;
						std::cout << "  Total Paid: " << data["total_paid"].to<double>() << " MMX" << std::endl;
						std::cout << "  Difficulty: " << data["difficulty"].to<uint64_t>() << std::endl;
						std::cout << "  Pool Share: " << data["pool_share"].to<float>() * 100 << " %" << std::endl;
						std::cout << "  Partial Rate: " << data["partial_rate"].to<float>() << " per hour" << std::endl;
						std::cout << "  Blocks Found: " << data["blocks_found"].to<int>() << std::endl;
						std::cout << "  Estimated Space: " << data["estimated_space"].to<float>() << " TBe" << std::endl;
					}
					catch(const std::exception& ex) {
						std::cout << "Failed with: " << ex.what() << std::endl;
					}
				}
			}
			else {
				std::cerr << "Help: mmx " << module << " [info]" << std::endl;
			}
		}
		else {
			std::cerr << "Help: mmx [node | wallet | farm | pool | harvester]" << std::endl;
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


