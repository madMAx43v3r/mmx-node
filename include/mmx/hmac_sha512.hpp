/*
 * hmac_sha512.h
 *
 *  Created on: Sep 11, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_HMAC_SHA512_H_
#define INCLUDE_MMX_HMAC_SHA512_H_

#include <mmx/hash_t.hpp>
#include <vnx/Input.h>

#include <hmac_sha512.h>
#include <utility>


namespace mmx {

inline std::pair<hash_t, hash_t> hmac_sha512(const hash_t& seed, const hash_t& key)
{
	HMAC_SHA512 func(key.data(), key.size());
	func.Write(seed.data(), seed.size());

	uint8_t tmp[64] = {};
	func.Finalize(tmp);

	std::pair<hash_t, hash_t> out;
	::memcpy(out.first.data(), tmp, 32);
	::memcpy(out.second.data(), tmp + 32, 32);
	return out;
}

inline std::pair<hash_t, hash_t> hmac_sha512_n(const hash_t& seed, const hash_t& key, const uint32_t index)
{
	HMAC_SHA512 func(key.data(), key.size());
	func.Write(seed.data(), seed.size());
	{
		const auto bindex = vnx::flip_bytes(index);
		func.Write((const uint8_t*)&bindex, sizeof(bindex));
	}
	uint8_t tmp[64] = {};
	func.Finalize(tmp);

	std::pair<hash_t, hash_t> out;
	::memcpy(out.first.data(), tmp, 32);
	::memcpy(out.second.data(), tmp + 32, 32);
	return out;
}

inline std::pair<hash_t, hash_t> pbkdf2_hmac_sha512(const hash_t& seed, const hash_t& key, const uint32_t num_iters)
{
	uint8_t tmp[64] = {};

	HMAC_SHA512 func(key.data(), key.size());
	func.Write(seed.data(), seed.size());
	func.Finalize(tmp);

	for(uint32_t i = 1; i < num_iters; ++i) {
		HMAC_SHA512 func(tmp, sizeof(tmp));
		func.Write(seed.data(), seed.size());
		func.Finalize(tmp);
	}
	std::pair<hash_t, hash_t> out;
	::memcpy(out.first.data(), tmp, 32);
	::memcpy(out.second.data(), tmp + 32, 32);
	return out;
}


} // mmx

#endif /* INCLUDE_MMX_HMAC_SHA512_H_ */
