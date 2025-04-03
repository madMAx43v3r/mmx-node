/*
 * mmx_wallet.cpp
 *
 *  Created on: Jan 7, 2022
 *      Author: mad
 */

#include <mmx/Wallet.h>
#include <mmx/WebAPI.h>
#include <mmx/Qt_GUI.h>

#include <vnx/addons/FileServer.h>
#include <vnx/addons/HttpServer.h>

#include <vnx/vnx.h>
#include <vnx/Proxy.h>
#include <vnx/Server.h>
#include <vnx/Terminal.h>
#include <vnx/TcpEndpoint.hxx>


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
	vnx::Directory(mmx_network).create();

	std::map<std::string, std::string> options;
	options["n"] = "node";
	options["node"] = "address";

	vnx::init("mmx_wallet", argc, argv, options);
	{
		std::string version;
		std::string commit;
		vnx::read_config("build.version", version);
		vnx::read_config("build.commit", commit);
		vnx::log_info() << "Build version: " << version;
		vnx::log_info() << "Build commit: " << commit;
	}

	bool with_gui = false;
	std::string node_url = ":11330";

	vnx::read_config("gui", with_gui);
	vnx::read_config("node", node_url);

	vnx::write_config("farmer", false);
	vnx::write_config("local_node", false);

	const auto api_token = mmx::hash_t::random().to_string();

	mmx::sync_type_codes(mmx_network + "wallet/type_codes");

	auto node = vnx::Endpoint::from_url(node_url);
	if(auto tcp = std::dynamic_pointer_cast<const vnx::TcpEndpoint>(node)) {
		if(!tcp->port || tcp->port == vnx::TcpEndpoint::default_port) {
			auto tmp = vnx::clone(tcp);
			tmp->port = 11330;
			node = tmp;
		}
	}
	vnx::Handle<vnx::Proxy> proxy = new vnx::Proxy("Proxy", node);
	proxy->forward_list = {"Node", "Router"};

	{
		vnx::Handle<vnx::Server> module = new vnx::Server("Server", vnx::Endpoint::from_url("localhost:11335"));
		module->use_authentication = true;
		module->default_access = "USER";
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
		module->database_path = mmx_network + module->database_path;
		module.start_detached();
	}
	{
		vnx::Handle<vnx::addons::FileServer> module = new vnx::addons::FileServer("FileServer_1");
		module->www_root = "ui/dist/gui/";
		module->directory_files.push_back("index.html");
		module.start_detached();
	}
	int http_port = 0;
	std::string api_token_header;
	{
		vnx::Handle<vnx::addons::HttpServer> module = new vnx::addons::HttpServer("HttpServer");
		module->default_access = "NETWORK";
		module->components["/server/"] = "HttpServer";
		module->components["/wapi/"] = "WebAPI";
		module->components["/api/node/"] = "Node";
		module->components["/api/wallet/"] = "Wallet";
		module->components["/api/router/"] = "Router";
		module->components["/gui/"] = "FileServer_1";
		if(with_gui) {
			http_port = module->port;
			api_token_header = module->token_header_name;
			module->token_map[api_token] = "ADMIN";
		}
		module.start_detached();
	}

	proxy.start();

	if(with_gui) {
		const auto host = "localhost:" + std::to_string(http_port);
#ifdef WITH_QT
		qt_gui_exec(argv, host, api_token, api_token_header);
#else
		vnx::log_warn() << "No GUI available";
		vnx::wait();
#endif
	} else {
		vnx::wait();
	}

	vnx::close();

	mmx::secp256k1_free();

	return 0;
}

