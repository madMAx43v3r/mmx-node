/*
 * hash_t.cpp
 *
 *  Created on: Jan 6, 2022
 *      Author: mad
 */

#include <mmx/hash_t.hpp>
#include <vnx/SHA256.h>
#include <vnx/Util.h>

#include <uint256_t.h>
#include <sha256_ni.h>

#include <atomic>
#include <thread>
#include <chrono>


namespace mmx {

hash_t::hash_t(const void* data, const size_t num_bytes)
{
	static bool have_sha_ni = sha256_ni_available();
	if(have_sha_ni) {
		sha256_ni(bytes.data(), (const uint8_t*)data, num_bytes);
	} else {
		vnx::SHA256 ctx;
		ctx.update((const uint8_t*)data, num_bytes);
		ctx.finalize(bytes.data());
	}
}

hash_t hash_t::random()
{
	hash_t out;
	vnx::secure_random_bytes(out.data(), out.size());
	return out;
}

hash_t hash_t::secure_random()
{
	uint256_t entropy = 0;
	static constexpr int BITS_PER_ITER = 4;
	for(int i = 0; i < 256 / BITS_PER_ITER; ++i)
	{
		entropy <<= BITS_PER_ITER;
		std::atomic_bool trigger {false};

		std::thread timer([&trigger]() {
			std::this_thread::sleep_for(std::chrono::microseconds(10));
			trigger = true;
		});
		while(!trigger) {
			entropy++;
		}
		timer.join();
	}
	const bytes_t<32> entropy_bytes(&entropy, sizeof(entropy));

	bytes_t<128> rand_bytes;
	vnx::secure_random_bytes(rand_bytes.data(), rand_bytes.size());

	return hash_t(entropy_bytes + rand_bytes);
}


} // mmx
