/*
 * Server.cpp
 *
 *  Created on: Jan 22, 2022
 *      Author: mad
 */

#include <mmx/exchange/Server.h>
#include <mmx/exchange/Client_approve.hxx>
#include <mmx/exchange/Client_approve_return.hxx>
#include <mmx/solution/PubKey.hxx>


namespace mmx {
namespace exchange {

Server::Server(const std::string& _vnx_name)
	:	ServerBase(_vnx_name)
{
	params = mmx::get_params();
}

void Server::init()
{
	Super::init();
	vnx::open_pipe(vnx_name, this, 10000);
	vnx::open_pipe(vnx_get_id(), this, 10000);
}

void Server::main()
{
	subscribe(input_blocks, 10000);

	node = std::make_shared<NodeAsyncClient>(node_server);
	server = std::make_shared<vnx::GenericAsyncClient>(vnx_get_id());
	server->vnx_set_non_blocking(true);
	add_async_client(node);
	add_async_client(server);

	Super::main();
}

void Server::handle(std::shared_ptr<const Block> block)
{
	size_t num_exec = 0;
	for(const auto& base : block->tx_list) {
		if(auto tx = std::dynamic_pointer_cast<const Transaction>(base)) {
			for(const auto& in : tx->inputs) {
				if(utxo_map.erase(in.prev)) {
					lock_map.erase(in.prev);
					num_exec++;
				}
			}
		}
	}
	size_t num_purged = 0;
	for(const auto& entry : trade_map) {
		const auto& book = entry.second;
		for(auto iter = book->orders.begin(); iter != book->orders.end();) {
			const auto& order = iter->second;
			if(utxo_map.count(order.bid_key) && !cancel_set.count(order.bid_key)) {
				iter++;
			} else {
				if(cancel_set.erase(order.bid_key)) {
					utxo_map.erase(order.bid_key);
				}
				iter = book->orders.erase(iter);
				num_purged++;
			}
		}
	}
	log(INFO) << "Height " << block->height << ": " << num_exec << " executed, " << num_purged << " canceled, "
			<< utxo_map.size() << " open, " << lock_map.size() << " locked";
}

void Server::cancel(const uint64_t& client, const std::vector<txio_key_t>& orders)
{
	const auto peer = get_peer(client);
	for(const auto& key : orders) {
		auto iter = peer->order_set.find(key);
		if(iter != peer->order_set.end()) {
			cancel_set.insert(key);
			peer->order_set.erase(iter);
		}
	}
}

void Server::reject(const uint64_t& client, const hash_t& txid)
{
	auto iter = pending.find(txid);
	if(iter == pending.end()) {
		throw std::logic_error("no such trade");
	}
	auto job = iter->second;
	if(job->pending_clients.count(client)) {
		for(const auto& in : job->tx->inputs) {
			lock_map.erase(in.prev);
		}
		pending.erase(iter);
		vnx_async_return_ex_what(job->request_id, "trade rejected");
		log(WARN) << "Trade was rejected: " << txid;
	}
}

void Server::approve(const uint64_t& client, std::shared_ptr<const Transaction> tx)
{
	if(!tx || !tx->is_valid()) {
		throw std::logic_error("invalid tx");
	}
	auto iter = pending.find(tx->id);
	if(iter == pending.end()) {
		throw std::logic_error("no such trade");
	}
	auto job = iter->second;
	if(!job->pending_clients.erase(client)) {
		throw std::logic_error("trade mismatch");
	}
	job->tx->merge_sign(tx);

	if(job->pending_clients.empty())
	{
		pending.erase(job->tx->id);
		node->add_transaction(job->tx, true,
			[this, job]() {
				execute_async_return(job->request_id);
				log(INFO) << "Executed trade: " << job->tx->id;
			},
			[this, job](const vnx::exception& ex) {
				for(const auto& in : job->tx->inputs) {
					lock_map.erase(in.prev);
				}
				vnx_async_return_ex(job->request_id, ex);
				log(WARN) << "Trade failed with: " << ex.what();
			});
	}
}

void Server::place_async(const uint64_t& client, const trade_pair_t& pair, const limit_order_t& order, const vnx::request_id_t& request_id) const
{
	const auto peer = get_peer(client);
	const auto solution = std::dynamic_pointer_cast<const solution::PubKey>(order.solution);
	if(!solution || !solution->signature.verify(solution->pubkey, order.calc_hash())) {
		throw std::logic_error("invalid signature");
	}
	const auto address = solution->pubkey.get_addr();
	addr_map[address] = client;

	node->get_txo_infos(order.bid_keys,
		[this, peer, pair, address, order, request_id](const std::vector<vnx::optional<txo_info_t>>& entries) {
			for(const auto& entry : entries) {
				if(!entry) {
					vnx_async_return_ex_what(request_id, "no such utxo");
					return;
				}
			}
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
						result.push_back(tmp);
					}
					total_bid += utxo.amount;
				}
			}
			if(result.empty()) {
				vnx_async_return_ex_what(request_id, "empty order");
				return;
			}
			auto& book = trade_map[pair];
			if(!book) {
				book = std::make_shared<order_book_t>();
			}
			const auto price = order.ask / double(total_bid);
			for(auto& tmp : result) {
				tmp.ask = tmp.bid * price;
				peer->order_set.insert(tmp.bid_key);
				book->orders.emplace(tmp.get_price(), tmp);
			}
			place_async_return(request_id, result);
		},
		std::bind(&Server::vnx_async_return_ex, this, request_id, std::placeholders::_1));
}

