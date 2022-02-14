/*
 * Server.cpp
 *
 *  Created on: Jan 22, 2022
 *      Author: mad
 */

#include <mmx/exchange/Server.h>
#include <mmx/exchange/Client_approve.hxx>
#include <mmx/exchange/Client_approve_return.hxx>
#include <mmx/exchange/Server_reject.hxx>
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

	vnx::Directory(storage_path).create();
	{
		::rocksdb::Options options;
		options.max_open_files = 16;
		options.keep_log_file_num = 3;
		options.max_manifest_file_size = 64 * 1024 * 1024;
		options.OptimizeForSmallDb();

		trade_history.open(storage_path + "trade_history", options);
	}

	node = std::make_shared<NodeAsyncClient>(node_server);
	server = std::make_shared<vnx::GenericAsyncClient>(vnx_get_id());
	server->vnx_set_non_blocking(true);
	add_async_client(node);
	add_async_client(server);

	set_timer_millis(1000, std::bind(&Server::update, this));

	Super::main();
}

void Server::handle(std::shared_ptr<const Block> block)
{
	if(!block->proof) {
		return;
	}
	std::unordered_map<txio_key_t, hash_t> exec_map;
	for(const auto& base : block->tx_list) {
		if(auto tx = std::dynamic_pointer_cast<const Transaction>(base)) {
			for(const auto& in : tx->inputs) {
				if(utxo_map.erase(in.prev)) {
					exec_map[in.prev] = tx->id;
					lock_map.erase(in.prev);
				}
			}
		}
	}
	for(const auto& entry : trade_map) {
		const auto& book = entry.second;
		for(auto iter = book->orders.begin(); iter != book->orders.end();) {
			const auto& order = iter->second;
			if(utxo_map.count(order.bid_key)) {
				iter++;
			} else {
				{
					auto iter = exec_map.find(order.bid_key);
					if(iter != exec_map.end()) {
						{
							trade_entry_t out;
							out.id = iter->second;
							out.height = block->height;
							out.type = trade_type_e::BUY;
							out.bid = order.bid;
							out.ask = order.ask;
							trade_history.insert(entry.first, out);
						}
						{
							trade_entry_t out;
							out.id = iter->second;
							out.height = block->height;
							out.type = trade_type_e::SELL;
							out.bid = order.ask;
							out.ask = order.bid;
							trade_history.insert(entry.first.reverse(), out);
						}
					}
				}
				{
					auto iter = owner_map.find(order.bid_key);
					if(iter != owner_map.end()) {
						if(auto peer = find_peer(iter->second)) {
							peer->order_map.erase(order.bid_key);
						}
						owner_map.erase(iter);
					}
				}
				book->key_map.erase(order.bid_key);
				iter = book->orders.erase(iter);
			}
		}
	}
	log(INFO) << "Height " << block->height << ": " << exec_map.size() << " executed, "
			<< utxo_map.size() << " open, " << lock_map.size() << " locked";
}

void Server::update()
{
	const auto now = vnx::get_time_millis();
	for(auto iter = pending_trades.begin(); iter != pending_trades.end();) {
		const auto trade = iter->second;
		if(now - trade->start_time_ms > trade_timeout_ms) {
			cancel_trade(trade);
			vnx_async_return_ex_what(trade->request_id, "trade timeout");
			log(WARN) << "Trade timeout with " << trade->pending_clients.size() << " approvals pending (" << iter->first << ")";
			iter = pending_trades.erase(iter);
		} else {
			iter++;
		}
	}
}

void Server::cancel_order(const trade_pair_t& pair, const txio_key_t& key)
{
	if(auto book = find_pair(pair)) {
		auto iter = book->key_map.find(key);
		if(iter != book->key_map.end()) {
			const auto range = book->orders.equal_range(iter->second);
			for(auto iter = range.first; iter != range.second; ++iter) {
				if(iter->second.bid_key == key) {
					book->orders.erase(iter);
					break;
				}
			}
			book->key_map.erase(iter);
		}
	}
	utxo_map.erase(key);
}

