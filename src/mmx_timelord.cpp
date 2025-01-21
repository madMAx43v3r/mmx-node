/*
 * mmx_timelord.cpp
 *
 *  Created on: Dec 6, 2021
 *      Author: mad
 */

#include <mmx/TimeLord.h>
#include <sha256_ni.h>
#include <sha256_arm.h>

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

	vnx::init("mmx_timelord", argc, argv, options);

	std::string node_url = ":11330";

	vnx::read_config("node", node_url);

	vnx::log_info() << "SHA-NI support: " << (sha256_ni_available() ? "yes" : "no");
#ifdef __aarch64__
	vnx::log_info() << "ARM-SHA2 support: " << (sha256_arm_available() ? "yes" : "no");
#endif // __aarch64__

	auto node = vnx::Endpoint::from_url(node_url);
	if(auto tcp = std::dynamic_pointer_cast<const vnx::TcpEndpoint>(node)) {
		if(!tcp->port || tcp->port == vnx::TcpEndpoint::default_port) {
			auto tmp = vnx::clone(tcp);
			tmp->port = 11330;
			node = tmp;
		}
	}
	vnx::Handle<vnx::Proxy> proxy = new vnx::Proxy("Proxy", node);
	proxy->forward_list = {"Node", "Wallet"};

	{
		vnx::Handle<vnx::Server> module = new vnx::Server("Server", vnx::Endpoint::from_url("localhost:11332"));
		module->use_authentication = true;
		module->default_access = "REMOTE";
		module.start_detached();
	}
	{
		vnx::Handle<vnx::Terminal> module = new vnx::Terminal("Terminal");
		module.start_detached();
	}
	{
		vnx::Handle<mmx::TimeLord> module = new mmx::TimeLord("TimeLord");
		module->storage_path = mmx_network + module->storage_path;
		proxy->import_list.push_back(module->input_request);
		proxy->export_list.push_back(module->output_proofs);
		module.start_detached();
	}

	proxy.start();

	vnx::wait();

	mmx::secp256k1_free();

	return 0;
}