void Server::execute_async(std::shared_ptr<const Transaction> tx, const vnx::request_id_t& request_id)
{
	if(!tx || !tx->is_valid()) {
		throw std::logic_error("invalid tx");
	}
	std::vector<txio_key_t> keys;
	for(const auto& in : tx->inputs) {
		keys.push_back(in.prev);
	}
	node->get_txo_infos(keys,
		[this, tx, request_id](const std::vector<vnx::optional<txo_info_t>>& entries) {
			try {
				std::unordered_set<addr_t> offer_addrs;
				std::unordered_set<addr_t> input_addrs;
				std::unordered_map<addr_t, uint64_t> input_amount;
				for(size_t i = 0; i < entries.size() && i < tx->inputs.size(); ++i) {
					const auto& in = tx->inputs[i];
					if(!is_open(in.prev)) {
						throw std::logic_error("offer no longer open");
					}
					if(const auto& entry = entries[i]) {
						if(entry->spent) {
							throw std::logic_error("input already spent");
						}
						const auto& utxo = entry->output;
						if(in.solution < tx->solutions.size()) {
							offer_addrs.insert(utxo.address);
						}
						input_addrs.insert(utxo.address);
						input_amount[utxo.contract] += utxo.amount;
					} else {
						throw std::logic_error("no such utxo");
					}
				}
				std::unordered_map<addr_t, uint64_t> output_amount;
				for(const auto& out : tx->outputs) {
					output_amount[out.contract] += out.amount;
				}
				std::unordered_map<addr_t, uint64_t> left_amount = input_amount;
				for(const auto& entry : output_amount) {
					auto& left = left_amount[entry.first];
					if(entry.second > left) {
						throw std::logic_error("over-spend");
					}
					left -= entry.second;
				}
				for(const auto& entry : left_amount) {
					if(entry.second && entry.first != addr_t()) {
						throw std::logic_error("left-over amount");
					}
				}
				const auto fee_amount = left_amount[addr_t()];
				const auto fee_needed = tx->calc_cost(params) + input_addrs.size() * params->min_txfee_sign;
				if(fee_amount < fee_needed) {
					throw std::logic_error("insufficient fee: " + std::to_string(fee_amount) + " < " + std::to_string(fee_needed));
				}
				auto job = std::make_shared<trade_job_t>();

				for(const auto& addr : offer_addrs) {
					auto iter = addr_map.find(addr);
					if(iter == addr_map.end()) {
						throw std::logic_error("client not found");
					}
					job->pending_clients.insert(iter->second);
				}
				auto request = Client_approve::create();
				request->tx = tx;
				for(const auto client : job->pending_clients) {
					send_to(client, request);
				}
				for(const auto& in : tx->inputs) {
					lock_map[in.prev] = tx->id;
				}
				job->tx = vnx::clone(tx);
				job->request_id = request_id;
				job->start_time_ms = vnx::get_time_millis();
				pending[tx->id] = job;
			}
			catch(const std::exception& ex) {
				vnx_async_return_ex(request_id, ex);
			}
		},
		std::bind(&Server::vnx_async_return_ex, this, request_id, std::placeholders::_1));
}

void Server::match_async(const trade_pair_t& pair, const trade_order_t& order, const vnx::request_id_t& request_id) const
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
			if(bid_keys.empty()) {
				vnx_async_return_ex_what(request_id, "empty order");
				return;
			}
			const auto book = find_pair(pair.reverse());
			if(!book) {
				vnx_async_return_ex_what(request_id, "no such trade pair");
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
					if(iter == utxo_map.end() || !is_open(order.bid_key)) {
						continue;
					}
					const auto& utxo = iter->second;
					output_map[utxo.address] += order.ask;
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
			if(output_map.empty()) {
				vnx_async_return_ex_what(request_id, "empty match");
				return;
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
			match_async_return(request_id, tx);
		},
		std::bind(&Server::vnx_async_return_ex, this, request_id, std::placeholders::_1));
}

