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
	std::array<uint256_t, 2> wallet = {0, 0};
	std::array<uint256_t, 2> balance = {0, 0};
	std::array<uint256_t, 2> user_total = {0, 0};
	std::array<uint256_t, 2> fees_paid = {0, 0};
	std::array<uint256_t, 2> fees_claimed = {0, 0};

	const uint256_t max_fee_rate = (uint256_t(1) << FRACT_BITS) / 4;
	const uint256_t min_fee_rate = (uint256_t(1) << FRACT_BITS) / 2048;

	void add_liquid(User& user, std::array<uint256_t, 2> amount)
	{
		payout(user);

		for(int i = 0; i < 2; ++i) {
			if(amount[i]) {
				wallet[i] += amount[i];
				balance[i] += amount[i];
				user_total[i] += amount[i];
				user.balance[i] += amount[i];
				user.last_user_total[i] = user_total[i];
			}
		}
	}

	void rem_liquid(User& user, const int i, uint256_t amount)
	{
		if(amount == 0) {
			throw std::logic_error("amount == 0");
		}
		if(amount > user.balance[i]) {
			throw std::logic_error("amount > user balance");
		}
		uint256_t ret_amount = amount;

		if(balance[i] < user_total[i]) {
			ret_amount = (amount * balance[i]) / user_total[i];

			const int k = (i + 1) % 2;
			const auto trade_amount = ((balance[k] - user_total[k]) * amount) / user_total[i];
			user.wallet[k] += trade_amount;
			wallet[k] -= trade_amount;
			balance[k] -= trade_amount;
		}
		user.wallet[i] += ret_amount;
		user.balance[i] -= amount;
		wallet[i] -= ret_amount;
		balance[i] -= ret_amount;
		user_total[i] -= amount;
	}

	void trade(User& user, const int i, uint256_t amount)
	{
		if(amount == 0) {
			return;
		}
		const int k = (i + 1) % 2;
		if(balance[k] == 0) {
			throw std::logic_error("nothing to buy");
		}
		wallet[i] += amount;
		balance[i] += amount;

		auto trade_amount = (amount * balance[k]) / balance[i];
		if(trade_amount < 4) {
			throw std::logic_error("trade_amount < 4");
		}
		const auto fee_rate = std::max((trade_amount * max_fee_rate) / balance[k], min_fee_rate);
		const auto fee_amount = std::max((trade_amount * fee_rate) >> FRACT_BITS, uint256_t(1));

		auto actual_amount = trade_amount - fee_amount;
		actual_amount = std::min(actual_amount, wallet[k]);
		user.wallet[k] += actual_amount;
		wallet[k] -= actual_amount;
		balance[k] -= trade_amount;
		fees_paid[k] += fee_amount;
	}

	void payout(User& user)
	{
		const auto user_share = get_earned_fees(user);
		for(int i = 0; i < 2; ++i) {
			if(user_share[i]) {
				wallet[i] -= user_share[i];
				fees_claimed[i] += user_share[i];
				user.wallet[i] += user_share[i];
			}
			user.last_user_total[i] = user_total[i];
			user.last_fees_paid[i] = fees_paid[i];
		}
	}

	std::array<uint256_t, 2> get_earned_fees(User& user)
	{
		std::array<uint256_t, 2> user_share = {0, 0};
		for(int i = 0; i < 2; ++i) {
			if(user.balance[i]) {
				const auto total_fees = fees_paid[i] - user.last_fees_paid[i];
				user_share[i] = (2 * total_fees * user.balance[i])
						/ (user_total[i] + user.last_user_total[i]);
				user_share[i] = std::min(user_share[i], wallet[i] - balance[i]);
			}
		}
		return user_share;
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
		std::cout << "balance =   [" << swap.balance[0] << ", " << swap.balance[1] << "]" << std::endl;
		std::cout << "wallet =    [" << swap.wallet[0] << ", " << swap.wallet[1] << "]" << std::endl << std::endl;

		swap.trade(C, 0, 1000);
		std::cout << "wallet =   [" << swap.wallet[0] << ", " << swap.wallet[1] << "]" << std::endl;
		std::cout << "balance =  [" << swap.balance[0] << ", " << swap.balance[1] << "]" << std::endl;
		std::cout << "C_wallet = [" << C.wallet[0] << ", " << C.wallet[1] << "]" << std::endl << std::endl;

		swap.payout(B);
		std::cout << "fees_paid =    [" << swap.fees_paid[0] << ", " << swap.fees_paid[1] << "]" << std::endl;
		std::cout << "fees_claimed = [" << swap.fees_claimed[0] << ", " << swap.fees_claimed[1] << "]" << std::endl << std::endl;

		swap.rem_liquid(B, 1, 5000);
		std::cout << "wallet =    [" << swap.wallet[0] << ", " << swap.wallet[1] << "]" << std::endl;
		std::cout << "balance =   [" << swap.balance[0] << ", " << swap.balance[1] << "]" << std::endl;
		std::cout << "B_balance = [" << B.balance[0] << ", " << B.balance[1] << "]" << std::endl;
		std::cout << "B_wallet =  [" << B.wallet[0] << ", " << B.wallet[1] << "]" << std::endl << std::endl;

		swap.rem_liquid(A, 0, A.balance[0]);
		swap.rem_liquid(B, 1, B.balance[1]);
		std::cout << "wallet =   [" << swap.wallet[0] << ", " << swap.wallet[1] << "]" << std::endl;
		std::cout << "balance =  [" << swap.balance[0] << ", " << swap.balance[1] << "]" << std::endl;
		std::cout << "A_wallet = [" << A.wallet[0] << ", " << A.wallet[1] << "]" << std::endl;
		std::cout << "B_wallet = [" << B.wallet[0] << ", " << B.wallet[1] << "]" << std::endl << std::endl;
	}


	return 0;
}

