/*
 * mmx_timelord_rewards.cpp
 *
 *  Created on: Mar 20, 2022
 *      Author: mad
 */

#include <mmx/TimeLordRewards.h>

#include <vnx/vnx.h>
#include <vnx/Proxy.h>
#include <vnx/Terminal.h>


int main(int argc, char** argv)
{
	mmx::secp256k1_init();

	std::map<std::string, std::string> options;
	options["n"] = "node";
	options["node"] = "address";

	vnx::init("mmx_timelord_rewards", argc, argv, options);

	std::string node_url = ":11331";

	vnx::read_config("node", node_url);

	vnx::Handle<vnx::Proxy> proxy = new vnx::Proxy("Proxy", vnx::Endpoint::from_url(node_url));
	proxy->forward_list = {"Node", "Wallet"};

	{
		vnx::Handle<vnx::Terminal> module = new vnx::Terminal("Terminal");
		module.start_detached();
	}
	{
		vnx::Handle<mmx::TimeLordRewards> module = new mmx::TimeLordRewards("TimeLordRewards");
		proxy->import_list.push_back(module->input_vdfs);
		module.start_detached();
	}

	proxy.start();

	vnx::wait();

	mmx::secp256k1_free();

	return 0;
}
