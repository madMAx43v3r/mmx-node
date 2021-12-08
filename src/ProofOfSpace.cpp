/*
 * ProofOfSpace.cpp
 *
 *  Created on: Nov 30, 2021
 *      Author: mad
 */

#include <mmx/ProofOfSpace.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

mmx::hash_t ProofOfSpace::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	buffer.reserve(1024);

	write_bytes(out, ksize);
	write_bytes(out, proof_bytes);
	out.flush();

	return hash_t(buffer);
}


} // mmx
