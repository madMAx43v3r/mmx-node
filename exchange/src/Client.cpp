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
#include <mmx/exchange/Server_execute.hxx>
#include <mmx/exchange/Server_execute_return.hxx>
#include <mmx/exchange/Server_match.hxx>
#include <mmx/exchange/Server_match_return.hxx>
#include <mmx/exchange/Server_get_orders.hxx>
#include <mmx/exchange/Server_get_orders_return.hxx>
#include <mmx/exchange/Server_get_price.hxx>
#include <mmx/exchange/Server_get_price_return.hxx>
#include <mmx/solution/PubKey.hxx>
#include <mmx/Request.hxx>

#include <vnx/vnx.h>
#include <vnx/TcpEndpoint.hxx>


namespace mmx {
namespace exchange {

Client::Client(const std::string& _vnx_name)
	:	ClientBase(_vnx_name)
{
}

void Client::init()
{
	Super::init();
	vnx::open_pipe(vnx_name, this, 10000);
}

void Client::main()
{
	subscribe(input_blocks, 10000);

	node = std::make_shared<NodeClient>(node_server);
	wallet = std::make_shared<WalletClient>(wallet_server);

	threads = new vnx::ThreadPool(1);

	set_timer_millis(60 * 1000, std::bind(&Client::connect, this));

	connect();

	Super::main();

	std::unordered_map<uint32_t, std::vector<txio_key_t>> keys;
	for(const auto& entry : order_map) {
		keys[entry.second.wallet].push_back(entry.first);
	}
	for(const auto& entry : keys) {
		try {
			wallet->release(entry.first, entry.second);
		} catch(...) {
			break;
		}
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
							offer->received += order.ask.amount;
						}
					}
					log(INFO) << "Sold " << order.bid.amount << " [" << order.bid.contract << "] for " << order.ask.amount << " [" << order.ask.currency << "]";
					order_map.erase(iter);
				}
			}
		}
	}
}

std::vector<std::string> Client::get_servers() const
{
	std::vector<std::string> list;
	for(const auto& entry : server_map) {
		if(avail_server_map.count(entry.second)) {
			list.push_back(entry.first);
		}
	}
	return list;
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
		if(offer->bid + utxo.amount > bid) {
			continue;
		}
		open_order_t order;
		order.wallet = index;
		order.offer_id = offer->id;
		order.bid = utxo;
		order.ask.amount = utxo.amount * price;
		order.ask.currency = pair.ask;
		offer->bid += order.bid.amount;
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
		const auto hash = order.calc_hash();
		order.solution = wallet->sign_msg(index, bundle.first, hash);
		offer->limit_orders.push_back(order);
	}
	return offer;
}

std::vector<trade_order_t> Client::make_trade(const uint32_t& index, const trade_pair_t& pair, const uint64_t& bid, const vnx::optional<uint64_t>& ask) const
{
	const double price = ask ? *ask / double(bid) : 0;
	std::unordered_map<addr_t, std::vector<std::pair<txio_key_t, uint64_t>>> addr_map;

	for(const auto& entry : wallet->gather_utxos_for(index, bid, pair.bid)) {
		const auto& utxo = entry.output;
		addr_map[utxo.address].emplace_back(entry.key, utxo.amount);
	}
	std::vector<trade_order_t> orders;

	for(const auto& bundle : addr_map) {
		trade_order_t order;
		if(ask) {
			order.ask = uint64_t(0);
		}
		for(const auto& entry : bundle.second) {
			if(ask) {
				*order.ask += entry.second * price;
			}
			order.bid += entry.second;
			order.bid_keys.push_back(entry.first);
		}
		const auto hash = order.calc_hash();
		order.solution = wallet->sign_msg(index, bundle.first, hash);
		orders.push_back(order);
	}
	return orders;
}

