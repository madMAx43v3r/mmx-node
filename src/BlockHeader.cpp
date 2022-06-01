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
	return version == 0 && calc_hash() == hash;
}

hash_t BlockHeader::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	buffer.reserve(64 * 1024);

	write_bytes(out, get_type_hash());
	write_field(out, "version", 	version);
	write_field(out, "prev",		prev);
	write_field(out, "height", 		height);
	write_field(out, "nonce", 		nonce);
	write_field(out, "time_diff", 	time_diff);
	write_field(out, "space_diff", 	space_diff);
	write_field(out, "vdf_iters", 	vdf_iters);
	write_field(out, "vdf_output", 	vdf_output);
	// TODO: use full hash
	write_field(out, "proof", 		proof ? proof->calc_hash() : hash_t());
	// TODO: use full hash
	write_field(out, "tx_base", 	tx_base ? tx_base->calc_hash() : hash_t());
	write_field(out, "tx_hash", 	tx_hash);
	out.flush();

	return hash_t(buffer);
}

hash_t BlockHeader::get_full_hash() const
{
	return farmer_sig ? hash_t(hash + *farmer_sig) : hash;
}

void BlockHeader::validate() const
{
	if(farmer_sig) {
		if(!proof) {
			throw std::logic_error("missing proof");
		}
		if(!farmer_sig->verify(proof->farmer_key, hash)) {
			throw std::logic_error("invalid farmer signature");
		}
	} else {
		// TODO: remove height switch
		if(proof && height > 200000) {
			throw std::logic_error("proof without farmer_sig");
		}
	}
}

std::shared_ptr<const BlockHeader> BlockHeader::get_header() const
{
	return vnx::clone(*this);
}


} // mmx
