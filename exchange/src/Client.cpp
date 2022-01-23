/*
 * Client.cpp
 *
 *  Created on: Jan 23, 2022
 *      Author: mad
 */

#include <mmx/exchange/Client.h>
#include <mmx/exchange/Client_approve.hxx>
#include <mmx/exchange/Client_approve_return.hxx>
#include <mmx/solution/PubKey.hxx>


namespace mmx {
namespace exchange {

std::shared_ptr<const Transaction> Client::approve(std::shared_ptr<const Transaction> tx) const
{
	std::unordered_set<addr_t> addr_set;
	std::unordered_set<uint32_t> wallets;
	std::unordered_map<addr_t, uint64_t> expect_amount;
	for(size_t i = 0; tx->inputs.size(); ++i) {
		const auto& in = tx->inputs[i];
		auto iter = order_map.find(in.prev);
		if(iter != order_map.end()) {
			const auto& order = iter->second;
			wallets.insert(order.wallet);
			addr_set.insert(order.bid.address);
			expect_amount[order.ask.currency] += order.ask.amount;
		}
	}
	std::unordered_map<addr_t, uint64_t> output_amount;
	for(const auto& out : tx->outputs) {
		if(addr_set.count(out.address)) {
			output_amount[out.contract] += out.amount;
		}
	}
	// TODO
}


} // exchange
} // mmx