void Client::send_offer(uint64_t server, std::shared_ptr<const OrderBundle> offer)
{
	for(const auto& order : offer->limit_orders) {
		auto method = Server_place::create();
		method->pair = offer->pair;
		method->order = order;
		auto req = Request::create();
		req->id = next_request_id++;
		req->method = method;
		send_to(server, req);
	}
}

void Client::place(std::shared_ptr<const OrderBundle> offer)
{
	for(const auto& entry : avail_server_map) {
		send_offer(entry.second, offer);
	}
	order_map.insert(offer->orders.begin(), offer->orders.end());
	offer_map[offer->id] = vnx::clone(offer);

	log(INFO) << "Placed offer " << offer->id << ", asking "
			<< offer->ask << " [" << offer->pair.ask << "] for " << offer->bid << " [" << offer->pair.bid << "]"
			<< " (" << avail_server_map.size() << " servers)";
	if(avail_server_map.empty()) {
		log(WARN) << "Not connected to any exchange servers at the moment!";
	}

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

void Client::execute_async(const std::string& server, const uint32_t& index, std::shared_ptr<const Transaction> tx, const vnx::request_id_t& request_id)
{
	auto peer = get_server(server);
	auto method = Server_execute::create();
	method->tx = wallet->sign_off(index, tx, true);
	send_request(peer, method, std::bind(&Client::vnx_async_return, this, request_id, std::placeholders::_1));
}

void Client::match_async(const std::string& server, const trade_pair_t& pair, const trade_order_t& order, const vnx::request_id_t& request_id) const
{
	auto peer = get_server(server);
	auto method = Server_match::create();
	method->pair = pair;
	method->order = order;
	send_request(peer, method, std::bind(&Client::vnx_async_return, this, request_id, std::placeholders::_1));
}

void Client::get_orders_async(const std::string& server, const trade_pair_t& pair, const vnx::request_id_t& request_id) const
{
	auto peer = get_server(server);
	auto method = Server_get_orders::create();
	method->pair = pair;
	send_request(peer, method, std::bind(&Client::vnx_async_return, this, request_id, std::placeholders::_1));
}

void Client::get_price_async(const std::string& server, const addr_t& want, const amount_t& have, const vnx::request_id_t& request_id) const
{
	auto peer = get_server(server);
	auto method = Server_get_price::create();
	method->want = want;
	method->have = have;
	send_request(peer, method, std::bind(&Client::vnx_async_return, this, request_id, std::placeholders::_1));
}

void Client::connect()
{
	for(const auto& entry : server_map) {
		if(!avail_server_map.count(entry.second) && !connecting.count(entry.second))
		{
			const auto address = entry.second;
			log(DEBUG) << "Trying to connect to " << address;

			connecting.insert(address);
			threads->add_task(std::bind(&Client::connect_task, this, address));
		}
	}
}

void Client::add_peer(const std::string& address, const int sock)
{
	connecting.erase(address);
	if(sock >= 0) {
		add_client(sock, address);
	}
}

void Client::connect_task(const std::string& address) noexcept
{
	auto peer = vnx::TcpEndpoint::from_url(address);
	int sock = -1;
	try {
		sock = peer->open();
		peer->connect(sock);
	}
	catch(const std::exception& ex) {
		if(sock >= 0) {
			peer->close(sock);
			sock = -1;
		}
		if(show_warnings) {
			log(WARN) << "Connecting to server " << address << " failed with: " << ex.what();
		}
	}
	if(vnx::do_run()) {
		add_task(std::bind(&Client::add_peer, this, address, sock));
	}
}

void Client::send_to(uint64_t client, std::shared_ptr<const vnx::Value> msg, bool reliable)
{
	if(auto peer = find_peer(client)) {
		send_to(peer, msg, reliable);
	}
}

void Client::send_to(std::shared_ptr<peer_t> peer, std::shared_ptr<const vnx::Value> msg, bool reliable)
{
	Super::send_to(peer, msg);
}

uint32_t Client::send_request(	std::shared_ptr<peer_t> peer, std::shared_ptr<const vnx::Value> method,
								const std::function<void(std::shared_ptr<const vnx::Value>)>& callback) const
{
	auto req = Request::create();
	req->id = next_request_id++;
	req->method = method;
	((Client*)this)->send_to(peer, req, true);

	peer->pending.insert(req->id);
	return_map[req->id] = callback;
	return req->id;
}

void Client::on_error(uint64_t client, uint32_t id, const vnx::exception& ex)
{
	auto ret = Return::create();
	ret->id = id;
	ret->result = ex.value();
	send_to(client, ret);
}

void Client::on_request(uint64_t client, std::shared_ptr<const Request> msg)
{
}

void Client::on_return(uint64_t client, std::shared_ptr<const Return> msg)
{
	auto iter = return_map.find(msg->id);
	if(iter != return_map.end()) {
		if(auto peer = find_peer(client)) {
			peer->pending.erase(msg->id);
		}
		const auto callback = std::move(iter->second);
		return_map.erase(iter);
		if(callback) {
			callback(msg->result);
		}
	} else if(auto ex = std::dynamic_pointer_cast<const vnx::Exception>(msg->result)) {
		log(WARN) << "Request failed with: " << ex->what;
	}
}

void Client::on_msg(uint64_t client, std::shared_ptr<const vnx::Value> msg)
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
	case Client_approve::VNX_TYPE_ID:
		if(auto value = std::dynamic_pointer_cast<const Client_approve>(msg)) {
			try {
				auto ret = Client_approve_return::create();
				ret->_ret_0 = approve(value->tx);
				send_to(client, ret);
			}
			catch(const std::exception& ex) {
				auto ret = vnx::Exception::create();
				ret->what = ex.what();
				send_to(client, ret);
				log(WARN) << "approve() failed with: " << ex.what();
			}
		}
		break;
	}
}


