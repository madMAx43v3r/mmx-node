/*
 * chiapos.cpp
 *
 *  Created on: Dec 9, 2021
 *      Author: mad
 */

#include <picosha2.hpp>
#include <verifier.hpp>

#include <mmx/chiapos.h>


namespace mmx {
namespace chiapos {

std::array<uint8_t, 32> verify(	const uint8_t ksize,
								const std::array<uint8_t, 32>& plot_id,
								const std::array<uint8_t, 32>& challenge,
								const void* proof_bytes,
								const size_t proof_size)
{
	Verifier verifier;
	const auto bits = verifier.ValidateProof(
			plot_id.data(), ksize, challenge.data(),
			(const uint8_t*)proof_bytes, proof_size);

	if(bits.GetSize() == 0) {
		throw std::logic_error("invalid proof");
	}
	std::array<uint8_t, 32> quality;
	if(bits.GetSize() != quality.size() * 8) {
		throw std::logic_error("quality size mismatch");
	}
	bits.ToBytes(quality.data());
	return quality;
}


} // chiapos
} // mmx
