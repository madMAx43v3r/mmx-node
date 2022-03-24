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
