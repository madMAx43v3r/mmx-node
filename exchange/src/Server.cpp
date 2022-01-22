/*
 * Server.cpp
 *
 *  Created on: Jan 22, 2022
 *      Author: mad
 */

#include <mmx/exchange/Server.h>
#include <mmx/solution/PubKey.hxx>


namespace mmx {
namespace exchange {

Server::Server(const std::string& _vnx_name)
	:	ServerBase(_vnx_name)
{
}

void Server::approve(std::shared_ptr<const Transaction> tx)
{
	// TODO
}

void Server::place_async(const trade_pair_t& pair, const limit_order_t& order, const vnx::request_id_t& request_id)
{
	const auto solution = std::dynamic_pointer_cast<const solution::PubKey>(order.solution);
	if(!solution || !solution->signature.verify(solution->pubkey, order.calc_hash())) {
		throw std::logic_error("invalid signature");
	}
	const auto address = solution->pubkey.get_addr();

	node->get_txo_infos(order.bid_keys,
		[this, pair, address, order, request_id](const std::vector<vnx::optional<txo_info_t>>& entries) {
			uint64_t total_bid = 0;
			std::vector<order_t> result;
			for(size_t i = 0; i < entries.size() && i < order.bid_keys.size(); ++i) {
				if(const auto& entry = entries[i]) {
					const auto& utxo = entry->output;
					const auto& bid_key = order.bid_keys[i];
					if(!entry->spent
						&& utxo.address == address
						&& utxo.contract == pair.bid
						&& utxo_map.emplace(bid_key, utxo).second)
					{
						order_t tmp;
						tmp.bid = utxo.amount;
						tmp.bid_key = bid_key;
						total_bid += tmp.bid;
						result.push_back(tmp);
					}
				}
			}
			if(!result.empty()) {
				auto& book = trade_map[pair];
				if(!book) {
					book = std::make_shared<order_book_t>();
				}
				const auto price = order.ask / double(total_bid);
				for(auto& tmp : result) {
					tmp.ask = tmp.bid * price;
					book->orders.emplace(tmp.get_price(), tmp);
				}
			}
			place_async_return(request_id);
		},
		std::bind(&Server::vnx_async_return_ex, request_id, std::placeholders::_1));
}

void Server::execute_async(const trade_pair_t& pair, const trade_order_t& order, const vnx::request_id_t& request_id) const
{
	const auto solution = std::dynamic_pointer_cast<const solution::PubKey>(order.solution);
	if(!solution || !solution->signature.verify(solution->pubkey, order.calc_hash())) {
		throw std::logic_error("invalid signature");
	}
	const auto address = solution->pubkey.get_addr();

	node->get_txo_infos(order.bid_keys,
		[this, pair, address, order, request_id](const std::vector<vnx::optional<txo_info_t>>& entries) {
			uint64_t max_bid = 0;
			std::vector<std::pair<txio_key_t, uint64_t>> bid_keys;
			for(size_t i = 0; i < entries.size() && i < order.bid_keys.size(); ++i) {
				if(const auto& entry = entries[i]) {
					const auto& utxo = entry->output;
					if(!entry->spent
						&& utxo.address == address
						&& utxo.contract == pair.bid)
					{
						max_bid += utxo.amount;
						bid_keys.emplace_back(order.bid_keys[i], utxo.amount);
					}
				}
			}
			const auto book = find_pair(pair.reverse());
			if(!book || bid_keys.empty()) {
				execute_async_return(request_id, nullptr);
				return;
			}
			const auto max_price = order.ask ? order.bid / double(*order.ask) : std::numeric_limits<double>::infinity();

			uint64_t final_bid = 0;
			uint64_t final_ask = 0;
			uint64_t bid_left = std::min(order.bid, max_bid);

			// match orders
			auto tx = Transaction::create();
			std::map<addr_t, uint64_t> output_map;
			for(const auto& entry : book->orders) {
				if(entry.first > max_price || !bid_left) {
					break;
				}
				const auto& order = entry.second;
				if(order.ask <= bid_left) {
					auto iter = utxo_map.find(order.bid_key);
					if(iter == utxo_map.end()) {
						continue;
					}
					output_map[iter->second.address] += order.ask;
					{
						tx_in_t input;
						input.prev = order.bid_key;
						tx->inputs.push_back(input);
					}
					bid_left -= order.ask;
					final_bid += order.ask;
					final_ask += order.bid;
				}
			}
			// give the bid
			for(const auto& entry : output_map) {
				tx_out_t out;
				out.address = entry.first;
				out.contract = pair.bid;
				out.amount = entry.second;
				tx->outputs.push_back(out);
			}
			// provide inputs for the bid
			bid_left = final_bid;
			for(const auto& entry : bid_keys) {
				if(!bid_left) {
					break;
				}
				tx_in_t input;
				input.prev = entry.first;
				tx->inputs.push_back(input);
				if(entry.second <= bid_left) {
					bid_left -= entry.second;
				} else {
					// change output
					tx_out_t out;
					out.address = address;
					out.contract = pair.bid;
					out.amount = entry.second - bid_left;
					tx->outputs.push_back(out);
					bid_left = 0;
				}
			}
			// take the ask
			{
				tx_out_t out;
				out.address = address;
				out.contract = pair.ask;
				out.amount = final_ask;
				tx->outputs.push_back(out);
			}
			execute_async_return(request_id, tx);
		},
		std::bind(&Server::vnx_async_return_ex, request_id, std::placeholders::_1));
}

std::vector<order_t> Server::get_orders(const trade_pair_t& pair) const
{
	std::vector<order_t> orders;
	if(auto book = find_pair(pair)) {
		for(const auto& entry : book->orders) {
			const auto& order = entry.second;
			if(utxo_map.count(order.bid_key)) {
				orders.push_back(order);
			}
		}
	}
	return orders;
}

ulong_fraction_t Server::get_price(const addr_t& want, const amount_t& have) const
{
	ulong_fraction_t price;
	price.inverse = 0;

	uint64_t left = have.amount;
	for(const auto& order : get_orders(trade_pair_t::create_ex(want, have.currency))) {
		if(order.bid >= left) {
			price.value += order.ask;
			price.inverse += order.bid;
			left = 0;
			break;
		} else {
			price.value += order.ask * (left / double(order.bid));
			price.inverse += left;
		}
		left -= order.bid;
	}
	return price;
}

std::shared_ptr<Server::order_book_t> Server::find_pair(const trade_pair_t& pair) const
{
	auto iter = trade_map.find(pair);
	if(iter != trade_map.end()) {
		return iter->second;
	}
	return nullptr;
}


} // exchange
} // mmx
