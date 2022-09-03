/*
 * mmx_wallet.cpp
 *
 *  Created on: Jan 7, 2022
 *      Author: mad
 */

#include <mmx/Wallet.h>
#include <mmx/WebAPI.h>
//#include <mmx/exchange/Client.h>

#include <vnx/addons/FileServer.h>
#include <vnx/addons/HttpServer.h>

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
		std::cerr << "MMX_HOME = " << mmx_home << std::endl;
	}
	if(auto path = ::getenv("MMX_NETWORK")) {
		mmx_network = path;
		std::cerr << "MMX_NETWORK = " << mmx_network << std::endl;
	}
	const auto root_path = mmx_home + mmx_network;

	if(!root_path.empty()) {
		vnx::Directory(root_path).create();
	}

	std::map<std::string, std::string> options;
	options["n"] = "node";
	options["node"] = "address";

	vnx::init("mmx_wallet", argc, argv, options);

	std::string node_url = ":11330";
	vnx::read_config("node", node_url);

	vnx::rocksdb::sync_type_codes(root_path + "wallet/type_codes");

	vnx::Handle<vnx::Proxy> proxy = new vnx::Proxy("Proxy", vnx::Endpoint::from_url(node_url));
	proxy->forward_list = {"Node", "Router"};

	{
		vnx::Handle<vnx::Server> module = new vnx::Server("Server", vnx::Endpoint::from_url(":11335"));
		module.start_detached();
	}
	{
		vnx::Handle<vnx::Terminal> module = new vnx::Terminal("Terminal");
		module.start_detached();
	}
	{
		vnx::Handle<mmx::WebAPI> module = new mmx::WebAPI("WebAPI");
		module->config_path = mmx_home + module->config_path;
		module.start_detached();
	}
	{
		vnx::Handle<mmx::Wallet> module = new mmx::Wallet("Wallet");
		module->config_path = mmx_home + module->config_path;
		module->storage_path = mmx_home + module->storage_path;
		module->database_path = root_path + module->database_path;
		module.start_detached();
	}
//	{
//		vnx::Handle<mmx::exchange::Client> module = new mmx::exchange::Client("ExchClient");
//		module->storage_path = root_path + module->storage_path;
//		module.start_detached();
//	}
	{
		vnx::Handle<vnx::addons::FileServer> module = new vnx::addons::FileServer("FileServer_1");
		module->www_root = "www/web-gui/public/";
		module->directory_files.push_back("index.html");
		module.start_detached();
	}
	{
		vnx::Handle<vnx::addons::HttpServer> module = new vnx::addons::HttpServer("HttpServer");
		module->default_access = "NETWORK";
		module->components["/server/"] = "HttpServer";
		module->components["/wapi/"] = "WebAPI";
		module->components["/api/node/"] = "Node";
		module->components["/api/wallet/"] = "Wallet";
		module->components["/api/router/"] = "Router";
		module->components["/api/exchange/"] = "ExchClient";
		module->components["/gui/"] = "FileServer_1";
		module.start_detached();
	}

	proxy.start();

	vnx::wait();

	mmx::secp256k1_free();

	return 0;
}

