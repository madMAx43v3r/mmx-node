/*
 * trade_pair_t.cpp
 *
 *  Created on: Mar 24, 2022
 *      Author: mad
 */

#include <mmx/exchange/trade_pair_t.hxx>


namespace mmx {
namespace exchange {

trade_pair_t trade_pair_t::reverse() const
{
	trade_pair_t pair;
	pair.ask = bid;
	pair.bid = ask;
	if(price) {
		pair.price = 1 / (*price);
	}
	return pair;
}

trade_pair_t trade_pair_t::create_ex(const addr_t& bid, const addr_t& ask)
{
	trade_pair_t pair;
	pair.bid = bid;
	pair.ask = ask;
	return pair;
}


} // exchange
} // mmx