void Server::cancel(const uint64_t& client, const std::vector<txio_key_t>& orders)
{
	const auto peer = get_peer(client);
	for(const auto& key : orders) {
		auto iter = peer->order_map.find(key);
		if(iter != peer->order_map.end()) {
			cancel_order(iter->second, key);
			peer->order_map.erase(iter);
			owner_map.erase(key);
		}
	}
}

void Server::reject(const uint64_t& client, const hash_t& txid)
{
	auto iter = pending_trades.find(txid);
	if(iter == pending_trades.end()) {
		throw std::logic_error("no such trade");
	}
	auto job = iter->second;
	if(job->pending_clients.count(client)) {
		cancel_trade(job);
		vnx_async_return_ex_what(job->request_id, "trade rejected");
		log(WARN) << "Trade was rejected: " << txid;
		pending_trades.erase(iter);
	}
}

void Server::approve(const uint64_t& client, std::shared_ptr<const Transaction> tx)
{
	if(!tx || !tx->is_valid()) {
		throw std::logic_error("invalid tx");
	}
	auto iter = pending_trades.find(tx->id);
	if(iter == pending_trades.end()) {
		throw std::logic_error("no such trade");
	}
	auto job = iter->second;
	if(!job->pending_clients.erase(client)) {
		throw std::logic_error("trade mismatch");
	}
	job->tx->merge_sign(tx);

	if(job->pending_clients.empty()) {
		finish_trade(job);
	}
}

void Server::finish_trade(std::shared_ptr<trade_job_t> job)
{
	const auto tx = job->tx;
	for(const auto& in : tx->inputs) {
		if(lock_map.count(in.prev)) {
			vnx_async_return_ex_what(job->request_id, "order already sold");
			return;
		}
	}
	for(const auto& in : tx->inputs) {
		if(utxo_map.count(in.prev)) {
			lock_map[in.prev] = tx->id;
			job->locked_keys.push_back(in.prev);
		}
	}
	pending_trades.erase(tx->id);

	node->add_transaction(tx, true,
		[this, job]() {
			execute_async_return(job->request_id);
			log(INFO) << "Executed trade: " << job->tx->id;
		},
		[this, job](const vnx::exception& ex) {
			cancel_trade(job);
			vnx_async_return_ex(job->request_id, ex);
			log(WARN) << "Trade failed with: " << ex.what();
		});
}

