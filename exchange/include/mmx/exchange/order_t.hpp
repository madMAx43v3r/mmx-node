/*
 * order_t.hpp
 *
 *  Created on: Jan 22, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_EXCHANGE_ORDER_T_HPP_
#define INCLUDE_MMX_EXCHANGE_ORDER_T_HPP_

#include <mmx/exchange/order_t.hxx>


namespace mmx {
namespace exchange {

inline
double order_t::get_price() const {
	return ask / double(bid);
}


} // exchange
} // mmx

#endif /* INCLUDE_MMX_EXCHANGE_ORDER_T_HPP_ */
