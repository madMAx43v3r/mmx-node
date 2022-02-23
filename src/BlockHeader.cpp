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
	write_bytes(out, version);
	write_bytes(out, prev);
	write_bytes(out, height);
	write_bytes(out, time_diff);
	write_bytes(out, space_diff);
	write_bytes(out, vdf_iters);
	write_bytes(out, vdf_output);
	write_bytes(out, proof ? proof->calc_hash() : hash_t());
	write_bytes(out, tx_base ? tx_base->calc_hash() : hash_t());
	write_bytes(out, tx_hash);
	out.flush();

	return hash_t(buffer);
}


} // mmx
