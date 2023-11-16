/*
 * test_pos_compute.cpp
 *
 *  Created on: Nov 6, 2023
 *      Author: mad
 */

#include <mmx/pos/verify.h>
#include <vnx/vnx.h>
#include <random>

using namespace mmx;


int main(int argc, char** argv)
{
	const int ksize = argc > 1 ? std::atoi(argv[1]) : 32;
	const int xbits = argc > 2 ? std::atoi(argv[2]) : 0;

	vnx::init("test_pos_compute", 0, nullptr);

	std::cout << "ksize = " << ksize << std::endl;
	std::cout << "xbits = " << xbits << std::endl;

	const bool full_mode = (xbits == ksize);

	const hash_t id;

	std::mt19937_64 generator(1337);
	std::uniform_int_distribution<uint64_t> dist(0, (uint64_t(1) << (ksize - xbits)) - 1);

	std::vector<uint32_t> X_values;
	if(!full_mode) {
		for(int i = 0; i < 256; ++i) {
			X_values.push_back(dist(generator));
		}
	} else {
		for(int i = 0; i < 256; ++i) {
			X_values.push_back(i);
		}
	}

	std::cout << "X = ";
	for(auto X : X_values) {
		std::cout << X << " ";
	}
	std::cout << std::endl;

	std::vector<uint32_t> X_out;
	const auto res = pos::compute(X_values, &X_out, id, ksize, full_mode ? ksize - 8 : xbits);

	for(size_t i = 0; i < std::min<size_t>(res.size(), 5); ++i)
	{
		std::cout << "Y = " << res[i].first << std::endl;
		std::cout << "M = " << res[i].second.to_string() << std::endl;
		std::cout << "X = ";
		for(size_t k = 0; k < 256; ++k) {
			std::cout << X_out[i * 256 + k] << " ";
		}
		std::cout << std::endl;
	}
	std::cout << "num_entries = " << res.size() << std::endl;

	vnx::close();
}


