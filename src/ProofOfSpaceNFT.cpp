/*
 * ProofOfSpaceNFT.cpp
 *
 *  Created on: Apr 13, 2022
 *      Author: mad
 */

#include <mmx/ProofOfSpaceNFT.hxx>
#include <mmx/write_bytes.h>

#include <bls.hpp>


namespace mmx {

vnx::bool_t ProofOfSpaceNFT::is_valid() const
{
	return Super::is_valid() && ksize > 0 && proof_bytes.size() <= 512;
}

mmx::hash_t ProofOfSpaceNFT::calc_hash(const vnx::bool_t& full_hash) const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	buffer.reserve(4 * 1024);

	write_bytes(out, get_type_hash());
	write_field(out, "version", 	version);
	write_field(out, "ksize", 		ksize);
	write_field(out, "score", 		score);
	write_field(out, "plot_id", 	plot_id);
	write_field(out, "proof_bytes", proof_bytes);
	write_field(out, "plot_key", 	plot_key);
	write_field(out, "contract", 	contract);
	out.flush();

	return hash_t(buffer);
}

void ProofOfSpaceNFT::validate() const
{
	const uint32_t port = 11337;
	if(hash_t(hash_t(contract + plot_key) + bytes_t<4>(&port, sizeof(port))) != plot_id) {
		throw std::logic_error("invalid proof keys or port");
	}
}


} // mmx
