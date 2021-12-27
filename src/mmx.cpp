/*
 * mmx.cpp
 *
 *  Created on: Dec 21, 2021
 *      Author: mad
 */

#include <mmx/NodeClient.hxx>
#include <mmx/RouterClient.hxx>
#include <mmx/WalletClient.hxx>
#include <mmx/HarvesterClient.hxx>
#include <mmx/KeyFile.hxx>
#include <mmx/secp256k1.hpp>
#include <mmx/utils.h>

#include <vnx/vnx.h>
#include <vnx/Proxy.h>

#include <sodium.h>


int main(int argc, char** argv)
{
	mmx::secp256k1_init();

	std::map<std::string, std::string> options;
	options["n"] = "node";
	options["f"] = "file";
	options["j"] = "index";
	options["a"] = "amount";
	options["t"] = "target";
	options["x"] = "contract";
	options["node"] = "address";
	options["file"] = "path";
	options["index"] = "0..?";
	options["amount"] = "123.456";
	options["target"] = "address";
	options["contract"] = "address";

	vnx::write_config("log_level", 2);

	vnx::init("mmx", argc, argv, options);

	std::string module;
	std::string command;
	std::string node_url = ":11331";
	std::string file_name;
	std::string target_addr;
	std::string contract_addr;
	int64_t index = 0;
	double amount = 0;
	vnx::read_config("$1", module);
	vnx::read_config("$2", command);
	vnx::read_config("node", node_url);
	vnx::read_config("file", file_name);
	vnx::read_config("index", index);
	vnx::read_config("amount", amount);
	vnx::read_config("target", target_addr);
	vnx::read_config("contract", contract_addr);

	bool did_fail = false;
	auto params = mmx::get_params();

	mmx::addr_t target;
	mmx::addr_t contract;
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

	if(module != "wallet" || command != "create")
	{
		vnx::Handle<vnx::Proxy> module = new vnx::Proxy("Proxy", vnx::Endpoint::from_url(node_url));
		module->forward_list = {"Wallet", "Node", "Farmer", "Harvester", "Router"};
		module.start_detached();
	}

	try {
		if(module == "wallet")
		{
			mmx::WalletClient wallet("Wallet");
			try {
				if(command != "create") {
					wallet.open_wallet(index);
				}
			}
			catch(std::exception& ex) {
				vnx::log_error() << "Failed to open wallet: " << ex.what();
				goto failed;
			}
			if(command == "show")
			{
				int num_addrs = 1;
				vnx::read_config("$3", num_addrs);

				const auto amount = wallet.get_balance(contract);
				std::cout << "Balance: " << amount / pow(10, params->decimals) << " MMX (" << amount << ")" << std::endl;
				for(int i = 0; i < num_addrs; ++i) {
					std::cout << "Address[" << i << "]: " << wallet.get_address(i) << std::endl;
				}
			}
			else if(command == "keys")
			{
				const auto keys = wallet.get_farmer_keys(index);
				std::cout << "Pool Public Key:   " << keys->pool_public_key << std::endl;
				std::cout << "Farmer Public Key: " << keys->farmer_public_key << std::endl;
			}
			else if(command == "get")
			{
				std::string subject;
				vnx::read_config("$3", subject);

				if(subject == "address")
				{
					vnx::read_config("$4", index);
					if(index < 0) {
						vnx::log_error() << "Invalid index: " << index;
						goto failed;
					}
					const auto addr = wallet.get_address(index);
					std::cout << addr << std::endl;
				}
				else if(subject == "amount")
				{
					std::cout << wallet.get_balance(contract) << std::endl;
				}
				else if(subject == "balance")
				{
					std::cout << wallet.get_balance(contract) / pow(10, params->decimals) << std::endl;
				}
				else if(subject == "seed")
				{
					std::cout << wallet.get_master_seed(index) << std::endl;
				}
				else {
					std::cerr << "Help: mmx wallet get [address | amount | balance]" << std::endl;
				}
			}
			else if(command == "send")
			{
				const int64_t mojo = amount * pow(10, params->decimals);
				if(amount <= 0 || mojo <= 0) {
					vnx::log_error() << "Invalid amount: " << amount;
					goto failed;
				}
				if(target == mmx::addr_t()) {
					vnx::log_error() << "Missing destination address argument: -t | --target";
					goto failed;
				}
				std::string currency = "MMX";
				if(contract != mmx::addr_t()) {
					currency = "?";
				}
				const auto txid = wallet.send(mojo, target, contract);
				std::cout << "Sent " << mojo / pow(10, params->decimals) << " " << currency << " to " << target << std::endl;
				std::cout << "Transaction ID: " << txid << std::endl;
			}
			else if(command == "log")
			{
				for(const auto& entry : wallet.get_history()) {
					std::cout << "[" << entry.height << "] ";
					switch(entry.type) {
						case mmx::wallet::tx_type_e::SEND:    std::cout << "SEND    "; break;
						case mmx::wallet::tx_type_e::RECEIVE: std::cout << "RECEIVE "; break;
						case mmx::wallet::tx_type_e::REWARD:  std::cout << "REWARD  "; break;
						default: std::cout << "????    "; break;
					}
					std::cout << entry.amount / pow(10, params->decimals) << " MMX (" << entry.amount << ") -> " << entry.address << std::endl;
				}
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
					std::vector<uint8_t> seed(4096);
					randombytes_buf(seed.data(), seed.size());
					wallet.seed_value = mmx::hash_t(seed);
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
				std::cerr << "Help: mmx wallet [show | log | send | create]" << std::endl;
			}
		}
		else if(module == "node")
		{
			mmx::NodeClient node("Node");
			mmx::RouterClient router("Router");

			if(command == "balance")
			{
				mmx::addr_t address;
				if(!vnx::read_config("$3", address)) {
					vnx::log_error() << "Missing address argument!";
					goto failed;
				}
				const auto amount = node.get_balance(address, contract);
				std::cout << "Balance: " << amount / pow(10, params->decimals) << " MMX (" << amount << ")" << std::endl;
			}
			else if(command == "info")
			{
				const auto height = node.get_height();
				std::cout << "Synced: " << (node.get_synced_height() ? "Yes" : "No") << std::endl;
				std::cout << "Height: " << height << std::endl;
				for(uint32_t i = 0; i < 2 * params->finality_delay && i < height; ++i) {
					const auto hash = node.get_block_hash(height - i);
					std::cout << "[" << (height - i) << "] " << (hash ? *hash : mmx::hash_t()) << std::endl;
				}
			}
			else if(command == "sync")
			{
				node.start_sync();
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

				const auto peers = router.get_peers(-1);
				std::cout << "Got " << peers.size() << " known peers" << std::endl;
			}
			else if(command == "tx")
			{
				mmx::hash_t txid;
				vnx::read_config("$3", txid);

				const auto tx = node.get_transaction(txid);
				if(!tx) {
					vnx::log_error() << "No such transaction: " << txid;
					goto failed;
				}
				const auto info = node.get_tx_key(txid);
				const auto height = node.get_height();
				if(info) {
					std::cout << "Confirmations: " << height - info->height + 1 << std::endl;
				} else {
					std::cout << "Confirmations: none yet" << std::endl;
				}
				size_t i = 0;
				for(const auto& out : tx->outputs) {
					std::cout << "Output[" << i++ << "]: " << out.amount / pow(10, params->decimals) << " MMX (" << out.amount << ") -> " << out.address << std::endl;
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

					const auto amount = node.get_balance(address, contract);
					std::cout << amount / pow(10, params->decimals) << std::endl;
				}
				else if(subject == "amount")
				{
					mmx::addr_t address;
					vnx::read_config("$4", address);

					const auto amount = node.get_balance(address, contract);
					std::cout << amount << std::endl;
				}
				else if(subject == "height")
				{
					std::cout << node.get_height() << std::endl;
				}
				else if(subject == "block" || subject == "header")
				{
					int64_t height = 0;
					vnx::read_config("$4", height);

					if(height < 0) {
						vnx::log_error() << "Invalid height: " << height;
						goto failed;
					}
					const auto block = node.get_block_at(height);
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
				else if(subject == "tx")
				{
					mmx::hash_t txid;
					vnx::read_config("$4", txid);

					const auto tx = node.get_transaction(txid);
					{
						std::stringstream ss;
						vnx::PrettyPrinter printer(ss);
						vnx::accept(printer, tx);
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
				else {
					std::cerr << "Help: mmx node get [height | tx | balance | amount | block | header | peers]" << std::endl;
				}
			}
			else if(command == "show")
			{
				std::string subject;
				vnx::read_config("$3", subject);

				if(subject == "peers")
				{
					std::cout << "TODO" << std::endl;
				}
				else {
					std::cerr << "Help: mmx node show [peers]" << std::endl;
				}
			}
			else {
				std::cerr << "Help: mmx node [info | show | tx | get | balance | sync]" << std::endl;
			}
		}
		else if(module == "farm") {
			mmx::HarvesterClient harvester("Harvester");

			auto info = harvester.get_farm_info();

			if(command == "info") {
				std::cout << "Total space: " << info->total_bytes / pow(1024, 4) << " TiB" << std::endl;
				for(const auto& entry : info->plot_count) {
					std::cout << "K" << int(entry.first) << ": " << entry.second << " plots" << std::endl;
				}
			}
			else if(command == "reload") {
				harvester.reload();
			}
			else if(command == "get") {
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
				std::cerr << "Help: mmx farm [info | reload]" << std::endl;
			}
		}
		else {
			std::cerr << "Help: mmx [node | wallet | farm]" << std::endl;
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


