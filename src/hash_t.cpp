/*
 * hash_t.cpp
 *
 *  Created on: Jan 6, 2022
 *      Author: mad
 */

#include <mmx/hash_t.hpp>

#include <bls.hpp>
#include <sodium.h>


namespace mmx {

hash_t::hash_t(const void* data, const size_t num_bytes)
{
	bls::Util::Hash256(bytes.data(), (const uint8_t*)data, num_bytes);
}

hash_t hash_t::random()
{
	std::vector<uint8_t> seed(4096);
	randombytes_buf(seed.data(), seed.size());
	return hash_t(seed);
}


} // mmx
