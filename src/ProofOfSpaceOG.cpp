/*
 * ProofOfSpaceOG.cpp
 *
 *  Created on: Nov 30, 2021
 *      Author: mad
 */

#include <mmx/ProofOfSpaceOG.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

vnx::bool_t ProofOfSpaceOG::is_valid() const
{
	return Super::is_valid() && ksize > 0 && proof_xs.size() <= 1024;
}

mmx::hash_t ProofOfSpaceOG::calc_hash(const vnx::bool_t& full_hash) const
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
	out.flush();

	return hash_t(buffer);
}

void ProofOfSpaceOG::validate() const
{
	const hash_t id(std::string("MMX/PLOTID/OG") + bytes_t<1>(&ksize, 1) + seed + farmer_key);

	if(id != plot_id) {
		throw std::logic_error("invalid plot id");
	}
}


} // mmx
