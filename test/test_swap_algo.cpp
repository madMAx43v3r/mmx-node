/*
 * test_swap_algo.cpp
 *
 *  Created on: Oct 26, 2022
 *      Author: mad
 */

#include <uint256_t.h>

constexpr int FRACT_BITS = 20;


class User {
public:
	uint256_t wallet[2] = {0, 0};
	uint256_t balance[2] = {0, 0};
	uint256_t last_pool_balance[2] = {0, 0};
	uint256_t last_trade_volume[2] = {0, 0};

};

class Swap {
public:
	uint256_t ratio[2] = {0, 0};
	uint256_t balance[2] = {0, 0};
	uint256_t trade_volume[2] = {0, 0};
	uint256_t fees_claimed[2] = {0, 0};

	const uint256_t fee_rate = (uint256_t(1) << FRACT_BITS) / 128;

	void add_liquid(User& user, uint256_t amount[2])
	{
		payout(user);

		for(int i = 0; i < 2; ++i) {
			user.balance[i] += amount[i];
		}
		for(int i = 0; i < 2; ++i) {
			balance[i] += amount[i];
		}
		update_ratio();
	}

	void rem_liquid(User& user, uint256_t amount[2])
	{
		for(int i = 0; i < 2; ++i) {
			if(amount[i] > user.balance[i]) {
				throw std::logic_error("amount > user balance");
			}
		}
		uint256_t delta[2];
		delta[0] = (amount[0] * ratio[0] + amount[1] * ratio[0]) >> FRACT_BITS;
		delta[1] = (amount[0] * ratio[1] + amount[1] * ratio[1]) >> FRACT_BITS;

		for(int i = 0; i < 2; ++i) {
			user.wallet[i] += delta[i];
			user.balance[i] -= amount[i];
			balance[i] -= delta[i];
		}
		update_ratio();
	}

	void trade(User& user, const int i, uint256_t amount)
	{
		const int k = (i + 1) % 2;
		if(balance[k] == 0) {
			throw std::logic_error("nothing to buy");
		}
		balance[i] += amount;
		update_ratio();

		const auto trade_amount = (amount * ratio[k]) / ratio[i];
		const auto fee_amount = (trade_amount * fee_rate) >> FRACT_BITS;
		const auto eff_amount = trade_amount - fee_amount;
		user.wallet[k] += eff_amount;

		balance[k] -= eff_amount;
		trade_volume[k] += trade_amount;
		update_ratio();
	}

	void update_ratio()
	{
		const auto sum = balance[0] + balance[1];
		if(sum) {
			for(int i = 0; i < 2; ++i) {
				ratio[i] = (balance[i] << FRACT_BITS) / sum;
			}
		} else {
			ratio[0] = 0;
			ratio[1] = 0;
		}
	}

	void payout(User& user)
	{
		for(int i = 0; i < 2; ++i) {
			if(user.balance[i]) {
				const auto volume = trade_volume[i] - user.last_trade_volume[i];
				const auto total_fee = (volume * fee_rate) >> FRACT_BITS;
				const auto user_share = (total_fee * user.balance[i]) / user.last_pool_balance[i];
				balance[i] -= user_share;
				fees_claimed[i] += user_share;
				user.balance[i] += user_share;
			}
			user.last_pool_balance[i] = balance[i];
			user.last_trade_volume[i] = trade_volume[i];
		}
		update_ratio();
	}

};


int main()
{
	uint256_t A_balance = 0;
	uint256_t B_balance = 0;

	uint256_t UA_balance = 1000;
	uint256_t UA_ratio = (1024 * UA_balance + 0) / 1024;



	return 0;
}

