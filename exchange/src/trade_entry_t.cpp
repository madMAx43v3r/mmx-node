/*
 * trade_entry_t.cpp
 *
 *  Created on: Mar 24, 2022
 *      Author: mad
 */

#include <mmx/exchange/trade_entry_t.hxx>


namespace mmx {
namespace exchange {

double trade_entry_t::get_price() const {
	return ask / double(bid);
}


} // exchange
} // mmx
