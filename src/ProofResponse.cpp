/*
 * ProofResponse.cpp
 *
 *  Created on: Apr 1, 2022
 *      Author: mad
 */

#include <mmx/ProofResponse.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

bool ProofResponse::is_valid() const
{
	return request && proof && proof->is_valid() && calc_hash() == hash && calc_hash(true) == content_hash;
}

mmx::hash_t ProofResponse::calc_hash(const vnx::bool_t& full_hash) const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	buffer.reserve(4 * 1024);

	write_bytes(out, get_type_hash());
	write_field(out, "request",		request ? request->calc_hash() : hash_t());
	write_field(out, "proof", 		proof ? proof->calc_hash(true) : hash_t());
	write_field(out, "farmer_addr",	farmer_addr);
	// TODO: harvester, lookup_time_ms

	if(full_hash) {
		write_field(out, "farmer_sig", farmer_sig);
	}
	out.flush();

	return hash_t(buffer);
}

void ProofResponse::validate() const
{
	if(proof) {
		if(!farmer_sig.verify(proof->farmer_key, hash)) {
			throw std::logic_error("invalid farmer signature");
		}
	}
}


} // mmx
