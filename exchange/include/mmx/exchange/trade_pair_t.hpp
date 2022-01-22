/*
 * trade_pair_t.hpp
 *
 *  Created on: Jan 22, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_EXCHANGE_TRADE_PAIR_T_HPP_
#define INCLUDE_MMX_EXCHANGE_TRADE_PAIR_T_HPP_

#include <mmx/exchange/trade_pair_t.hxx>


namespace mmx {
namespace exchange {

inline
trade_pair_t trade_pair_t::reverse() const
{
	trade_pair_t pair;
	pair.ask = bid;
	pair.bid = ask;
	return pair;
}

inline
trade_pair_t trade_pair_t::create_ex(const addr_t& bid, const addr_t& ask)
{
	trade_pair_t pair;
	pair.bid = bid;
	pair.ask = ask;
	return pair;
}

inline
bool operator==(const trade_pair_t& lhs, const trade_pair_t& rhs) {
	return lhs.bid == rhs.bid && lhs.ask == rhs.ask;
}

inline
bool operator<(const trade_pair_t& lhs, const trade_pair_t& rhs) {
	if(lhs.bid == rhs.bid) {
		return lhs.ask < rhs.ask;
	}
	return lhs.bid < rhs.bid;
}


} // exchange
} // mmx

#endif /* INCLUDE_MMX_EXCHANGE_TRADE_PAIR_T_HPP_ */
