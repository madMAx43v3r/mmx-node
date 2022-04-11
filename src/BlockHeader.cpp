/*
 * BlockHeader.cpp
 *
 *  Created on: Nov 30, 2021
 *      Author: mad
 */

#include <mmx/BlockHeader.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

vnx::bool_t BlockHeader::is_valid() const
{
	return calc_hash() == hash;
}

mmx::hash_t BlockHeader::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	buffer.reserve(64 * 1024);

	write_bytes(out, get_type_hash());
	write_field(out, "version", 	version);
	write_field(out, "prev",		prev);
	write_field(out, "height", 		height);
	write_field(out, "time_diff", 	time_diff);
	write_field(out, "space_diff", 	space_diff);
	write_field(out, "vdf_iters", 	vdf_iters);
	write_field(out, "vdf_output", 	vdf_output);
	write_field(out, "proof", 		proof ? proof->calc_hash() : hash_t());
	write_field(out, "tx_base", 	tx_base ? tx_base->calc_hash() : hash_t());
	write_field(out, "tx_hash", 	tx_hash);
	out.flush();

	return hash_t(buffer);
}

void BlockHeader::validate() const
{
	if(proof) {
		if(!farmer_sig) {
			throw std::logic_error("missing farmer signature");
		}
		if(!farmer_sig->verify(proof->farmer_key, hash)) {
			throw std::logic_error("invalid farmer signature");
		}
	} else {
		if(farmer_sig) {
			throw std::logic_error("extra farmer signature");
		}
	}
}


} // mmx
