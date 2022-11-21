/*
 * offer_data_t.cpp
 *
 *  Created on: Oct 12, 2022
 *      Author: mad
 */

#include <mmx/offer_data_t.hxx>


namespace mmx {

vnx::bool_t offer_data_t::is_scam() const
{
	return bid_currency == ask_currency && inv_price < (uint128_t(1) << 64);
}


} // mmx
