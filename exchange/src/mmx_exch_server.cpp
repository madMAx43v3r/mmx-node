/*
 * mmx_exch_server.cpp
 *
 *  Created on: Jan 26, 2022
 *      Author: mad
 */

#include <mmx/exchange/Server.h>

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

	vnx::init("mmx_exch_server", argc, argv, options);

	std::string node_url = ":11331";
	std::string endpoint = ":11340";

	vnx::read_config("node", node_url);
	vnx::read_config("endpoint", endpoint);

	vnx::Directory(root_path + "exchange/server").create();
	vnx::rocksdb::sync_type_codes(root_path + "exchange/server/type_codes");

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
		vnx::Handle<mmx::exchange::Server> module = new mmx::exchange::Server("ExchServer");
		module->storage_path = root_path + module->storage_path;
		proxy->import_list.push_back(module->input_blocks);
		module.start_detached();
	}

	proxy.start();

	vnx::wait();

	mmx::secp256k1_free();

	return 0;
}


