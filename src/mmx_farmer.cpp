/*
 * mmx_farmer.cpp
 *
 *  Created on: Jan 7, 2022
 *      Author: mad
 */

#include <mmx/Farmer.h>
#include <mmx/Wallet.h>
#include <mmx/Harvester.h>

#include <vnx/vnx.h>
#include <vnx/Proxy.h>
#include <vnx/Server.h>
#include <vnx/Terminal.h>


int main(int argc, char** argv)
{
	std::string mmx_home;
	std::string mmx_network;
	if(auto path = ::getenv("MMX_HOME")) {
		mmx_home = path;
		std::cerr << "MMX_HOME = " << mmx_home << std::endl;
	}
	if(auto path = ::getenv("MMX_NETWORK")) {
		mmx_network = path;
		std::cerr << "MMX_NETWORK = " << mmx_network << std::endl;
	}
	auto root_path = mmx_home + mmx_network;

	vnx::write_config("mmx_farmer.log_file_path", root_path + "logs/");

	mmx::secp256k1_init();

	std::map<std::string, std::string> options;
	options["n"] = "node";
	options["node"] = "address";

	vnx::init("mmx_farmer", argc, argv, options);

	std::string node_url = ":11330";
	std::string endpoint = "0.0.0.0:11333";

	vnx::read_config("node", node_url);
	vnx::read_config("endpoint", endpoint);

	vnx::Handle<vnx::Proxy> proxy = new vnx::Proxy("Proxy", vnx::Endpoint::from_url(node_url));
	proxy->forward_list = {"Node", "Router"};

	{
		vnx::Handle<vnx::Server> module = new vnx::Server("Server", vnx::Endpoint::from_url(endpoint));
		module->use_authentication = true;
		module->default_access = "USER";
		module.start_detached();
	}
	{
		vnx::Handle<vnx::Server> module = new vnx::Server("Server5", vnx::Endpoint::from_url(":11335"));
		module.start_detached();
	}
	{
		vnx::Handle<vnx::Terminal> module = new vnx::Terminal("Terminal");
		module.start_detached();
	}
	{
		vnx::Handle<mmx::Wallet> module = new mmx::Wallet("Wallet");
		module->config_path = mmx_home + module->config_path;
		module->storage_path = mmx_home + module->storage_path;
		module->database_path = root_path + module->database_path;
		module.start_detached();
	}
	{
		vnx::Handle<mmx::Farmer> module = new mmx::Farmer("Farmer");
		module.start_detached();
	}
	{
		vnx::Handle<mmx::Harvester> module = new mmx::Harvester("Harvester");
		proxy->import_list.push_back(module->input_challenges);
		proxy->export_list.push_back(module->output_proofs);
		module.start_detached();
	}

	proxy.start();

	vnx::wait();

	mmx::secp256k1_free();

	return 0;
}

