/*
 * test_pos_compute.cpp
 *
 *  Created on: Nov 6, 2023
 *      Author: mad
 */

#include <mmx/pos/verify.h>
#include <random>

using namespace mmx;


int main(int argc, char** argv)
{
	const int ksize = argc > 1 ? std::atoi(argv[1]) : 32;
	const int xbits = argc > 2 ? std::atoi(argv[2]) : 0;

	std::cout << "ksize = " << ksize << std::endl;
	std::cout << "xbits = " << xbits << std::endl;

	uint8_t id[32] = {};

	std::mt19937_64 generator(1337);
	std::uniform_int_distribution<uint64_t> dist(0, (1 << (ksize - xbits)) - 1);

	std::vector<uint32_t> X_values;
	if(xbits < ksize) {
		for(int i = 0; i < 256; ++i) {
			X_values.push_back(dist(generator));
		}
	} else {
		X_values.push_back(0);
	}

	std::cout << "X = ";
	for(auto X : X_values) {
		std::cout << X << " ";
	}
	std::cout << std::endl;

	std::vector<uint32_t> X_out;
	const auto res = pos::compute(X_values, &X_out, id, ksize, xbits);

	for(const auto& entry : res) {
		std::cout << "Y = " << entry.first << std::endl;
		std::cout << "M = " << entry.second.to_string() << std::endl;
	}
	std::cout << "num_entries = " << res.size() << std::endl;

}