void Client::on_pause(uint64_t client)
{
}

void Client::on_resume(uint64_t client)
{
}

void Client::on_connect(uint64_t client, const std::string& address)
{
	auto peer = std::make_shared<peer_t>();
	peer->client = client;
	peer->address = address;
	peer_map[client] = peer;
	avail_server_map[address] = client;

	for(const auto& entry : offer_map) {
		send_offer(client, entry.second);
	}
	log(INFO) << "Connected to server " << peer->address;
}

void Client::on_disconnect(uint64_t client)
{
	if(auto peer = find_peer(client)) {
		for(const auto id : peer->pending) {
			auto iter = return_map.find(id);
			if(iter != return_map.end()) {
				const auto callback = std::move(iter->second);
				return_map.erase(iter);
				if(callback) {
					callback(vnx::Exception::from_what("server disconnect"));
				}
			}
		}
		avail_server_map.erase(peer->address);

		log(INFO) << "Server " << peer->address << " disconnected";
	}
	add_task([this, client]() {
		peer_map.erase(client);
	});
}

std::shared_ptr<Client::Super::peer_t> Client::get_peer_base(uint64_t client) const
{
	return get_peer(client);
}

std::shared_ptr<Client::peer_t> Client::get_peer(uint64_t client) const
{
	if(auto peer = find_peer(client)) {
		return peer;
	}
	throw std::logic_error("no such peer");
}

std::shared_ptr<Client::peer_t> Client::find_peer(uint64_t client) const
{
	auto iter = peer_map.find(client);
	if(iter != peer_map.end()) {
		return iter->second;
	}
	return nullptr;
}

std::shared_ptr<Client::peer_t> Client::get_server(const std::string& name) const
{
	auto iter = server_map.find(name);
	if(iter != server_map.end()) {
		const auto& address = iter->second;
		auto iter = avail_server_map.find(address);
		if(iter != avail_server_map.end()) {
			return get_peer(iter->second);
		}
	}
	throw std::logic_error("no such server");
}


} // exchange
} // mmx
