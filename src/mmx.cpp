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
#include <mmx/exchange/ClientClient.hxx>
#include <mmx/exchange/trade_pair_t.hpp>
#include <mmx/Contract.hxx>
#include <mmx/contract/NFT.hxx>
#include <mmx/contract/Token.hxx>
#include <mmx/contract/Staking.hxx>
#include <mmx/KeyFile.hxx>
#include <mmx/secp256k1.hpp>
#include <mmx/hash_t.hpp>
#include <mmx/utils.h>

#include <vnx/vnx.h>
#include <vnx/Proxy.h>


void show_history(const std::vector<mmx::tx_entry_t>& history, mmx::NodeClient& node, std::shared_ptr<const mmx::ChainParams> params)
{
	std::unordered_map<mmx::addr_t, std::shared_ptr<const mmx::Contract>> contract_map;
	for(const auto& entry : history) {
		std::cout << "[" << entry.height << "] ";
		switch(entry.type) {
			case mmx::tx_type_e::SEND:    std::cout << "SEND    - "; break;
			case mmx::tx_type_e::SPEND:   std::cout << "SPEND   - "; break;
			case mmx::tx_type_e::INPUT:   std::cout << "INPUT   + "; break;
			case mmx::tx_type_e::RECEIVE: std::cout << "RECEIVE + "; break;
			case mmx::tx_type_e::REWARD:  std::cout << "REWARD  + "; break;
			default: std::cout << "????    "; break;
		}
		auto iter = contract_map.find(entry.contract);
		if(iter == contract_map.end()) {
			iter = contract_map.emplace(entry.contract, std::dynamic_pointer_cast<const mmx::Contract>(node.get_contract(entry.contract))).first;
		}
		if(auto nft = std::dynamic_pointer_cast<const mmx::contract::NFT>(iter->second)) {
			std::cout << entry.contract << " -> " << entry.address << std::endl;
		} else {
			const auto token = std::dynamic_pointer_cast<const mmx::contract::Token>(iter->second);
			const auto decimals = token ? token->decimals : params->decimals;
			std::cout << entry.amount / pow(10, decimals) << " " << (token ? token->symbol : "MMX") << " (" << entry.amount << ") -> " << entry.address << std::endl;
		}
	}
}


