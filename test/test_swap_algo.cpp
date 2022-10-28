/*
 * test_swap_algo.cpp
 *
 *  Created on: Oct 26, 2022
 *      Author: mad
 */

#include <array>
#include <iostream>
#include <vnx/vnx.h>
#include <uint256_t.h>

constexpr int FRACT_BITS = 20;


class User {
public:
	std::array<uint256_t, 2> wallet = {0, 0};
	std::array<uint256_t, 2> balance = {0, 0};

	std::array<uint256_t, 2> last_user_total = {0, 0};		// user total at last payout
	std::array<uint256_t, 2> last_fees_paid = {0, 0};		// pool fees at last payout

};

class Swap {
public:
	std::array<uint256_t, 2> ratio = {0, 0};
	std::array<uint256_t, 2> balance = {0, 0};
	std::array<uint256_t, 2> user_total = {0, 0};
	std::array<uint256_t, 2> trade_volume = {0, 0};
	std::array<uint256_t, 2> fees_paid = {0, 0};
	std::array<uint256_t, 2> fees_claimed = {0, 0};

	const uint256_t max_fee_rate = (uint256_t(1) << FRACT_BITS) / 4;
	const uint256_t min_fee_rate = (uint256_t(1) << FRACT_BITS) / 2048;

	void add_liquid(User& user, std::array<uint256_t, 2> amount)
	{
		payout(user);

		for(int i = 0; i < 2; ++i) {
			if(amount[i]) {
				balance[i] += amount[i];
				user_total[i] += amount[i];
				user.balance[i] += amount[i];
				user.last_user_total[i] = user_total[i];
			}
		}
		update_ratio();
	}

	void rem_liquid(User& user, std::array<uint256_t, 2> amount)
	{
		for(int i = 0; i < 2; ++i) {
			if(amount[i] > user.balance[i]) {
				throw std::logic_error("amount > user balance");
			}
		}
		for(int i = 0; i < 2; ++i) {
			if(amount[i]) {
				auto ret_amount = (amount[i] * balance[i]) / user_total[i];
				if(ret_amount > balance[i]) {
					std::cout << "rem_liquid(): ret_amount > balance[i]: " << ret_amount << " > " << balance[i] << " (i=" << i << ")" << std::endl;
				}
				ret_amount = std::min(ret_amount, balance[i]);
				user.wallet[i] += ret_amount;
				user.balance[i] -= amount[i];
				balance[i] -= ret_amount;
				user_total[i] -= amount[i];
			}
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
		if(trade_amount < 1) {
			return;
		}
		const auto fee_rate = std::max((trade_amount * max_fee_rate) / balance[k], min_fee_rate);
		const auto fee_amount = std::max((trade_amount * fee_rate) >> FRACT_BITS, uint256_t(1));

		auto actual_amount = trade_amount - fee_amount;
		actual_amount = std::min(actual_amount, balance[k]);
		user.wallet[k] += actual_amount;
		balance[k] -= actual_amount;
		fees_paid[k] += fee_amount;
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
				const auto total_fees = fees_paid[i] - user.last_fees_paid[i];
				auto user_share = (2 * total_fees * user.balance[i]) / (user_total[i] + user.last_user_total[i]);
				user_share = std::min(user_share, balance[i]);
				balance[i] -= user_share;
				fees_claimed[i] += user_share;
				user.wallet[i] += user_share;
			}
			user.last_user_total[i] = user_total[i];
			user.last_fees_paid[i] = fees_paid[i];
		}
		update_ratio();
	}

};


int main()
{
	{
		Swap swap;

		User A;
		User B;
		User C;

		swap.add_liquid(A, {1000, 0});
		swap.add_liquid(B, {0, 5000});
		swap.add_liquid(A, {1000, 0});
		swap.add_liquid(B, {0, 5000});
		std::cout << "A_balance = [" << A.balance[0] << ", " << A.balance[1] << "]" << std::endl;
		std::cout << "B_balance = [" << B.balance[0] << ", " << B.balance[1] << "]" << std::endl;
		std::cout << "balance =   [" << swap.balance[0] << ", " << swap.balance[1] << "]" << std::endl << std::endl;

		swap.trade(C, 0, 1000);
		std::cout << "balance = [" << swap.balance[0] << ", " << swap.balance[1] << "]" << std::endl;
		std::cout << "wallet =  [" << C.wallet[0] << ", " << C.wallet[1] << "]" << std::endl << std::endl;

		swap.payout(B);
		std::cout << "fees_paid =    [" << swap.fees_paid[0] << ", " << swap.fees_paid[1] << "]" << std::endl;
		std::cout << "fees_claimed = [" << swap.fees_claimed[0] << ", " << swap.fees_claimed[1] << "]" << std::endl << std::endl;

		swap.rem_liquid(B, {0, 5000});
		std::cout << "balance =   [" << swap.balance[0] << ", " << swap.balance[1] << "]" << std::endl;
		std::cout << "B_balance = [" << B.balance[0] << ", " << B.balance[1] << "]" << std::endl;
		std::cout << "B_wallet =  [" << B.wallet[0] << ", " << B.wallet[1] << "]" << std::endl << std::endl;

		swap.rem_liquid(A, A.balance);
		swap.rem_liquid(B, B.balance);
		std::cout << "balance =  [" << swap.balance[0] << ", " << swap.balance[1] << "]" << std::endl;
		std::cout << "A_wallet = [" << A.wallet[0] << ", " << A.wallet[1] << "]" << std::endl;
		std::cout << "B_wallet = [" << B.wallet[0] << ", " << B.wallet[1] << "]" << std::endl << std::endl;
	}


	return 0;
}

