/*
 * mmx_node.cpp
 *
 *  Created on: Dec 10, 2021
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/Wallet.h>
#include <mmx/TimeLord.h>
#include <mmx/Farmer.h>
#include <mmx/Harvester.h>
#include <mmx/Router.h>
#include <mmx/WebAPI.h>
//#include <mmx/exchange/Client.h>
#include <mmx/WalletClient.hxx>
#include <mmx/secp256k1.hpp>
#include <mmx/utils.h>

#include <vnx/vnx.h>
#include <vnx/Server.h>
#include <vnx/Terminal.h>
#include <vnx/TcpEndpoint.hxx>
#include <vnx/addons/FileServer.h>
#include <vnx/addons/HttpServer.h>


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
	auto root_path = mmx_home + mmx_network;

	if(!root_path.empty()) {
		vnx::Directory(root_path).create();
	}
	vnx::write_config("mmx_node.log_file_path", root_path + "logs/");

	std::map<std::string, std::string> options;
	options["t"] = "timelord";

	vnx::init("mmx_node", argc, argv, options);

	const auto params = mmx::get_params();

	if(params->vdf_seed == "test1" || params->vdf_seed == "test2" || params->vdf_seed == "test3" || params->vdf_seed == "test4" || params->vdf_seed == "test5") {
		vnx::log_error() << "This version is not compatible with testnet 1-5, please remove NETWORK file and try again to switch to testnet6.";
		vnx::close();
		return -1;
	}
	bool with_farmer = true;
	bool with_wallet = true;
	bool with_timelord = true;
	bool with_harvester = true;
	std::string public_endpoint = "0.0.0.0:11330";
	vnx::read_config("wallet", with_wallet);
	vnx::read_config("farmer", with_farmer);
	vnx::read_config("timelord", with_timelord);
	vnx::read_config("harvester", with_harvester);
	vnx::read_config("public_endpoint", public_endpoint);

#ifdef WITH_OPENCL
	try {
		std::string platform_name;
		vnx::read_config("opencl.platform", platform_name);
		automy::basic_opencl::create_context(CL_DEVICE_TYPE_GPU, platform_name);
	}
	catch(const std::exception& ex) {
		vnx::log_info() << "No OpenCL GPU platform found: " << ex.what();
	}
#endif

	if(with_farmer) {
		with_wallet = true;
	} else {
		with_harvester = false;
	}
	vnx::rocksdb::sync_type_codes(root_path + "type_codes");

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
		vnx::Handle<vnx::Server> module = new vnx::Server("Server0", vnx::Endpoint::from_url(public_endpoint));
		module->use_authentication = true;
		module->default_access = "USER";
		module.start_detached();
	}
	{
		vnx::Handle<vnx::Server> module = new vnx::Server("Server1", vnx::Endpoint::from_url(":11331"));
		module.start_detached();
	}
	if(with_wallet) {
		{
			vnx::Handle<mmx::Wallet> module = new mmx::Wallet("Wallet");
			module->config_path = mmx_home + module->config_path;
			module->storage_path = mmx_home + module->storage_path;
			module->database_path = root_path + module->database_path;
			module.start_detached();
		}
//		{
//			vnx::Handle<mmx::exchange::Client> module = new mmx::exchange::Client("ExchClient");
//			module->storage_path = root_path + module->storage_path;
//			module.start_detached();
//		}
		{
			vnx::Handle<vnx::Server> module = new vnx::Server("Server5", vnx::Endpoint::from_url(":11335"));
			module.start_detached();
		}
	}
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
		module->components["/api/farmer/"] = "Farmer";
		module->components["/api/router/"] = "Router";
		module->components["/api/harvester/"] = "Harvester";
		module->components["/api/exchange/"] = "ExchClient";
		module->components["/gui/"] = "FileServer_1";
		module.start_detached();
	}
	if(with_timelord) {
		{
			vnx::Handle<mmx::TimeLord> module = new mmx::TimeLord("TimeLord");
			module->storage_path = root_path + module->storage_path;
			module.start_detached();
		}
		{
			vnx::Handle<vnx::Server> module = new vnx::Server("Server2", vnx::Endpoint::from_url(":11332"));
			module.start_detached();
		}
	}
	if(with_farmer) {
		{
			vnx::Handle<mmx::Farmer> module = new mmx::Farmer("Farmer");
			module.start_detached();
		}
		{
			vnx::Handle<vnx::Server> module = new vnx::Server("Server3", vnx::Endpoint::from_url("0.0.0.0:11333"));
			module->use_authentication = true;
			module->default_access = "USER";
			module.start_detached();
		}
	}
	if(with_harvester) {
		vnx::Handle<mmx::Harvester> module = new mmx::Harvester("Harvester");
		module.start_detached();
	}

	while(vnx::do_run())
	{
		vnx::Handle<mmx::Node> module = new mmx::Node("Node");
		module->storage_path = root_path + module->storage_path;
		module->database_path = root_path + module->database_path;
		module.start();
		module.wait();
		if(!module->do_restart) {
			break;
		}
	}
	vnx::close();

	mmx::secp256k1_free();

#ifdef WITH_OPENCL
	automy::basic_opencl::release_context();
#endif

	return 0;
}


