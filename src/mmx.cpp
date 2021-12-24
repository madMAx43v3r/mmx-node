/*
 * mmx.cpp
 *
 *  Created on: Dec 21, 2021
 *      Author: mad
 */

#include <mmx/NodeClient.hxx>
#include <mmx/WalletClient.hxx>
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
	std::string command2;
	std::string node_url = ".mmx_node.sock";
	std::string file_name;
	std::string target_addr;
	std::string contract_addr;
	int64_t index = 0;
	double amount = 0;
	vnx::read_config("$1", module);
	vnx::read_config("$2", command);
	vnx::read_config("$3", command2);
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

	{
		vnx::Handle<vnx::Proxy> module = new vnx::Proxy("Proxy", vnx::Endpoint::from_url(node_url));
		module->forward_list = {"Wallet", "Node", "Farmer"};
		module.start_detached();
	}

	if(module == "wallet")
	{
		mmx::WalletClient wallet("Wallet");
		try {
			wallet.open_wallet(index);
		}
		catch(std::exception& ex) {
			vnx::log_error() << "Failed to open wallet: " << ex.what();
			goto failed;
		}
		if(command == "show")
		{
			std::string currency = "MMX";
			if(contract != mmx::addr_t()) {
				currency = "?";
			}
			try {
				const auto amount = wallet.get_balance(contract);
				std::cout << "Balance: " << amount / 1e6 << " " << currency << " (" << amount << ")" << std::endl;
				std::cout << "Address[0]: " << wallet.get_address(0) << std::endl;
			}
			catch(std::exception& ex) {
				vnx::log_error() << "Failed with: " << ex.what();
				goto failed;
			}
		}
		else if(command == "keys")
		{
			try {
				const auto keys = wallet.get_farmer_keys(index);
				std::cout << "Pool Public Key:   " << keys->pool_public_key << std::endl;
				std::cout << "Farmer Public Key: " << keys->farmer_public_key << std::endl;
			}
			catch(std::exception& ex) {
				vnx::log_error() << "Failed with: " << ex.what();
				goto failed;
			}
		}
		else if(command == "get")
		{
			if(command2 == "address")
			{
				vnx::read_config("$4", index);
				if(index < 0) {
					vnx::log_error() << "Invalid index: " << index;
					goto failed;
				}
				try {
					const auto addr = wallet.get_address(index);
					std::cout << addr << std::endl;
				}
				catch(std::exception& ex) {
					vnx::log_error() << "Failed to get address: " << ex.what();
					goto failed;
				}
			}
			else {
				std::cerr << "Help: mmx wallet get [address]" << std::endl;
			}
		}
		else if(command == "send")
		{
			const uint64_t mojo = amount * pow(10, params->decimals);
			if(mojo <= 0) {
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
			try {
				const auto txid = wallet.send(mojo, target, contract);
				std::cout << "Sent " << mojo / pow(10, params->decimals) << " " << currency << " to " << target << std::endl;
				std::cout << "Transaction ID: " << txid << std::endl;
			}
			catch(std::exception& ex) {
				vnx::log_error() << "Failed to send: " << ex.what();
				goto failed;
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
			std::vector<uint8_t> seed(4096);
			randombytes_buf(seed.data(), seed.size());

			mmx::KeyFile wallet;
			wallet.seed_value = mmx::hash_t(seed);

			vnx::write_to_file(file_name, wallet);

			std::cout << "Created wallet '" << file_name << "' with seed: "
					<< std::endl << wallet.seed_value << std::endl;
		}
		else {
			std::cerr << "Help: mmx wallet [show | send | create]" << std::endl;
		}
	}
	else if(module == "node")
	{
		mmx::NodeClient node("Node");

		if(command == "balance")
		{
			try {
				mmx::addr_t address;
				mmx::addr_t contract;
				vnx::read_config("$3", address);
				vnx::read_config("$4", contract);

				std::string currency = "MMX";
				if(contract != mmx::addr_t()) {
					currency = "?";
				}
				const auto amount = node.get_balance(address, contract);
				std::cout << "Balance: " << amount / 1e6 << " " << currency << " (" << amount << ")" << std::endl;
			}
			catch(std::exception& ex) {
				vnx::log_error() << ex.what();
				goto failed;
			}
		}
	}
	else {
		std::cerr << "Help: mmx [node | wallet | farm]" << std::endl;
	}

	goto exit;
failed:
	did_fail = true;
exit:
	vnx::close();

	mmx::secp256k1_free();

	return did_fail ? -1 : 0;
}


