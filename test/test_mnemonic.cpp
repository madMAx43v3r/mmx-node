/*
 * test_mnemonic.cpp
 *
 *  Created on: Sep 12, 2022
 *      Author: mad
 */

#include <mmx/mnemonic.h>

#include <iostream>

using namespace mmx;

void test_both(const hash_t& seed)
{
	const auto words = mnemonic::seed_to_words(seed);
	const auto res = mnemonic::words_to_seed(words);
	if(res != seed) {
		throw std::logic_error("conversion failed");
	}
}


int main(int argc, char** argv)
{
	std::cout << vnx::to_string(mnemonic::seed_to_words(hash_t())) << std::endl;
	std::cout << vnx::to_string(mnemonic::seed_to_words(hash_t("test"))) << std::endl;

	for(int i = 0; i < 10000; ++i) {
		const auto k = ::rand();
		test_both(hash_t(&k, sizeof(k)));
	}
}

