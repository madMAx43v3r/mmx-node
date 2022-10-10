/*
 * calc_test_rewards.cpp
 *
 *  Created on: Oct 10, 2022
 *      Author: mad
 */

#include <mmx/BlockHeader.hxx>
#include <mmx/ProofOfSpaceOG.hxx>

#include <vnx/vnx.h>

using namespace mmx;


int main(int argc, char** argv)
{
	std::map<std::string, std::string> options;
	options["f"] = "file";
	options["s"] = "start";
	options["e"] = "end";
	options["j"] = "json";
	options["r"] = "reward";
	options["file"] = "block_chain.dat";
	options["start"] = "height";
	options["end"] = "height";
	options["reward"] = "MMX";

	vnx::init("calc_test_rewards", argc, argv, options);

	std::string file_path;
	uint32_t start_height = 25000;
	uint32_t end_height = -1;
	double reward = 0.25;
	bool json = false;
	vnx::read_config("file", file_path);
	vnx::read_config("start", start_height);
	vnx::read_config("end", end_height);
	vnx::read_config("reward", reward);
	vnx::read_config("json", json);

	vnx::File block_chain(file_path);
	block_chain.open("rb");

	std::map<addr_t, size_t> reward_count;

	while(auto value = vnx::read(block_chain.in))
	{
		if(auto block = std::dynamic_pointer_cast<const BlockHeader>(value))
		{
			if(block->height >= start_height
				&& block->height <= end_height
				&& std::dynamic_pointer_cast<const ProofOfSpaceOG>(block->proof))
			{
				if(auto tx = block->tx_base)
				{
					if(tx->outputs.size() > 1) {
						reward_count[tx->outputs[1].address]++;
					}
				}
			}
			while(auto value = vnx::read(block_chain.in));
		}
	}

	if(json) {
		std::map<addr_t, uint64_t> amounts;
		for(const auto& entry : reward_count) {
			amounts[entry.first] = entry.second * reward * 1e6;
		}
		std::cout << vnx::to_string(amounts) << std::endl;
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


