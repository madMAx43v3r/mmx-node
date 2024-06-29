/*
 * calc_test_rewards.cpp
 *
 *  Created on: Oct 10, 2022
 *      Author: mad
 */

#include <mmx/BlockHeader.hxx>
#include <mmx/Transaction.hxx>
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
	double reward = 0.5;
	bool json = false;
	vnx::read_config("file", file_path);
	vnx::read_config("start", start_height);
	vnx::read_config("end", end_height);
	vnx::read_config("reward", reward);
	vnx::read_config("json", json);

	vnx::File block_chain(file_path);
	block_chain.open("rb");

	size_t block_count = 0;
	std::map<addr_t, size_t> reward_count;
	std::map<addr_t, std::string> contract_map;
	std::map<pubkey_t, addr_t> farmer_address_map;

	while(auto value = vnx::read(block_chain.in))
	{
		if(auto block = std::dynamic_pointer_cast<const BlockHeader>(value))
		{
			if(block->height >= start_height && block->height <= end_height)
			{
				block_count++;

				if(auto proof = std::dynamic_pointer_cast<const ProofOfSpaceOG>(block->proof))
				{
					if(auto addr = block->reward_addr)
					{
						const auto iter = contract_map.find(*addr);
						if(iter != contract_map.end()) {
							const auto iter2 = farmer_address_map.find(proof->farmer_key);
							if(iter2 != farmer_address_map.end()) {
								std::cout << "[" << block->height << "] Redirected reward sent to " << iter->second << ": " << *addr << " -> " << iter2->second << std::endl;
								addr = iter2->second;
							} else {
								std::cerr << "[" << block->height << "] Lost reward sent to " << iter->second << " at " << *block->reward_addr << std::endl;
								addr = nullptr;
							}
						}
						if(addr) {
							farmer_address_map[proof->farmer_key] = *addr;
							reward_count[*addr] += block_count;
							block_count = 0;
						}
					}
				}
			}

			while(auto value = vnx::read(block_chain.in)) {
				if(auto tx = std::dynamic_pointer_cast<const Transaction>(value)) {
					if(tx->deploy) {
						contract_map[tx->id] = tx->deploy->get_type_name();
					}
				}
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


