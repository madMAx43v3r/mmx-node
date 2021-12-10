/*
 * create_wallet.cpp
 *
 *  Created on: Dec 11, 2021
 *      Author: mad
 */

#include <mmx/KeyFile.hxx>
#include <mmx/secp256k1.hpp>

#include <vnx/vnx.h>

#include <sodium.h>


int main(int argc, char** argv)
{
	vnx::init("create_wallet", argc, argv);

	std::string file_path = "wallet.dat";

	if(vnx::File(file_path).exists())
	{
		std::cerr << "Wallet file already exists." << std::endl;
		vnx::close();
		return -1;
	}

	std::vector<uint8_t> seed(1024);
	randombytes_buf(seed.data(), seed.size());

	mmx::KeyFile wallet;
	wallet.seed_value = mmx::hash_t(seed);

	vnx::write_to_file(file_path, wallet);

	std::cout << "Created wallet '" << file_path << "'" << std::endl;

	vnx::close();

	return 0;
}


