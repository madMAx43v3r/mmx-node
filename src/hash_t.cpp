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


static bool check_cpuid_sha_ni()
{
	int a, b, c, d;

	// Look for CPUID.7.0.EBX[29]
	// EAX = 7, ECX = 0
	a = 7;
	c = 0;

	asm volatile ("cpuid"
		:"=a"(a), "=b"(b), "=c"(c), "=d"(d)
		:"a"(a), "c"(c)
	);

	// Intel® SHA Extensions feature bit is EBX[29]
	return ((b >> 29) & 1);
}


namespace mmx {

hash_t::hash_t(const void* data, const size_t num_bytes)
{
	static bool have_init = false;
	static bool have_sha_ni = false;
	if(!have_init) {
		have_init = true;
		have_sha_ni = check_cpuid_sha_ni();
	}
	if(have_sha_ni) {
		sha256_ni(bytes.data(), (const uint8_t*)data, num_bytes);
	} else {
		bls::Util::Hash256(bytes.data(), (const uint8_t*)data, num_bytes);
	}
}

hash_t hash_t::random()
{
	std::vector<uint8_t> seed(4096);
	randombytes_buf(seed.data(), seed.size());
	return hash_t(seed);
}


} // mmx
