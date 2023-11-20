/*
 * ProofOfSpaceNFT.cpp
 *
 *  Created on: Apr 13, 2022
 *      Author: mad
 */

#include <mmx/ProofOfSpaceNFT.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

vnx::bool_t ProofOfSpaceNFT::is_valid() const
{
	return Super::is_valid() && ksize > 0 && proof_xs.size() <= 1024;
}

mmx::hash_t ProofOfSpaceNFT::calc_hash(const vnx::bool_t& full_hash) const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	buffer.reserve(4 * 1024);

	write_bytes(out, get_type_hash());
	write_field(out, "score", 		score);
	write_field(out, "plot_id", 	plot_id);
	write_field(out, "farmer_key", 	farmer_key);
	write_field(out, "ksize", 		ksize);
	write_field(out, "seed", 		seed);
	write_field(out, "proof_xs", 	proof_xs);
	write_field(out, "contract", 	contract);
	out.flush();

	return hash_t(buffer);
}

void ProofOfSpaceNFT::validate() const
{
	const hash_t id(std::string("MMX/PLOTID/NFT") + bytes_t<1>(&ksize, 1) + seed + farmer_key + contract);

	if(id != plot_id) {
		throw std::logic_error("invalid plot id");
	}
}


} // mmx
