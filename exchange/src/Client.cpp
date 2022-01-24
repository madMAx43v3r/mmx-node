/*
 * Client.cpp
 *
 *  Created on: Jan 23, 2022
 *      Author: mad
 */

#include <mmx/exchange/Client.h>
#include <mmx/exchange/Client_approve.hxx>
#include <mmx/exchange/Client_approve_return.hxx>
#include <mmx/exchange/Server_place.hxx>
#include <mmx/exchange/Server_place_return.hxx>
#include <mmx/solution/PubKey.hxx>
#include <mmx/Request.hxx>


namespace mmx {
namespace exchange {

Client::Client(const std::string& _vnx_name)
	:	ClientBase(_vnx_name)
{
}

void Client::init()
{
	vnx::open_pipe(vnx_name, this, 10000);
}

void Client::main()
{
	subscribe(input_blocks, 10000);

	node = std::make_shared<NodeClient>(node_server);
	wallet = std::make_shared<WalletClient>(wallet_server);

	Super::main();

	std::unordered_map<uint32_t, std::vector<txio_key_t>> keys;
	for(const auto& entry : order_map) {
		keys[entry.second.wallet].push_back(entry.first);
	}
	for(const auto& entry : keys) {
		wallet->release(entry.first, entry.second);
	}
}

void Client::handle(std::shared_ptr<const Block> block)
{
	for(const auto& base : block->tx_list) {
		if(auto tx = std::dynamic_pointer_cast<const Transaction>(base)) {
			for(const auto& in : tx->inputs) {
				auto iter = order_map.find(in.prev);
				if(iter != order_map.end()) {
					const auto& order = iter->second;
					{
						auto iter = offer_map.find(order.offer_id);
						if(iter != offer_map.end()) {
							const auto offer = iter->second;
							offer->bid_sold += order.bid.amount;
							offer->received += order.ask;
						}
					}
					log(INFO) << "Sold " << order.bid.amount << " [" << order.bid.contract << "] for " << order.ask << " [" << order.ask.currency << "]";
					order_map.erase(iter);
				}
			}
		}
	}
}

vnx::optional<open_order_t> Client::get_order(const txio_key_t& key) const
{
	auto iter = order_map.find(key);
	if(iter != order_map.end()) {
		return iter->second;
	}
	return nullptr;
}

std::shared_ptr<const OrderBundle> Client::get_offer(const uint64_t& id) const
{
	auto iter = offer_map.find(id);
	if(iter != offer_map.end()) {
		return iter->second;
	}
	return nullptr;
}

std::vector<std::shared_ptr<const OrderBundle>> Client::get_all_offers() const
{
	std::vector<std::shared_ptr<const OrderBundle>> res;
	for(const auto& entry : offer_map) {
		res.push_back(entry.second);
	}
	return res;
}

std::shared_ptr<const OrderBundle> Client::make_offer(const uint32_t& index, const trade_pair_t& pair, const uint64_t& bid, const uint64_t& ask) const
{
	auto offer = OrderBundle::create();
	offer->id = next_offer_id++;
	offer->wallet = index;
	offer->pair = pair;

	const auto price = ask / double(bid);
	std::unordered_map<addr_t, std::vector<std::pair<txio_key_t, uint64_t>>> addr_map;

	for(const auto& entry : wallet->gather_utxos_for(index, bid, pair.bid)) {
		const auto& utxo = entry.output;
		open_order_t order;
		order.wallet = index;
		order.offer_id = offer->id;
		order.bid = utxo.amount;
		order.ask.amount = utxo.amount * price;
		order.ask.currency = pair.ask;
		offer->bid += order.bid;
		offer->ask += order.ask.amount;
		offer->orders.emplace_back(entry.key, order);
		addr_map[utxo.address].emplace_back(entry.key, order.ask.amount);
	}
	for(const auto& bundle : addr_map) {
		limit_order_t order;
		for(const auto& entry : bundle.second) {
			order.ask += entry.second;
			order.bid_keys.push_back(entry.first);
		}
		offer->limit_orders.push_back(order);
	}
	return offer;
}

void Client::send_offer(uint64_t server, std::shared_ptr<const OrderBundle> offer)
{
	for(const auto& order : offer->limit_orders) {
		auto req = Request::create();
		req->id = next_request_id++;
		auto method = Server_place::create();
		method->pair = offer->pair;
		method->order = order;
		send_to(server, req);
	}
}

void Client::place(std::shared_ptr<const OrderBundle> offer_)
{
	auto offer = vnx::clone(offer_);
	for(auto& order : offer->limit_orders) {
		// TODO: sign
	}
	for(auto server : server_set) {
		send_offer(server, offer);
	}
	order_map.insert(offer->orders.begin(), offer->orders.end());
	offer_map[offer->id] = offer;

	log(INFO) << "Placed offer " << offer->id << ", asking " << offer->ask << " [" << offer->pair.ask << "] for " << offer->bid << " [" << offer->pair.bid << "]";

	std::vector<txio_key_t> keys;
	for(const auto& entry : offer->orders) {
		keys.push_back(entry.first);
	}
	wallet->reserve(offer->wallet, keys);
}

std::shared_ptr<const Transaction> Client::approve(std::shared_ptr<const Transaction> tx) const
{
	std::unordered_set<addr_t> addr_set;
	std::unordered_set<uint32_t> wallets;
	std::unordered_map<addr_t, uint64_t> expect_amount;
	for(const auto& in : tx->inputs) {
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
	for(const auto& entry : expect_amount) {
		auto iter = output_amount.find(entry.first);
		if(iter == output_amount.end() || iter->second < entry.second) {
			throw std::logic_error("expected amount: " + std::to_string(entry.second) + " [" + entry.first.to_string() + "]");
		}
	}
	auto copy = vnx::clone(tx);
	for(auto index : wallets) {
		wallet->sign_off(index, copy);
	}
	log(INFO) << "Accepted trade " << tx->id;
	return copy;
}


} // exchange
} // mmx
