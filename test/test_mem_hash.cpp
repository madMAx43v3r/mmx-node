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
	const int N = 16;
	const int M = 11;
	const int B = 8;

	const int num_iter = argc > 1 ? std::atoi(argv[1]) : 1;

	const uint64_t mem_size = uint64_t(N) << B;

	std::cout << "N = " << N << std::endl;
	std::cout << "M = " << M << std::endl;
	std::cout << "B = " << B << std::endl;
	std::cout << "mem_size = " << mem_size << " (" << mem_size * 4 / 1024 << " KiB)" << std::endl;

	uint32_t* mem = new uint32_t[mem_size];

	for(int iter = 0; iter < num_iter; ++iter)
	{
		uint8_t key[32] = {};
		::memcpy(key, &iter, sizeof(iter));

		gen_mem_array(mem, key, mem_size);

		if(iter == 0) {
			std::map<uint32_t, uint32_t> init_count;

			for(int k = 0; k < (1 << B); ++k) {
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
		mmx::bytes_t<64> hash;

		calc_mem_hash(mem, hash.data(), M, B);

		std::cout << "[" << iter << "] " << hash << std::endl;
	}

	delete [] mem;

	return 0;
}


