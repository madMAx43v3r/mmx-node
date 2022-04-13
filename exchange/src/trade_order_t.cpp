/*
 * trade_order_t.cpp
 *
 *  Created on: Jan 26, 2022
 *      Author: mad
 */

#include <mmx/exchange/trade_order_t.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace exchange {

hash_t trade_order_t::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", version);
	write_field(out, "bid", bid);
	write_field(out, "ask", ask);
	write_field(out, "bid_keys");
	for(const auto& key : bid_keys) {
		write_bytes(out, key);
	}
	out.flush();

	return hash_t(buffer);
}


} // exchange
} // mmx
