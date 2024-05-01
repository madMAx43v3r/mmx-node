/*
 * test_encoding.cpp
 *
 *  Created on: Nov 20, 2023
 *      Author: mad
 */

#include <mmx/pos/encoding.h>

#include <iostream>


int main(int argc, char** argv)
{
	const int num_symbols = argc > 1 ? ::atoi(argv[1]) : 4096;

	std::vector<uint8_t> symbols;

	for(int i = 0; i < num_symbols; ++i)
	{
		uint8_t sym = 0;
		const auto ticket = ::rand() % 1000;
		if(ticket < 900) {
			sym = ticket % 3;
		} else if(ticket < 990) {
			sym = 3 + ticket % 3;
		} else {
			sym = 6 + ticket % 3;
		}
		symbols.push_back(sym);
	}

	for(auto sym : symbols) {
		std::cout << int(sym) << " ";
	}
	std::cout << std::endl;

	uint64_t total_bits = 0;
	const auto bit_stream = mmx::pos::encode(symbols, total_bits);

	std::cout << "symbols = " << num_symbols << std::endl;
	std::cout << "bit_stream = " << (total_bits + 7) / 8 << " bytes, " << double(total_bits) / num_symbols << " bits / symbol" << std::endl;

	const auto test = mmx::pos::decode(bit_stream, symbols.size());

	if(test != symbols) {
		for(auto sym : test) {
			std::cout << int(sym) << " ";
		}
		throw std::logic_error("test != symbols");
	}
	return 0;
}


