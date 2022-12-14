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

vnx::bool_t offer_data_t::is_open() const
{
	return bid_balance > 0;
}

uint64_t offer_data_t::get_bid_amount(const uint64_t& ask_amount) const
{
	const auto bid_amount = (uint256_t(ask_amount) * inv_price) >> 64;
	if(bid_amount.upper() || bid_amount.lower().upper()) {
		throw std::logic_error("get_bid_amount(): bid amount overflow");
	}
	return bid_amount;
}

uint64_t offer_data_t::get_ask_amount(const uint64_t& bid_amount) const
{
	const auto ask_amount = ((uint256_t(bid_amount) << 64) + inv_price - 1) / inv_price;
	if(ask_amount.upper() || ask_amount.lower().upper()) {
		throw std::logic_error("get_ask_amount(): ask amount overflow");
	}
	return ask_amount;
}


} // mmx
