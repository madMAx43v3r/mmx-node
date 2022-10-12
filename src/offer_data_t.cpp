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
	return bid_currency == ask_currency && bid_amount < ask_amount;
}


} // mmx
