/*
 * calc_test_rewards.cpp
 *
 *  Created on: Oct 10, 2022
 *      Author: mad
 */

#include <mmx/BlockHeader.hxx>
#include <mmx/Transaction.hxx>
#include <mmx/ProofOfSpaceOG.hxx>
#include <mmx/NodeClient.hxx>

#include <vnx/vnx.h>
#include <vnx/Proxy.h>

using namespace mmx;


int main(int argc, char** argv)
{
	std::map<std::string, std::string> options;
	options["s"] = "start";
	options["e"] = "end";
	options["j"] = "json";
	options["r"] = "reward";
	options["start"] = "height";
	options["end"] = "height";
	options["reward"] = "MMX";
	options["timelord"] = "TL rewards";

	vnx::init("calc_test_rewards", argc, argv, options);

	uint32_t start_height = 25000;
	uint32_t end_height = -1;
	double reward = 0;
	bool json = false;
	bool timelord = false;
	vnx::read_config("start", start_height);
	vnx::read_config("end", end_height);
	vnx::read_config("reward", reward);
	vnx::read_config("json", json);
	vnx::read_config("timelord", timelord);

	if(reward <= 0) {
		if(timelord) {
			reward = 0.01;
		} else {
			reward = 0.5;
		}
	}

	vnx::Handle<vnx::Proxy> proxy = new vnx::Proxy("Proxy", vnx::Endpoint::from_url(":11330"));
	proxy->forward_list = {"Node"};
	proxy.start();

	NodeClient node("Node");

	std::map<addr_t, size_t> reward_count;

	for(uint32_t height = start_height; height <= end_height; ++height)
	{
		const auto block = node.get_header_at(height);
		if(!block) {
			break;
		}
		if(timelord) {
			for(auto addr : block->vdf_reward_addr) {
				reward_count[addr]++;
			}
		} else {
			if(auto addr = block->reward_addr) {
				reward_count[*addr]++;
			}
		}
	}

	if(json) {
		std::cout << vnx::to_string(reward_count) << std::endl;
	}
	else {
		std::vector<std::pair<addr_t, size_t>> sorted(reward_count.begin(), reward_count.end());
		std::sort(sorted.begin(), sorted.end(),
			[](const std::pair<addr_t, size_t>& L, const std::pair<addr_t, size_t>& R) -> bool {
				return L.second > R.second;
			});
		for(const auto& entry : sorted) {
			std::cout << entry.first << "\t" << entry.second << "\t" << entry.second * reward << std::endl;
		}
	}

	vnx::close();

	return 0;
}


