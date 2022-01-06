/*
 * hash_t.cpp
 *
 *  Created on: Jan 6, 2022
 *      Author: mad
 */

#include <mmx/hash_t.hpp>

#include <sodium.h>


namespace mmx {

hash_t hash_t::random()
{
	std::vector<uint8_t> seed(4096);
	randombytes_buf(seed.data(), seed.size());
	return hash_t(seed);
}


} // mmx
