/*
 * Block.cpp
 *
 *  Created on: Nov 27, 2021
 *      Author: mad
 */

#include <mmx/Block.hxx>
#include <mmx/write_bytes.h>

#include <vnx/Output.hpp>


namespace mmx {

void Block::finalize()
{
	hash = calc_hash();
}

vnx::bool_t Block::is_valid() const
{
	return calc_hash() == hash;
}

mmx::hash_t Block::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	buffer.reserve(1024 * 1024);

	write_bytes(out, prev);
	write_bytes(out, version);
	write_bytes(out, time_ms);
	write_bytes(out, next_diff);

	if(auto tx = coin_base) {
		write_bytes(out, tx->calc_hash());
	}
	for(const auto& tx : tx_list) {
		write_bytes(out, tx->calc_hash());
	}
	out.flush();

	return hash_t(buffer.data(), buffer.size());
}


} // mmx
