/*
 * tx_bench.cpp
 *
 *  Created on: Dec 30, 2021
 *      Author: mad
 */

#include <mmx/NodeClient.hxx>
#include <mmx/WalletClient.hxx>

#include <vnx/vnx.h>
#include <vnx/Proxy.h>


int main(int argc, char** argv)
{
	std::map<std::string, std::string> options;
	options["n"] = "node";
	options["s"] = "speed";
	options["j"] = "index";
	options["a"] = "amount";
	options["t"] = "target";
	options["x"] = "contract";
	options["node"] = "address";
	options["speed"] = "TPS";
	options["index"] = "0..?";
	options["amount"] = "123.456";
	options["target"] = "address";
	options["contract"] = "address";

	vnx::write_config("log_level", 2);

	vnx::init("tx_bench", argc, argv, options);

	std::string node_url = ":11331";
	std::string target_addr;
	std::string contract_addr;
	int64_t index = 0;
	int64_t amount = 1000;
	double speed = 1;
	vnx::read_config("node", node_url);
	vnx::read_config("speed", speed);
	vnx::read_config("index", index);
	vnx::read_config("amount", amount);
	vnx::read_config("target", target_addr);
	vnx::read_config("contract", contract_addr);

	const int64_t interval = 1e6 / speed;

	std::cout << "Sending amount " << amount << " at speed of " << speed << " TX/s" << std::endl;

	mmx::NodeClient node("Node");
	mmx::WalletClient wallet("Wallet");

	mmx::addr_t target;
	mmx::addr_t contract;
	if(!target_addr.empty()) {
		target.from_string(target_addr);
	}
	if(!contract_addr.empty()) {
		contract.from_string(contract_addr);
	}
	{
		vnx::Handle<vnx::Proxy> module = new vnx::Proxy("Proxy", vnx::Endpoint::from_url(node_url));
		module->forward_list = {"Wallet", "Node"};
		module.start_detached();
	}

	size_t total = 0;
	size_t counter = 0;
	int64_t time_next = vnx::get_wall_time_micros();
	int64_t last_info = time_next;
	while(true)
	{
		try {
			wallet.send(index, amount, target, contract);
			counter++;
			total++;
		} catch(...) {
			if(vnx::do_run()) {
				throw;
			}
		}
		if(total % 1000 == 0 || !vnx::do_run())
		{
			const auto now = vnx::get_wall_time_micros();
			const auto actual = (counter * 1e6) / (now - last_info);
			std::cout << "Sent " << total << " total so far, speed = " << actual << " TX/s" << std::endl;
			last_info = now;
			counter = 0;
		}
		if(!vnx::do_run()) {
			break;
		}
		time_next += interval;
		const auto left = time_next - vnx::get_wall_time_micros();
		if(left > 0) {
			std::this_thread::sleep_for(std::chrono::microseconds(left));
		}
	}

	vnx::close();

	return 0;
}





