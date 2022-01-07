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
#include <mmx/secp256k1.hpp>
#include <mmx/utils.h>

#include <vnx/vnx.h>
#include <vnx/Server.h>
#include <vnx/Terminal.h>
#include <vnx/TcpEndpoint.hxx>
#include <vnx/addons/HttpServer.h>


int main(int argc, char** argv)
{
	mmx::secp256k1_init();

	std::map<std::string, std::string> options;
	options["t"] = "timelord";

	vnx::init("mmx_node", argc, argv, options);

	const auto params = mmx::get_params();

	if(params->vdf_seed == "test1" || params->vdf_seed == "test2") {
		vnx::log_error() << "This version is not compatible with testnet1/2, please remove NETWORK file and try again to switch to testnet3.";
		vnx::close();
		return -1;
	}
	bool with_farmer = true;
	bool with_wallet = true;
	bool with_timelord = true;
	std::string endpoint = ":11331";
	std::string public_endpoint = "0.0.0.0:11330";
	std::string root_path;
	vnx::read_config("wallet", with_wallet);
	vnx::read_config("farmer", with_farmer);
	vnx::read_config("timelord", with_timelord);
	vnx::read_config("endpoint", endpoint);
	vnx::read_config("public_endpoint", public_endpoint);
	vnx::read_config("root_path", root_path);

	if(!root_path.empty()) {
		vnx::Directory(root_path).create();
	}
	try {
		std::string platform_name;
		vnx::read_config("opencl.platform", platform_name);
		automy::basic_opencl::create_context(CL_DEVICE_TYPE_GPU, platform_name);
	}
	catch(const std::exception& ex) {
		vnx::log_info() << "No OpenCL GPU platform found: " << ex.what();
	}

	{
		vnx::Handle<vnx::Terminal> module = new vnx::Terminal("Terminal");
		module.start_detached();
	}
	{
		vnx::Handle<vnx::Server> module = new vnx::Server("Server", vnx::Endpoint::from_url(endpoint));
		module.start_detached();
	}
	{
		vnx::Handle<vnx::Server> module = new vnx::Server("PublicServer", vnx::Endpoint::from_url(public_endpoint));
		module.start_detached();
	}
	{
		vnx::Handle<vnx::addons::HttpServer> module = new vnx::addons::HttpServer("HttpServer");
		module->components["/api/node/"] = "Node";
		module->components["/api/wallet/"] = "Wallet";
		module.start_detached();
	}
	{
		vnx::Handle<mmx::Router> module = new mmx::Router("Router");
		module->storage_path = root_path + module->storage_path;
		module.start_detached();
	}
	if(with_timelord) {
		vnx::Handle<mmx::TimeLord> module = new mmx::TimeLord("TimeLord");
		module.start_detached();
	}
	if(with_farmer || with_wallet) {
		vnx::Handle<mmx::Wallet> module = new mmx::Wallet("Wallet");
		module.start_detached();
	}
	if(with_farmer) {
		vnx::Handle<mmx::Farmer> module = new mmx::Farmer("Farmer");
		module.start_detached();
	}
	if(with_farmer) {
		vnx::Handle<mmx::Harvester> module = new mmx::Harvester("Harvester");
		module.start_detached();
	}

	while(vnx::do_run())
	{
		vnx::Handle<mmx::Node> module = new mmx::Node("Node");
		module->storage_path = root_path + module->storage_path;
		module.start();
		module.wait();
	}
	vnx::close();

	mmx::secp256k1_free();
	automy::basic_opencl::release_context();

	return 0;
}


