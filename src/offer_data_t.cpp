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

uint64_t offer_data_t::get_trade_amount(const uint64_t& amount) const
{
	const uint256_t trade_amount = (uint256_t(amount) * inv_price) >> 64;
	if(trade_amount.upper() || trade_amount.lower().upper()) {
		throw std::logic_error("get_trade_amount(): trade amount overflow");
	}
	return trade_amount;
}


} // mmx
