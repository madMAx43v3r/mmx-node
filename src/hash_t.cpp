/*
 * hash_t.cpp
 *
 *  Created on: Jan 6, 2022
 *      Author: mad
 */

#include <mmx/hash_t.hpp>

#include <bls.hpp>
#include <sodium.h>
#include <sha256_ni.h>


namespace mmx {

hash_t::hash_t(const void* data, const size_t num_bytes)
{
	static bool have_sha_ni = sha256_ni_available();
	if(have_sha_ni) {
		sha256_ni(bytes.data(), (const uint8_t*)data, num_bytes);
	} else {
		bls::Util::Hash256(bytes.data(), (const uint8_t*)data, num_bytes);
	}
}

hash_t hash_t::random()
{
	std::vector<uint8_t> seed(4096);
	::randombytes_buf(seed.data(), seed.size());
	return hash_t(seed);
}


} // mmx
