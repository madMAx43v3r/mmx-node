/*
 * limit_order_t.cpp
 *
 *  Created on: Jan 26, 2022
 *      Author: mad
 */

#include <mmx/exchange/limit_order_t.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace exchange {

hash_t limit_order_t::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_bytes(out, version);
	for(const auto& entry : bids) {
		write_bytes(out, entry.first);
		write_bytes(out, entry.second);
	}
	out.flush();

	return hash_t(buffer);
}


} // exchange
} // mmx
