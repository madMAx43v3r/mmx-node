/*
 * swap_info_t.cpp
 *
 *  Created on: Nov 3, 2022
 *      Author: mad
 */

#include <mmx/swap_info_t.hxx>

static constexpr int FRACT_BITS = 64;
static const uint256_t max_fee_rate = (uint256_t(1) << FRACT_BITS) / 4;
static const uint256_t min_fee_rate = (uint256_t(1) << FRACT_BITS) / 2048;


namespace mmx {

vnx::float64_t swap_info_t::get_price() const
{
	return balance[1].to_double() / balance[0].to_double();
}

uint64_t swap_info_t::get_trade_amount(const uint32_t& i, const uint64_t& amount) const
{
	if(i >= 2) {
		throw std::logic_error("i >= 2");
	}
	const auto k = (i + 1) % 2;
	const uint256_t trade_amount = (amount * balance[k]) / (balance[i] + amount);
	if(trade_amount < 4) {
		throw std::logic_error("trade amount < 4");
	}
	const uint256_t fee_rate = std::max((trade_amount * max_fee_rate) / balance[k], min_fee_rate);
	const uint256_t fee_amount = std::max((trade_amount * fee_rate) >> FRACT_BITS, uint256_t(1));

	const auto actual_amount = std::min<uint128>(trade_amount - fee_amount, wallet[k]);
	if(actual_amount.upper()) {
		throw std::logic_error("trade amount overflow");
	}
	return actual_amount;
}

std::array<uint64_t, 2> swap_info_t::get_earned_fees(const swap_user_info_t& user) const
{
	std::array<uint128_t, 2> result = {};
	for(int i = 0; i < 2; ++i) {
		if(user.balance[i]) {
			const uint256_t total_fees = fees_paid[i] - user.last_fees_paid[i];
			result[i] = ((2 * total_fees * user.balance[i]) / (user_total[i] + user.last_user_total[i])).lower();
			result[i] = std::min<uint128>(result[i], wallet[i] - balance[i]);
		}
	}
	for(int i = 0; i < 2; ++i) {
		if(result[i].upper()) {
			throw std::logic_error("amount overflow");
		}
	}
	return {result[0], result[1]};
}

std::array<uint64_t, 2> swap_info_t::get_remove_amount(const swap_user_info_t& user, const std::array<uint64_t, 2>& amount) const
{
	std::array<uint128_t, 2> result = {};
	for(int i = 0; i < 2; ++i) {
		if(amount[i] > user.balance[i]) {
			throw std::logic_error("amount > user balance");
		}
		if(balance[i] < user_total[i]) {
			const int k = (i + 1) % 2;
			result[i] += (uint256_t(amount[i]) * balance[i]) / user_total[i];
			if(balance[k] > user_total[k]) {
				result[k] += (uint256_t(balance[k] - user_total[k]) * amount[i]) / user_total[i];
			}
		} else {
			result[i] += amount[i];
		}
	}
	for(int i = 0; i < 2; ++i) {
		if(result[i].upper()) {
			throw std::logic_error("amount overflow");
		}
	}
	return {result[0], result[1]};
}


} // mmx
