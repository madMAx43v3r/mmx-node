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
	return request
			&& harvester.size() < 4096
			&& proof && proof->is_valid()
			&& hash == calc_hash()
			&& content_hash == calc_content_hash();
}

hash_t ProofResponse::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	buffer.reserve(4 * 1024);

	// Note: farmer_addr, harvester and lookup_time_ms are not hashed (local info only)

	write_bytes(out, get_type_hash());
	write_field(out, "request",		request ? request->calc_hash() : hash_t());
	write_field(out, "proof", 		proof ? proof->calc_hash() : hash_t());
	out.flush();

	return hash_t(buffer);
}

hash_t ProofResponse::calc_content_hash() const
{
	return hash_t(hash + farmer_sig);
}

void ProofResponse::validate() const
{
	if(proof) {
		proof->validate();

		if(!farmer_sig.verify(proof->farmer_key, hash)) {
			throw std::logic_error("invalid farmer signature");
		}
	}
}


} // mmx
