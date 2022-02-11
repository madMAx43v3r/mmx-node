/*
 * LocalTrade.cpp
 *
 *  Created on: Feb 11, 2022
 *      Author: mad
 */

#include <mmx/exchange/LocalTrade.hxx>
#include <mmx/exchange/trade_pair_t.hpp>


namespace mmx {
namespace exchange {

std::shared_ptr<const LocalTrade> LocalTrade::reverse() const
{
	auto out = std::make_shared<LocalTrade>(*this);
	out->pair = pair.reverse();
	out->type = (type == trade_type_e::BUY ? trade_type_e::SELL : trade_type_e::BUY);
	out->ask = bid;
	out->bid = ask;
	return out;
}


} // exchange
} // mmx
