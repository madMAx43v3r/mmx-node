/*
 * mmx_harvester.cpp
 *
 *  Created on: Jan 7, 2022
 *      Author: mad
 */

#include <mmx/Harvester.h>
#include <mmx/pos/verify.h>

#include <vnx/vnx.h>
#include <vnx/Proxy.h>
#include <vnx/Server.h>
#include <vnx/Terminal.h>
#include <vnx/TcpEndpoint.hxx>

#ifdef WITH_CUDA
#include <mmx/pos/cuda_recompute.h>
#endif


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
	vnx::Directory(mmx_network).create();

	vnx::write_config("mmx_harvester.log_file_path", mmx_network + "logs/");

	std::map<std::string, std::string> options;
	options["n"] = "node";
	options["node"] = "address";

	vnx::init("mmx_harvester", argc, argv, options);
	{
		std::string version;
		std::string commit;
		vnx::read_config("build.version", version);
		vnx::read_config("build.commit", commit);
		vnx::log_info() << "Build version: " << version;
		vnx::log_info() << "Build commit: " << commit;
	}

	std::string node_url = ":11333";
	std::string endpoint = "0.0.0.0";		// requires allow_remote
	bool allow_remote = false;
	bool remote_compute = false;

	vnx::read_config("node", node_url);
	vnx::read_config("endpoint", endpoint);
	vnx::read_config("allow_remote", allow_remote);
	vnx::read_config("remote_compute", remote_compute);

#ifdef WITH_CUDA
	vnx::log_info() << "CUDA available: yes";
	mmx::pos::cuda_recompute_init();
#else
	vnx::log_info() << "CUDA available: no";
#endif

	if(!allow_remote) {
		endpoint = "localhost";
	}
	vnx::log_info() << "Remote service access is: " << (allow_remote ? "enabled on " + endpoint : "disabled");

	mmx::pos::set_remote_compute(remote_compute);
	vnx::log_info() << "Remote compute is: " << (remote_compute ? "enabled" : "disabled");

	auto node = vnx::Endpoint::from_url(node_url);
	if(auto tcp = std::dynamic_pointer_cast<const vnx::TcpEndpoint>(node)) {
		if(!tcp->port || tcp->port == vnx::TcpEndpoint::default_port) {
			auto tmp = vnx::clone(tcp);
			tmp->port = 11333;
			node = tmp;
		}
	}
	vnx::Handle<vnx::Proxy> proxy = new vnx::Proxy("Proxy", node);
	proxy->forward_list = {"Node", "Farmer", "ProofServer"};

	{
		vnx::Handle<vnx::Server> module = new vnx::Server("Server", vnx::Endpoint::from_url(endpoint + ":11333"));
		module->use_authentication = true;
		module->default_access = "REMOTE";
		module.start_detached();
	}
	{
		vnx::Handle<vnx::Terminal> module = new vnx::Terminal("Terminal");
		module.start_detached();
	}
	{
		vnx::Handle<mmx::Harvester> module = new mmx::Harvester("Harvester");
		module->config_path = mmx_home + module->config_path;
		module->storage_path = mmx_network + module->storage_path;
		proxy->import_list.push_back(module->input_challenges);
		proxy->export_list.push_back(module->output_info);
		proxy->export_list.push_back(module->output_proofs);
		proxy->export_list.push_back(module->output_lookups);
		proxy->export_list.push_back(module->output_partials);
		module.start_detached();
	}

	proxy.start();

	vnx::wait();

#ifdef WITH_CUDA
	mmx::pos::cuda_recompute_shutdown();
#endif

	return 0;
}

