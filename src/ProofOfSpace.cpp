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

	buffer.reserve(4 * 1024);

	// TODO: write_bytes(out, get_type_hash());
	// TODO: write_bytes(out, version);
	write_bytes(out, ksize);
	write_bytes(out, plot_id);
	write_bytes(out, proof_bytes);
	write_bytes(out, local_key);
	write_bytes(out, farmer_key);
	write_bytes(out, pool_key);
	out.flush();

	return hash_t(buffer);
}


} // mmx