std::vector<order_t> Server::get_orders(const trade_pair_t& pair) const
{
	std::vector<order_t> orders;
	if(auto book = find_pair(pair)) {
		for(const auto& entry : book->orders) {
			const auto& order = entry.second;
			if(utxo_map.count(order.bid_key) && is_open(order.bid_key)) {
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

bool Server::is_open(const txio_key_t& bid_key) const
{
	return !lock_map.count(bid_key) && !cancel_set.count(bid_key);
}

std::shared_ptr<Server::order_book_t> Server::find_pair(const trade_pair_t& pair) const
{
	auto iter = trade_map.find(pair);
	if(iter != trade_map.end()) {
		return iter->second;
	}
	return nullptr;
}

void Server::send_to(uint64_t client, std::shared_ptr<const vnx::Value> msg, bool reliable)
{
	if(auto peer = find_peer(client)) {
		send_to(peer, msg, reliable);
	}
}

void Server::send_to(std::shared_ptr<peer_t> peer, std::shared_ptr<const vnx::Value> msg, bool reliable)
{
	Super::send_to(peer, msg);
}

void Server::on_error(uint64_t client, uint32_t id, const vnx::exception& ex)
{
	auto ret = Return::create();
	ret->id = id;
	ret->result = ex.value();
	send_to(client, ret);
}

void Server::on_request(uint64_t client, std::shared_ptr<const Request> msg)
{
	auto method = vnx::clone(msg->method);
	if(method) {
		method->set_field("client", vnx::Variant(client));
	}
	auto ret = Return::create();
	ret->id = msg->id;
	server->call(method,
		[this, client, ret](std::shared_ptr<const vnx::Value> result) {
			ret->result = result;
			send_to(client, ret);
		},
		std::bind(&Server::on_error, this, client, msg->id, std::placeholders::_1));
}

void Server::on_return(uint64_t client, std::shared_ptr<const Return> msg)
{
	const auto result = msg->result;
	if(!result) {
		return;
	}
	switch(result->get_type_hash()) {}
}

void Server::on_msg(uint64_t client, std::shared_ptr<const vnx::Value> msg)
{
	switch(msg->get_type_hash())
	{
	case Request::VNX_TYPE_ID:
		if(auto value = std::dynamic_pointer_cast<const Request>(msg)) {
			on_request(client, value);
		}
		break;
	case Return::VNX_TYPE_ID:
		if(auto value = std::dynamic_pointer_cast<const Return>(msg)) {
			on_return(client, value);
		}
		break;
	case Client_approve_return::VNX_TYPE_ID:
		if(auto value = std::dynamic_pointer_cast<const Client_approve_return>(msg)) {
			try {
				approve(client, value->_ret_0);
			} catch(const std::exception& ex) {
				log(WARN) << "approve() failed with: " << ex.what();
			}
		}
		break;
	}
}

void Server::on_pause(uint64_t client)
{
	if(auto peer = find_peer(client)) {
		peer->is_blocked = true;
		if(!peer->is_outbound) {
			pause(client);		// pause incoming traffic
		}
	}
}

void Server::on_resume(uint64_t client)
{
	if(auto peer = find_peer(client)) {
		peer->is_blocked = false;
		if(!peer->is_outbound) {
			resume(client);		// resume incoming traffic
		}
	}
}

void Server::on_connect(uint64_t client, const std::string& address)
{
	auto peer = std::make_shared<peer_t>();
	peer->client = client;
	peer->address = address;
	peer_map[client] = peer;

	log(INFO) << "Connected to client " << peer->address;
}

void Server::on_disconnect(uint64_t client)
{
	if(auto peer = find_peer(client)) {
		cancel_set.insert(peer->order_set.begin(), peer->order_set.end());
		log(INFO) << "Client " << peer->address << " disconnected, " << peer->order_set.size() << " orders canceled";
	}
	add_task([this, client]() {
		peer_map.erase(client);
	});
}

std::shared_ptr<Server::Super::peer_t> Server::get_peer_base(uint64_t client) const
{
	return get_peer(client);
}

std::shared_ptr<Server::peer_t> Server::get_peer(uint64_t client) const
{
	if(auto peer = find_peer(client)) {
		return peer;
	}
	throw std::logic_error("no such peer");
}

std::shared_ptr<Server::peer_t> Server::find_peer(uint64_t client) const
{
	auto iter = peer_map.find(client);
	if(iter != peer_map.end()) {
		return iter->second;
	}
	return nullptr;
}


} // exchange
} // mmx