int main(int argc, char** argv)
{
	mmx::secp256k1_init();

	std::map<std::string, std::string> options;
	options["n"] = "node";
	options["f"] = "file";
	options["j"] = "index";
	options["k"] = "server";
	options["a"] = "amount";
	options["b"] = "bid";
	options["m"] = "outputs";
	options["s"] = "source";
	options["t"] = "target";
	options["x"] = "contract";
	options["y"] = "yes";
	options["node"] = "address";
	options["file"] = "path";
	options["index"] = "0..?";
	options["server"] = "index";
	options["amount"] = "123.456";
	options["bid"] = "123.456";
	options["outputs"] = "count";
	options["source"] = "address";
	options["target"] = "address";
	options["contract"] = "address";

	vnx::write_config("log_level", 2);

	vnx::init("mmx", argc, argv, options);

	std::string module;
	std::string command;
	std::string file_name;
	std::string source_addr;
	std::string target_addr;
	std::string contract_addr;
	int64_t index = 0;
	int64_t server_index = 0;
	int64_t num_outputs = 0;
	double amount = 0;
	double bid_amount = 0;
	bool pre_accept = false;
	vnx::read_config("$1", module);
	vnx::read_config("$2", command);
	vnx::read_config("yes", pre_accept);
	vnx::read_config("file", file_name);
	vnx::read_config("index", index);
	vnx::read_config("server", server_index);
	vnx::read_config("amount", amount);
	vnx::read_config("bid", bid_amount);
	vnx::read_config("outputs", num_outputs);
	vnx::read_config("source", source_addr);
	vnx::read_config("target", target_addr);
	vnx::read_config("contract", contract_addr);

	bool did_fail = false;
	auto params = mmx::get_params();

	mmx::spend_options_t spend_options;
	spend_options.split_output = std::max<uint32_t>(num_outputs, 1);

	mmx::NodeClient node("Node");

	mmx::addr_t source;
	mmx::addr_t target;
	mmx::addr_t contract;
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
		vnx::log_error() << "Invalid contract: " << ex.what();
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

			if(command == "show")
			{
				int num_addrs = 1;
				vnx::read_config("$3", num_addrs);

				std::vector<mmx::addr_t> nfts;
				for(const auto& entry : wallet.get_balances(index))
				{
					const auto contract = node.get_contract(entry.first);
					if(std::dynamic_pointer_cast<const mmx::contract::NFT>(contract)) {
						nfts.push_back(entry.first);
					} else {
						const auto token = std::dynamic_pointer_cast<const mmx::contract::Token>(contract);
						if(token || entry.first == mmx::addr_t()) {
							const auto decimals = token ? token->decimals : params->decimals;
							std::cout << "Balance: " << entry.second / pow(10, decimals) << " " << (token ? token->symbol : "MMX") << " (" << entry.second << ")";
							if(token) {
								std::cout << " [" << entry.first << "]";
							}
							std::cout << std::endl;
						}
					}
				}
				for(const auto& addr : nfts) {
					std::cout << "NFT: " << addr << std::endl;
				}
				for(const auto& entry : wallet.get_contracts(index))
				{
					const auto& address = entry.first;
					const auto& contract = entry.second;
					std::cout << "Contract: " << address << " (" << contract->get_type_name();
					if(auto token = std::dynamic_pointer_cast<const mmx::contract::Token>(contract)) {
						std::cout << ", " << token->symbol << ", " << token->name;
					}
					if(auto staking = std::dynamic_pointer_cast<const mmx::contract::Staking>(contract)) {
						if(auto contract = node.get_contract(staking->currency)) {
							if(auto token = std::dynamic_pointer_cast<const mmx::contract::Token>(contract)) {
								std::cout << ", " << token->symbol;
							} else {
								std::cout << ", ???";
							}
						} else {
							std::cout << ", ???";
						}
					}
					std::cout << ")" << std::endl;

					for(const auto& entry : node.get_total_balances({address}))
					{
						const auto currency = node.get_contract(entry.first);
						const auto token = std::dynamic_pointer_cast<const mmx::contract::Token>(currency);
						const auto decimals = token ? token->decimals : params->decimals;
						std::cout << "  Balance: " << entry.second / pow(10, decimals) << " " << (token ? token->symbol : "MMX") << " (" << entry.second << ")" << std::endl;
					}
				}
				for(int i = 0; i < num_addrs; ++i) {
					std::cout << "Address[" << i << "]: " << wallet.get_address(index, i) << std::endl;
				}
			}
			else if(command == "accounts")
			{
				for(const auto& entry : wallet.get_accounts()) {
					const auto& config = entry.second;
					std::cout << "[" << entry.first << "] name = '" << config.name << "', account = " << config.index
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
					std::cout << wallet.get_balance(index, contract) << std::endl;
				}
				else if(subject == "balance")
				{
					const auto token = std::dynamic_pointer_cast<const mmx::contract::Token>(node.get_contract(contract));
					std::cout << wallet.get_balance(index, contract) / pow(10, token ? token->decimals : params->decimals) << std::endl;
				}
				else if(subject == "contracts")
				{
					for(const auto& entry : wallet.get_contracts(index)) {
						std::cout << entry.first << std::endl;
					}
				}
				else if(subject == "seed")
				{
					std::cout << wallet.get_master_seed(index) << std::endl;
				}
				else {
					std::cerr << "Help: mmx wallet get [address | balance | amount | contracts | seed]" << std::endl;
				}
			}
			else if(command == "send")
			{
				const auto token = std::dynamic_pointer_cast<const mmx::contract::Token>(node.get_contract(contract));

				const auto decimals = token ? token->decimals : params->decimals;
				const int64_t mojo = amount * pow(10, decimals);
				if(amount <= 0 || mojo <= 0) {
					vnx::log_error() << "Invalid amount: " << amount << " (-a | --amount)";
					goto failed;
				}
				if(target == mmx::addr_t()) {
					vnx::log_error() << "Missing destination address argument: -t | --target";
					goto failed;
				}
				const auto txid = wallet.send(index, mojo, target, contract, spend_options);
				std::cout << "Sent " << mojo / pow(10, decimals) << " " << (token ? token->symbol : "MMX") << " (" << mojo << ") to " << target << std::endl;
				std::cout << "Transaction ID: " << txid << std::endl;
			}
			else if(command == "send_from")
			{
				const auto token = std::dynamic_pointer_cast<const mmx::contract::Token>(node.get_contract(contract));

				const auto decimals = token ? token->decimals : params->decimals;
				const int64_t mojo = amount * pow(10, decimals);
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
				const auto txid = wallet.send_from(index, mojo, target, source, contract, spend_options);
				std::cout << "Sent " << mojo / pow(10, decimals) << " " << (token ? token->symbol : "MMX") << " (" << mojo << ") to " << target << std::endl;
				std::cout << "Transaction ID: " << txid << std::endl;
			}
			else if(command == "transfer")
			{
				if(target == mmx::addr_t()) {
					vnx::log_error() << "Missing destination address argument: -t | --target";
					goto failed;
				}
				const auto txid = wallet.send(index, 1, target, contract);
				std::cout << "Sent " << contract << " to " << target << std::endl;
				std::cout << "Transaction ID: " << txid << std::endl;
			}
			else if(command == "mint")
			{
				const auto token = std::dynamic_pointer_cast<const mmx::contract::Token>(node.get_contract(contract));
				if(!token) {
					vnx::log_error() << "No such token: " << contract;
					goto failed;
				}
				const int64_t mojo = amount * pow(10, token->decimals);
				if(amount <= 0 || mojo <= 0) {
					vnx::log_error() << "Invalid amount: " << amount << " (-a | --amount)";
					goto failed;
				}
				if(target == mmx::addr_t()) {
					vnx::log_error() << "Missing destination address argument: -t | --target";
					goto failed;
				}
				const auto txid = wallet.mint(index, mojo, target, contract);
				std::cout << "Minted " << mojo / pow(10, token->decimals) << " (" << mojo << ") " << token->symbol << " to " << target << std::endl;
				std::cout << "Transaction ID: " << txid << std::endl;
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
				const auto txid = wallet.deploy(index, payload);
				std::cout << "Deployed " << payload->get_type_name() << " as " << mmx::addr_t(txid) << std::endl;
				std::cout << "Transaction ID: " << txid << std::endl;
			}
			else if(command == "log")
			{
				int64_t since = 0;
				vnx::read_config("$3", since);

				show_history(wallet.get_history(index, since), node, params);
			}
			else if(command == "create")
			{
				if(file_name.empty()) {
					file_name = "wallet.dat";
				}
				if(vnx::File(file_name).exists()) {
					vnx::log_error() << "Wallet file '" << file_name << "' already exists";
					goto failed;
				}
				std::string seed_str;
				vnx::read_config("$3", seed_str);

				mmx::KeyFile wallet;
				if(seed_str.empty()) {
					wallet.seed_value = mmx::hash_t::random();
				} else {
					if(seed_str.size() == 64) {
						wallet.seed_value.from_string(seed_str);
					} else {
						vnx::log_error() << "Invalid seed value: '" << seed_str << "'";
						goto failed;
					}
				}
				vnx::write_to_file(file_name, wallet);

				std::cout << "Created wallet '" << file_name << "' with seed: "
						<< std::endl << wallet.seed_value << std::endl;
			}
			else {
				std::cerr << "Help: mmx wallet [show | log | send | send_from | transfer | mint | deploy | create | accounts]" << std::endl;
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
					const auto contract = node.get_contract(entry.first);
					if(!std::dynamic_pointer_cast<const mmx::contract::NFT>(contract)) {
						const auto token = std::dynamic_pointer_cast<const mmx::contract::Token>(contract);
						const auto decimals = token ? token->decimals : params->decimals;
						std::cout << "Balance: " << entry.second / pow(10, decimals) << " " << (token ? token->symbol : "MMX") << " (" << entry.second << ")" << std::endl;
					}
				}
			}
			else if(command == "info")
			{
				const auto info = node.get_network_info();
				std::cout << "Synced:   " << (node.get_synced_height() ? "Yes" : "No") << std::endl;
				std::cout << "Height:   " << info->height << std::endl;
				std::cout << "Netspace: " << info->total_space / pow(1024, 4) << " TiB" << std::endl;
				std::cout << "Reward:   " << info->block_reward / 1e6 << " MMX" << std::endl;
				std::cout << "Supply:   " << info->total_supply / 1e6 << " MMX" << std::endl;
				std::cout << "N(UTXO):    " << info->utxo_count << std::endl;
				std::cout << "N(Address): " << info->address_count << std::endl;
				for(uint32_t i = 0; i < 2 * params->finality_delay && i < info->height; ++i) {
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

				show_history(node.get_history_for({address}, since), node, params);
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
					std::cout << ", " << peer.credits << " credits";
					std::cout << ", " << peer.tx_credits / 1000 << " tx credits";
					std::cout << ", " << peer.ping_ms << " ms ping";
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
				for(const auto& out : tx->outputs) {
					const auto token = std::dynamic_pointer_cast<const mmx::contract::Token>(node.get_contract(out.contract));
					const auto decimals = token ? token->decimals : params->decimals;
					std::cout << "Output[" << i++ << "]: " << out.amount / pow(10, decimals)
							<< " " << (token ? token->symbol : "MMX") << " (" << out.amount << ") -> " << out.address << std::endl;
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

					const auto token = std::dynamic_pointer_cast<const mmx::contract::Token>(node.get_contract(contract));
					std::cout << node.get_balance(address, contract) / pow(10, token ? token->decimals : params->decimals) << std::endl;
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
					int64_t height = 0;
					vnx::read_config("$4", height);

					const auto block = node.get_block_at(height);
					{
						std::stringstream ss;
						vnx::PrettyPrinter printer(ss);
						vnx::accept(printer, block);
						std::cout << ss.str() << std::endl;
					}
				}
				else if(subject == "header")
				{
					int64_t height = 0;
					vnx::read_config("$4", height);

					const auto header = node.get_header_at(height);
					{
						std::stringstream ss;
						vnx::PrettyPrinter printer(ss);
						vnx::accept(printer, header);
						std::cout << ss.str() << std::endl;
					}
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
						vnx::log_error() << "Missing peer argument: node fetch <peer> [height]";
						goto failed;
					}
					if(height < 0) {
						vnx::log_error() << "Invalid height: " << height;
						goto failed;
					}
					const auto block = router.fetch_block_at(from_peer, height);
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
			else {
				std::cerr << "Help: mmx node [info | peers | tx | get | fetch | balance | history | sync]" << std::endl;
			}
		}
		else if(module == "farm")
		{
			std::string node_url = ":11333";
			vnx::read_config("node", node_url);

			vnx::Handle<vnx::Proxy> proxy = new vnx::Proxy("Proxy", vnx::Endpoint::from_url(node_url));
			proxy->forward_list = {"Farmer", "Harvester", "Node"};
			proxy.start_detached();

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
				std::cout << "Total space: " << info->total_bytes / pow(1024, 4) << " TiB" << std::endl;
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
		else if(module == "exch") {
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
				auto bid_token = std::dynamic_pointer_cast<const mmx::contract::Token>(node.get_contract(pair.bid));
				auto ask_token = std::dynamic_pointer_cast<const mmx::contract::Token>(node.get_contract(pair.ask));

				if(!bid_token && pair.bid != mmx::addr_t()) {
					vnx::log_error() << "Invalid token: " << pair.bid;
					goto failed;
				}
				if(!ask_token && pair.ask != mmx::addr_t()) {
					vnx::log_error() << "Invalid token: " << pair.ask;
					goto failed;
				}
				const auto bid_symbol = (bid_token ? bid_token->symbol : "MMX");
				const auto ask_symbol = (ask_token ? ask_token->symbol : "MMX");
				const auto bid_factor = pow(10, bid_token ? bid_token->decimals : params->decimals);
				const auto ask_factor = pow(10, ask_token ? ask_token->decimals : params->decimals);

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
				auto bid_token = std::dynamic_pointer_cast<const mmx::contract::Token>(node.get_contract(pair.bid));
				auto ask_token = std::dynamic_pointer_cast<const mmx::contract::Token>(node.get_contract(pair.ask));

				if(!bid_token && pair.bid != mmx::addr_t()) {
					vnx::log_error() << "Invalid token: " << pair.bid;
					goto failed;
				}
				if(!ask_token && pair.ask != mmx::addr_t()) {
					vnx::log_error() << "Invalid token: " << pair.ask;
					goto failed;
				}
				const auto bid_symbol = (bid_token ? bid_token->symbol : "MMX");
				const auto ask_symbol = (ask_token ? ask_token->symbol : "MMX");
				const auto bid_factor = pow(10, bid_token ? bid_token->decimals : params->decimals);
				const auto ask_factor = pow(10, ask_token ? ask_token->decimals : params->decimals);

				const uint64_t ask = amount * ask_factor;
				const uint64_t bid = bid_amount * bid_factor;
				if(bid == 0) {
					vnx::log_error() << "Invalid bid amount! (-b)";
					goto failed;
				}
				const auto trades = client.make_trade(index, pair, bid, ask > 0 ? vnx::optional<uint64_t>(ask) : nullptr);
				if(trades.empty()) {
					vnx::log_error() << "Unable to make trade!";
					goto failed;
				}
				std::cout << "Server: " << server << std::endl;
				std::cout << "Bids: ";
				int i = 0;
				uint64_t total_bid = 0;
				uint64_t total_ask = 0;
				for(const auto& trade : trades) {
					total_bid += trade.bid;
					total_ask += trade.ask ? *trade.ask : 0;
					std::cout << (i++ ? " + " : "") << trade.bid / bid_factor;
				}
				std::cout << " " << bid_symbol << std::endl;
				std::cout << "----------------------------------------------------------------" << std::endl;
				std::cout << "Total Bid: " << total_bid / bid_factor << " (" << total_bid << ") " << bid_symbol << std::endl;
				if(ask > 0) {
					std::cout << "Total Ask: " << total_ask / ask_factor << " (" << total_ask << ") " << ask_symbol << std::endl;
					std::cout << "Price: " << bid / double(ask) << " " << bid_symbol << " / " << ask_symbol << std::endl;
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
					const auto matched = client.match(server, pair, trades);
					if(matched.empty()) {
						vnx::log_error() << "Matched nothing!";
						goto failed;
					}
					int i = 0;
					uint64_t total_bid = 0;
					uint64_t total_match = 0;
					for(const auto& order : matched) {
						total_bid += order.bid;
						total_match += order.ask;
						std::cout << "Trade[" << i++ << "]: " << order.ask / ask_factor << " " << ask_symbol << " for " << order.bid / bid_factor
								<< " " << bid_symbol << " [" << order.bid / double(order.ask) << " " << bid_symbol << " / " << ask_symbol << "]" << std::endl;
					}
					std::cout << "----------------------------------------------------------------" << std::endl;
					std::cout << "Total: " << total_match / ask_factor << " " << ask_symbol << " for " << total_bid / bid_factor
							<< " " << bid_symbol << " [" << double(total_bid) / total_match << " " << bid_symbol << " / " << ask_symbol << "]" << std::endl;

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
						int i = 0;
						std::cout << "----------------------------------------------------------------" << std::endl;
						for(const auto& order : matched) {
							std::cout << "Trade[" << i++ << "]: ";
							try {
								const auto id = client.execute(server, index, order);
								std::cout << order.ask / ask_factor << " " << ask_symbol << " (" << id << ")" << std::endl;
							} catch(std::exception& ex) {
								std::cout << ex.what() << std::endl;
							}
						}
					}
				}
			}
			else if(command == "offers")
			{
				for(const auto offer : client.get_all_offers())
				{
					auto bid_token = std::dynamic_pointer_cast<const mmx::contract::Token>(node.get_contract(offer->pair.bid));
					auto ask_token = std::dynamic_pointer_cast<const mmx::contract::Token>(node.get_contract(offer->pair.ask));
					const auto bid_symbol = (bid_token ? bid_token->symbol : "MMX");
					const auto ask_symbol = (ask_token ? ask_token->symbol : "MMX");
					const auto bid_factor = pow(10, bid_token ? bid_token->decimals : params->decimals);
					const auto ask_factor = pow(10, ask_token ? ask_token->decimals : params->decimals);
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

				auto bid_token = std::dynamic_pointer_cast<const mmx::contract::Token>(node.get_contract(pair.bid));
				auto ask_token = std::dynamic_pointer_cast<const mmx::contract::Token>(node.get_contract(pair.ask));
				const auto bid_symbol = (bid_token ? bid_token->symbol : "MMX");
				const auto ask_symbol = (ask_token ? ask_token->symbol : "MMX");
				const auto bid_factor = pow(10, bid_token ? bid_token->decimals : params->decimals);

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

				auto bid_token = std::dynamic_pointer_cast<const mmx::contract::Token>(node.get_contract(pair.bid));
				auto ask_token = std::dynamic_pointer_cast<const mmx::contract::Token>(node.get_contract(pair.ask));

				mmx::exchange::amount_t have;
				have.amount = amount > 0 ? amount / pow(10, bid_token ? bid_token->decimals : params->decimals) : 1;
				have.currency = pair.bid;
				const auto price = client.get_price(server, pair.ask, have);
				if(price.value > 0) {
					std::cout << price.inverse / double(price.value) << " "
							<< (bid_token ? bid_token->symbol : "MMX") << " / " << (ask_token ? ask_token->symbol : "MMX") << std::endl;
				} else {
					std::cout << "No orders available!" << std::endl;
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
			else {
				std::cerr << "Help: mmx exch [offer | trade | offers | orders | price | servers | cancel]" << std::endl;
			}
		}
		else {
			std::cerr << "Help: mmx [node | wallet | farm | exch]" << std::endl;
		}
	}
	catch(std::exception& ex) {
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


