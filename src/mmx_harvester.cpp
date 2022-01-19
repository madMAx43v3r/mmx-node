/*
 * mmx_harvester.cpp
 *
 *  Created on: Jan 7, 2022
 *      Author: mad
 */

#include <mmx/Harvester.h>

#include <vnx/vnx.h>
#include <vnx/Proxy.h>
#include <vnx/Server.h>
#include <vnx/Terminal.h>


int main(int argc, char** argv)
{
	std::map<std::string, std::string> options;
	options["n"] = "node";
	options["node"] = "address";

	vnx::init("mmx_harvester", argc, argv, options);

	std::string node_url = ":11330";
	std::string endpoint = ":11333";

	vnx::read_config("node", node_url);
	vnx::read_config("endpoint", endpoint);

	vnx::Handle<vnx::Proxy> proxy = new vnx::Proxy("Proxy", vnx::Endpoint::from_url(node_url));
	proxy->forward_list = {"Node", "Farmer"};

	{
		vnx::Handle<vnx::Server> module = new vnx::Server("Server", vnx::Endpoint::from_url(endpoint));
		module.start_detached();
	}
	{
		vnx::Handle<vnx::Terminal> module = new vnx::Terminal("Terminal");
		module.start_detached();
	}
	{
		vnx::Handle<mmx::Harvester> module = new mmx::Harvester("Harvester");
		proxy->import_list.push_back(module->input_challenges);
		proxy->export_list.push_back(module->output_info);
		proxy->export_list.push_back(module->output_proofs);
		module.start_detached();
	}

	proxy.start();

	vnx::wait();

	return 0;
}

