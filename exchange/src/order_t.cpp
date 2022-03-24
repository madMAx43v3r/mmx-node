/*
 * order_t.cpp
 *
 *  Created on: Mar 24, 2022
 *      Author: mad
 */

#include <mmx/exchange/order_t.hxx>


namespace mmx {
namespace exchange {

double order_t::get_price() const {
	return ask / double(bid);
}


} // exchange
} // mmx
