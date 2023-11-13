/*
 * hash_512_t.cpp
 *
 *  Created on: Nov 13, 2023
 *      Author: mad
 */

#include <mmx/hash_512_t.hpp>

#include <sha512.h>
#include <sodium.h>


namespace mmx {

hash_512_t::hash_512_t(const void* data, const size_t num_bytes)
{
	SHA512 hash;
	hash.Write((const uint8_t*)data, num_bytes);
	hash.Finalize(this->data());
}

hash_512_t hash_512_t::random()
{
	std::vector<uint8_t> seed(4096);
	::randombytes_buf(seed.data(), seed.size());
	return hash_512_t(seed);
}


} // mmx
