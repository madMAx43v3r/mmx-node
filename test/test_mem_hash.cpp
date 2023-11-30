/*
 * test_mem_hash.cpp
 *
 *  Created on: Oct 30, 2023
 *      Author: mad
 */

#include <mmx/bytes_t.hpp>
#include <mmx/pos/mem_hash.h>

#include <map>
#include <cstring>
#include <iostream>

using namespace mmx::pos;


int main(int argc, char** argv)
{
	const int N = 32;

	const int count = argc > 1 ? std::atoi(argv[1]) : 1;
	const int num_iter = argc > 2 ? std::atoi(argv[2]) : 256;

	const uint64_t mem_size = uint64_t(N) * N;

	std::cout << "count = " << count << std::endl;
	std::cout << "num_iter = " << num_iter << std::endl;
	std::cout << "mem_size = " << mem_size << " (" << mem_size * 4 / 1024 << " KiB)" << std::endl;

	size_t pop_sum = 0;
	size_t num_pass = 0;
	size_t min_pop_count = 1024;

	uint32_t* mem = new uint32_t[mem_size];

	for(int iter = 0; iter < count; ++iter)
	{
		uint8_t key[32] = {};
		::memcpy(key, &iter, sizeof(iter));

		gen_mem_array(mem, key, 32, mem_size);

		if(iter == 0) {
			std::map<uint32_t, uint32_t> init_count;

			for(int k = 0; k < 32; ++k) {
				std::cout << "[" << k << "] " << std::hex;
				for(int i = 0; i < N; ++i) {
					init_count[mem[k * N + i]]++;
					std::cout << mem[k * N + i] << " ";
				}
				std::cout << std::dec << std::endl;
			}

			for(const auto& entry : init_count) {
				if(entry.second > 1) {
					std::cout << "WARN [" << std::hex << entry.first << std::dec << "] " << entry.second << std::endl;
				}
			}
		}
		mmx::bytes_t<128> hash;

		calc_mem_hash(mem, hash.data(), num_iter);

		size_t pop = 0;
		for(int i = 0; i < 1024; ++i) {
			pop += (hash[i / 8] >> (i % 8)) & 1;
		}
		pop_sum += pop;

		min_pop_count = std::min(min_pop_count, pop);

		if(pop <= 469) {
			num_pass++;
		}

		std::cout << "[" << iter << "] " << hash << " (" << pop << ")" << std::endl;
	}

	std::cout << "num_pass = " << num_pass << " (" << num_pass / double(count) << ")" << std::endl;
	std::cout << "min_pop_count = " << min_pop_count << std::endl;
	std::cout << "avg_pop_count = " << pop_sum / double(count) << std::endl;

	delete [] mem;

	return 0;
}


