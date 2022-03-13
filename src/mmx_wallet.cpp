/*
 * mmx_wallet.cpp
 *
 *  Created on: Jan 7, 2022
 *      Author: mad
 */

#include <mmx/Wallet.h>

#include <vnx/vnx.h>
#include <vnx/Proxy.h>
#include <vnx/Server.h>
#include <vnx/Terminal.h>


int main(int argc, char** argv)
{
	mmx::secp256k1_init();

	std::string mmx_home;
	std::string mmx_network;
	if(auto path = ::getenv("MMX_HOME")) {
		mmx_home = path;
	}
	if(auto path = ::getenv("MMX_NETWORK")) {
		mmx_network = path;
	}
	const auto root_path = mmx_home + mmx_network;

	std::map<std::string, std::string> options;
	options["n"] = "node";
	options["node"] = "address";

	vnx::init("mmx_wallet", argc, argv, options);

	std::string node_url = ":11330";
	std::string endpoint = ":11335";

	vnx::read_config("node", node_url);
	vnx::read_config("endpoint", endpoint);

	vnx::rocksdb::sync_type_codes(root_path + "wallet/type_codes");

	vnx::Handle<vnx::Proxy> proxy = new vnx::Proxy("Proxy", vnx::Endpoint::from_url(node_url));
	proxy->forward_list = {"Node"};

	{
		vnx::Handle<vnx::Server> module = new vnx::Server("Server", vnx::Endpoint::from_url(endpoint));
		module.start_detached();
	}
	{
		vnx::Handle<vnx::Terminal> module = new vnx::Terminal("Terminal");
		module.start_detached();
	}
	{
		vnx::Handle<mmx::Wallet> module = new mmx::Wallet("Wallet");
		module->storage_path = mmx_home + module->storage_path;
		module->database_path = root_path + module->database_path;
		module.start_detached();
	}

	proxy.start();

	vnx::wait();

	mmx::secp256k1_free();

	return 0;
}