void Server::cancel_trade(std::shared_ptr<trade_job_t> job)
{
	for(const auto& key : job->locked_keys) {
		lock_map.erase(key);
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

	std::vector<txio_key_t> keys;
	for(const auto& entry : order.bids) {
		keys.push_back(entry.first);
	}
	node->get_txo_infos(keys,
		[this, peer, pair, address, order, request_id](const std::vector<vnx::optional<txo_info_t>>& entries) {
			for(const auto& entry : entries) {
				if(!entry) {
					vnx_async_return_ex_what(request_id, "no such utxo");
					return;
				}
			}
			uint64_t total_bid = 0;
			std::vector<order_t> result;
			for(size_t i = 0; i < entries.size() && i < order.bids.size(); ++i) {
				if(const auto& entry = entries[i]) {
					const auto& utxo = entry->output;
					const auto& bid = order.bids[i];
					if(!entry->spent
						&& utxo.address == address
						&& utxo.contract == pair.bid
						&& utxo_map.emplace(bid.first, utxo).second)
					{
						order_t tmp;
						tmp.ask = bid.second;
						tmp.bid = utxo.amount;
						tmp.bid_key = bid.first;
						result.push_back(tmp);
					}
					total_bid += utxo.amount;
				}
			}
			if(!result.empty()) {
				auto& book = trade_map[pair];
				if(!book) {
					book = std::make_shared<order_book_t>();
				}
				for(const auto& entry : result) {
					const auto price = entry.get_price();
					book->key_map[entry.bid_key] = price;
					book->orders.emplace(price, entry);
					peer->order_map[entry.bid_key] = pair;
					owner_map[entry.bid_key] = peer->client;
				}
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
	if(std::unordered_set<txio_key_t>(keys.begin(), keys.end()).size() != keys.size()) {
		throw std::logic_error("double spend");
	}
	node->get_txo_infos(keys,
		[this, tx, request_id](const std::vector<vnx::optional<txo_info_t>>& entries) {
			try {
				auto job = std::make_shared<trade_job_t>();
				std::unordered_set<addr_t> input_addrs;
				std::unordered_map<addr_t, uint64_t> input_amount;
				for(size_t i = 0; i < entries.size() && i < tx->inputs.size(); ++i) {
					const auto& in = tx->inputs[i];
					if(!is_open(in.prev)) {
						throw std::logic_error("offer no longer open");
					}
					if(in.solution >= tx->solutions.size()) {
						auto iter = owner_map.find(in.prev);
						if(iter != owner_map.end()) {
							job->pending_clients.insert(iter->second);
						} else {
							throw std::logic_error("missing solution for input " + std::to_string(i));
						}
					}
					if(const auto& entry = entries[i]) {
						if(entry->spent) {
							throw std::logic_error("input already spent");
						}
						const auto& utxo = entry->output;
						input_addrs.insert(utxo.address);
						input_amount[utxo.contract] += utxo.amount;
					} else {
						throw std::logic_error("invalid input");
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
				if(tx->solutions.size() > input_addrs.size()) {
					throw std::logic_error("too many solutions");
				}
				const auto fee_amount = left_amount[addr_t()];
				const auto fee_needed = tx->calc_cost(params) + (input_addrs.size() - tx->solutions.size()) * params->min_txfee_sign;
				if(fee_amount < fee_needed) {
					throw std::logic_error("insufficient fee: " + std::to_string(fee_amount) + " < " + std::to_string(fee_needed));
				}

				auto request = Client_approve::create();
				request->tx = tx;
				for(auto client : job->pending_clients) {
					send_to(client, request);
				}
				job->tx = vnx::clone(tx);
				job->request_id = request_id;
				job->start_time_ms = vnx::get_time_millis();
				pending_trades[tx->id] = job;

				if(job->pending_clients.empty()) {
					finish_trade(job);
				}
			}
			catch(const std::exception& ex) {
				vnx_async_return_ex(request_id, ex);
			}
		},
		std::bind(&Server::vnx_async_return_ex, this, request_id, std::placeholders::_1));
}

void Server::match_async(const trade_order_t& order, const vnx::request_id_t& request_id) const
{
	if(order.ask_addr == addr_t()) {
		throw std::logic_error("invalid ask_addr");
	}
	std::vector<addr_t> bid_addrs;
	for(const auto& entry : order.solutions) {
		const auto solution = std::dynamic_pointer_cast<const solution::PubKey>(entry);
		if(!solution || !solution->signature.verify(solution->pubkey, order.calc_hash())) {
			throw std::logic_error("invalid signature");
		}
		bid_addrs.push_back(solution->pubkey.get_addr());
	}
	const std::unordered_set<addr_t> bid_addr_set(bid_addrs.begin(), bid_addrs.end());

	node->get_txo_infos(order.bid_keys,
		[this, bid_addr_set, order, request_id](const std::vector<vnx::optional<txo_info_t>>& entries) {
			uint64_t max_bid = 0;
			std::vector<std::pair<txio_key_t, uint64_t>> bid_keys;
			for(size_t i = 0; i < entries.size() && i < order.bid_keys.size(); ++i) {
				if(const auto& entry = entries[i]) {
					const auto& utxo = entry->output;
					if(!entry->spent
						&& bid_addr_set.count(utxo.address)
						&& utxo.contract == order.pair.bid)
					{
						max_bid += utxo.amount;
						const auto& key = order.bid_keys[i];
						bid_keys.emplace_back(key, utxo.amount);
					}
				}
			}
			if(bid_keys.empty()) {
				vnx_async_return_ex_what(request_id, "empty order");
				return;
			}
			const auto book = find_pair(order.pair.reverse());
			if(!book) {
				vnx_async_return_ex_what(request_id, "no such trade pair");
				return;
			}
			const auto max_price = order.ask ? order.bid / double(*order.ask) : std::numeric_limits<double>::infinity();

			matched_order_t result;
			result.pair = order.pair;

			// match orders
			auto tx = Transaction::create();
			std::map<addr_t, uint64_t> output_map;

			uint64_t bid_left = std::min(order.bid, max_bid);
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
					result.bid += order.ask;
					result.ask += order.bid;
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
				out.contract = order.pair.bid;
				out.amount = entry.second;
				tx->outputs.push_back(out);
			}
			// provide inputs for the bid
			bid_left = result.bid;
			for(const auto& entry : bid_keys) {
				tx_in_t input;
				input.prev = entry.first;
				tx->inputs.push_back(input);

				if(entry.second <= bid_left) {
					bid_left -= entry.second;
				} else {
					break;
				}
			}
			// take the ask
			{
				tx_out_t out;
				out.address = order.ask_addr;
				out.contract = order.pair.ask;
				out.amount = result.ask;
				tx->outputs.push_back(out);
			}
			result.tx = tx;
			match_async_return(request_id, result);
		},
		std::bind(&Server::vnx_async_return_ex, this, request_id, std::placeholders::_1));
}

std::vector<trade_pair_t> Server::get_trade_pairs() const
{
	std::vector<trade_pair_t> res;
	for(const auto& entry : trade_map) {
		res.push_back(entry.first);
	}
	return res;
}

std::vector<order_t> Server::get_orders(const trade_pair_t& pair, const int32_t& limit_) const
{
	const size_t limit = limit_;
	std::vector<order_t> orders;
	if(auto book = find_pair(pair)) {
		for(const auto& entry : book->orders) {
			if(orders.size() >= limit) {
				break;
			}
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
	if(auto book = find_pair(trade_pair_t::create_ex(want, have.currency))) {
		for(const auto& entry : book->orders) {
			if(!left) {
				break;
			}
			const auto& order = entry.second;
			if(order.ask <= left && utxo_map.count(order.bid_key) && is_open(order.bid_key)) {
				price.value += order.bid;
				price.inverse += order.ask;
				left -= order.ask;
			}
		}
	}
	return price;
}

ulong_fraction_t Server::get_min_trade(const trade_pair_t& pair) const
{
	ulong_fraction_t price;
	price.inverse = 0;

	uint64_t min_amount = -1;
	if(auto book = find_pair(pair)) {
		for(const auto& entry : book->orders) {
			const auto& order = entry.second;
			if(order.ask < min_amount && utxo_map.count(order.bid_key) && is_open(order.bid_key)) {
				price.value = order.bid;
				price.inverse = order.ask;
				min_amount = order.ask;
			}
		}
	}
	return price;
}

std::vector<trade_entry_t> Server::get_history(const trade_pair_t& pair, const int32_t& limit) const
{
	std::vector<trade_entry_t> result;
	trade_history.find_last(pair, result, limit);
	return result;
}

void Server::ping(const uint64_t& client) const
{
}

bool Server::is_open(const txio_key_t& bid_key) const
{
	return !lock_map.count(bid_key);
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
	case Server_reject::VNX_TYPE_ID:
		if(auto value = std::dynamic_pointer_cast<const Server_reject>(msg)) {
			try {
				reject(client, value->txid);
			} catch(const std::exception& ex) {
				log(WARN) << "reject() failed with: " << ex.what();
			}
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
		for(const auto& entry : peer->order_map) {
			cancel_order(entry.second, entry.first);
		}
		log(INFO) << "Client " << peer->address << " disconnected, " << peer->order_map.size() << " orders canceled";
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
