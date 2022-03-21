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
#include <mmx/exchange/Server_cancel.hxx>
#include <mmx/exchange/Server_cancel_return.hxx>
#include <mmx/exchange/Server_execute.hxx>
#include <mmx/exchange/Server_execute_return.hxx>
#include <mmx/exchange/Server_match.hxx>
#include <mmx/exchange/Server_match_return.hxx>
#include <mmx/exchange/Server_get_trade_pairs.hxx>
#include <mmx/exchange/Server_get_orders.hxx>
#include <mmx/exchange/Server_get_history.hxx>
#include <mmx/exchange/Server_get_price.hxx>
#include <mmx/exchange/Server_get_min_trade.hxx>
#include <mmx/exchange/Server_ping.hxx>
#include <mmx/exchange/Server_reject.hxx>
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

	http = std::make_shared<vnx::addons::HttpInterface<Client>>(this, vnx_name);
	add_async_client(http);

	threads = new vnx::ThreadPool(1);

	vnx::Directory(storage_path).create();
	{
		::rocksdb::Options options;
		options.max_open_files = 4;
		options.keep_log_file_num = 1;
		options.max_manifest_file_size = 8 * 1024 * 1024;
		options.OptimizeForSmallDb();

		offer_table.open(storage_path + "offer_table", options);
	}

	trade_log = std::make_shared<vnx::File>(storage_path + "trade_log.dat");
	if(trade_log->exists()) {
		int64_t offset = 0;
		trade_log->open("rb+");
		while(true) {
			try {
				offset = trade_log->get_input_pos();
				if(auto value = vnx::read(trade_log->in)) {
					if(auto trade = std::dynamic_pointer_cast<LocalTrade>(value)) {
						trade_history.emplace(trade->height, trade);
					}
				} else {
					break;
				}
			} catch(const std::exception& ex) {
				log(WARN) << ex.what();
				break;
			}
		}
		trade_log->seek_to(offset);
	} else {
		trade_log->open("wb");
	}

	set_timer_millis(10 * 1000, std::bind(&Client::update, this));
	set_timer_millis(60 * 1000, std::bind(&Client::connect, this));
	if(post_interval > 0) {
		set_timer_millis(post_interval * 1000, std::bind(&Client::post_offers, this));
	}

	set_timeout_millis(2000, [this]() {
		offer_table.scan(
			[this](const uint64_t& id, const std::shared_ptr<OfferBundle>& offer) {
				place(offer);
			});
	});

	connect();

	Super::main();

	{
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
	{
		vnx::write(trade_log->out, nullptr);
		trade_log->close();
	}
}

void Client::update()
{
	// send keep alive
	for(const auto& entry : avail_server_map) {
		auto req = Request::create();
		req->method = Server_ping::create();
		send_to(entry.second, req);
	}
	while(trade_history.size() > max_trade_history) {
		trade_history.erase(trade_history.begin());
	}
}

void Client::handle(std::shared_ptr<const Block> block)
{
	if(!block->proof) {
		return;
	}
	std::unordered_map<uint32_t, std::vector<txio_key_t>> sold_map;

	for(const auto& base : block->tx_list) {
		if(auto tx = std::dynamic_pointer_cast<const Transaction>(base)) {
			// check for completed offers
			if(pending_approvals.count(tx->id)) {
				std::map<uint64_t, std::shared_ptr<LocalTrade>> trade_map;
				for(const auto& in : tx->inputs) {
					auto iter = order_map.find(in.prev);
					if(iter != order_map.end()) {
						const auto& order = iter->second;
						if(auto offer = find_offer(order.offer_id)) {
							offer->bid_sold += order.bid.amount;
							sold_map[offer->wallet].push_back(in.prev);
						}
						auto& trade = trade_map[order.offer_id];
						if(!trade) {
							trade = LocalTrade::create();
							trade->id = tx->id;
							trade->height = block->height;
							trade->pair.bid = order.bid.contract;
							trade->pair.ask = order.ask.currency;
							trade->offer_id = order.offer_id;
						}
						trade->bid += order.bid.amount;
						trade->ask += order.ask.amount;
						order_map.erase(iter);
					}
				}
				for(const auto& entry : trade_map) {
					const auto trade = entry.second;
					try {
						vnx::write(trade_log->out, trade);
						trade_log->flush();
					} catch(const std::exception& ex) {
						log(WARN) << ex.what();
					}
					trade_history.emplace(trade->height, trade);
					log(INFO) << "Sold " << trade->bid << " [" << trade->pair.bid << "] for " << trade->ask << " [" << trade->pair.ask << "]";
				}
				pending_approvals.erase(tx->id);
			}
			{
				auto iter = pending_trades.find(tx->id);
				if(iter != pending_trades.end()) {
					auto trade = iter->second;
					trade->height = block->height;
					try {
						vnx::write(trade_log->out, trade);
						trade_log->flush();
					} catch(const std::exception& ex) {
						log(WARN) << ex.what();
					}
					trade_history.emplace(trade->height, trade);
					pending_trades.erase(iter);
				}
			}
		}
	}
	for(const auto& entry : sold_map) {
		wallet->release(entry.first, entry.second);
	}

	std::vector<uint64_t> posted_offers;
	for(const auto& entry : pending_offers) {
		if(try_place(entry.second)) {
			posted_offers.push_back(entry.first);
		}
	}
	for(auto id : posted_offers) {
		pending_offers.erase(id);
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

open_order_t Client::get_order(const txio_key_t& key) const
{
	auto iter = order_map.find(key);
	if(iter != order_map.end()) {
		return iter->second;
	}
	throw std::runtime_error("no such order");
}

std::shared_ptr<OfferBundle> Client::find_offer(const uint64_t& id) const
{
	auto iter = offer_map.find(id);
	if(iter != offer_map.end()) {
		return iter->second;
	}
	return nullptr;
}

std::shared_ptr<const OfferBundle> Client::get_offer(const uint64_t& id) const
{
	return find_offer(id);
}

std::vector<std::shared_ptr<const OfferBundle>> Client::get_all_offers() const
{
	std::vector<std::shared_ptr<const OfferBundle>> res;
	for(const auto& entry : offer_map) {
		res.push_back(entry.second);
	}
	return res;
}

std::vector<std::shared_ptr<const LocalTrade>>
Client::get_local_history(const vnx::optional<trade_pair_t>& pair, const int32_t& limit) const
{
	const trade_pair_t reverse = pair ? pair->reverse() : trade_pair_t();

	std::vector<std::shared_ptr<const LocalTrade>> result;
	for(const auto& entry : trade_history) {
		const auto& trade = entry.second;
		if(!pair || trade->pair == *pair) {
			result.push_back(trade);
		}
		if(pair && trade->pair == reverse) {
			result.push_back(trade->reverse());
		}
	}
	for(const auto& entry : pending_trades) {
		const auto& trade = entry.second;
		if(!pair || trade->pair == *pair) {
			result.push_back(trade);
		}
		if(pair && trade->pair == reverse) {
			result.push_back(trade->reverse());
		}
	}
	std::reverse(result.begin(), result.end());
	if(result.size() > size_t(limit)) {
		result.resize(limit);
	}
	return result;
}

void Client::cancel_offer(const uint64_t& id)
{
	auto iter = offer_map.find(id);
	if(iter == offer_map.end()) {
		throw std::logic_error("no such offer");
	}
	auto offer = iter->second;
	auto method = Server_cancel::create();
	for(const auto& order : offer->limit_orders) {
		for(const auto& entry : order.bids) {
			method->orders.push_back(entry.first);
		}
	}
	for(const auto& entry : avail_server_map) {
		auto req = Request::create();
		req->id = next_request_id++;
		req->method = method;
		send_to(entry.second, req);
	}
	for(const auto& key : method->orders) {
		order_map.erase(key);
	}
	offer_table.erase(id);
	offer_map.erase(iter);
	pending_offers.erase(id);
	wallet->release(offer->wallet, method->orders);
}

void Client::cancel_all()
{
	std::vector<uint64_t> ids;
	for(const auto& entry : offer_map) {
		ids.push_back(entry.first);
	}
	for(const auto id : ids) {
		cancel_offer(id);
	}
}

std::shared_ptr<const OfferBundle>
Client::make_offer(const uint32_t& index, const trade_pair_t& pair, const uint64_t& bid, const uint64_t& ask, const uint32_t& num_chunks) const
{
	if(bid == 0 || ask == 0 || num_chunks == 0) {
		throw std::logic_error("invalid argument");
	}
	if(num_chunks > 1000) {
		throw std::logic_error("num_chunks > 1000");
	}
	auto offer = OfferBundle::create();
	offer->id = next_offer_id++;
	offer->wallet = index;
	offer->pair = pair;

	limit_order_t limit_order;
	const auto price = ask / double(bid);
	const auto address = wallet->get_address(index, 0);

	auto tx_ = Transaction::create();
	tx_->add_output(pair.bid, address, bid, num_chunks);
	auto tx = wallet->complete(index, tx_);
	offer->generator.push_back(tx);

	for(uint32_t i = 0; i < num_chunks && i < tx->outputs.size(); ++i)
	{
		const auto key = txio_key_t::create_ex(tx->id, i);
		const auto& utxo = tx->outputs[i];
		open_order_t order;
		order.wallet = index;
		order.offer_id = offer->id;
		order.bid = utxo;
		order.ask.amount = utxo.amount * price;
		order.ask.currency = pair.ask;
		offer->bid += order.bid.amount;
		offer->ask += order.ask.amount;
		offer->orders.emplace_back(key, order);
		limit_order.bids.emplace_back(key, order.ask.amount);
	}
	{
		const auto hash = limit_order.calc_hash();
		limit_order.solution = wallet->sign_msg(index, address, hash);
		offer->limit_orders.push_back(limit_order);
	}
	return offer;
}

trade_order_t
Client::make_trade(const uint32_t& index, const trade_pair_t& pair, const uint64_t& bid, const vnx::optional<uint64_t>& ask) const
{
	const double price = ask ? *ask / double(bid) : 0;

	trade_order_t order;
	order.pair = pair;
	if(ask) {
		order.ask = uint64_t(0);
	}
	order.ask_addr = wallet->get_address(index, 0);

	uint64_t bid_left = bid;
	std::unordered_set<addr_t> addr_set;
	for(const auto& entry : wallet->gather_utxos_for(index, bid, pair.bid)) {
		if(!bid_left) {
			break;
		}
		const auto& utxo = entry.output;
		const auto amount = std::min(utxo.amount, bid_left);
		if(ask) {
			*order.ask += amount * price;
		}
		order.bid += amount;
		order.bid_keys.push_back(entry.key);
		addr_set.insert(utxo.address);
		bid_left -= amount;
	}
	if(addr_set.empty()) {
		throw std::logic_error("no funds to trade with");
	}
	const auto hash = order.calc_hash();

	for(const auto& addr : addr_set) {
		order.solutions.push_back(wallet->sign_msg(index, addr, hash));
	}
	return order;
}

void Client::send_offer(uint64_t server, std::shared_ptr<const OfferBundle> offer)
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

void Client::send_offer(std::shared_ptr<const OfferBundle> offer)
{
	for(const auto& entry : avail_server_map) {
		send_offer(entry.second, offer);
	}
}

void Client::place(std::shared_ptr<const OfferBundle> offer)
{
	if(!offer->bid || !offer->ask) {
		throw std::logic_error("empty offer");
	}
	if(offer_map.count(offer->id)) {
		throw std::logic_error("offer already exists");
	}
	auto copy = vnx::clone(offer);
	if(!try_place(copy)) {
		pending_offers[copy->id] = copy;
	}
}

bool Client::try_place(std::shared_ptr<OfferBundle> offer)
{
	std::vector<txio_key_t> keys;
	for(const auto& order : offer->orders) {
		keys.push_back(order.first);
	}
	const auto height = node->get_synced_height();

	offer->bid_sold = 0;
	bool is_ready = true;
	bool is_generated = true;
	if(height) {
		for(const auto& entry : node->get_txo_infos(keys)) {
			if(entry) {
				const auto& utxo = entry->output;
				if(entry->spent) {
					offer->bid_sold += utxo.amount;
				}
				if(utxo.height + min_confirm > *height + 1) {
					is_ready = false;
				}
			} else {
				is_ready = false;
				is_generated = false;
			}
		}
	} else {
		is_ready = false;
	}
	if(offer->bid_sold >= offer->bid) {
		offer_table.erase(offer->id);
		return true;
	}
	if(!is_generated) {
		for(auto tx : offer->generator) {
			wallet->send_off(offer->wallet, tx);
			log(INFO) << "Sent transaction for offer " << offer->id << " (" << tx->id << ")";
		}
	}
	if(!pending_offers.count(offer->id)) {
		next_offer_id = std::max(offer->id + 1, next_offer_id);
		order_map.insert(offer->orders.begin(), offer->orders.end());
		offer_map[offer->id] = offer;
		offer_table.insert(offer->id, offer);
		wallet->reserve(offer->wallet, keys);
	}
	if(is_ready) {
		send_offer(offer);
		log(INFO) << "Placed offer " << offer->id << ", asking "
				<< offer->ask << " [" << offer->pair.ask << "] for " << offer->bid << " [" << offer->pair.bid << "]"
				<< " (" << avail_server_map.size() << " servers)";
	}
	return is_ready;
}

void Client::post_offers()
{
	for(const auto& entry : offer_map) {
		if(!pending_offers.count(entry.first)) {
			send_offer(entry.second);
		}
	}
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
			const uint64_t amount = iter != output_amount.end() ? iter->second : 0;
			throw std::logic_error("expected amount: " + std::to_string(entry.second) + " != " + std::to_string(amount) + " [" + entry.first.to_string() + "]");
		}
	}
	auto out = vnx::clone(tx);
	for(auto index : wallets) {
		if(auto tx = wallet->sign_off(index, out, false)) {
			out = vnx::clone(tx);
		} else {
			throw std::logic_error("unable to sign off");
		}
	}
	pending_approvals.insert(out->id);
	log(INFO) << "Accepted trade " << out->id;
	return out;
}

void Client::execute_async(const std::string& server, const uint32_t& index, const matched_order_t& order, const vnx::request_id_t& request_id) const
{
	if(!order.tx) {
		throw std::logic_error("!tx");
	}
	auto peer = get_server(server);

	std::unordered_set<addr_t> addr_set;
	for(const auto& addr : wallet->get_all_addresses(index)) {
		addr_set.insert(addr);
	}
	std::vector<txio_key_t> keys;
	for(const auto& in : order.tx->inputs) {
		keys.push_back(in.prev);
	}

	uint64_t total_bid = 0;
	vnx::optional<addr_t> change_addr;
	std::vector<std::pair<txio_key_t, utxo_t>> utxo_list;
	for(const auto& entry : node->get_txo_infos(keys)) {
		if(!entry) {
			throw std::logic_error("unknown input");
		}
		if(entry->spent) {
			throw std::logic_error("input already spent");
		}
		const auto& utxo = entry->output;
		if(addr_set.count(utxo.address)) {
			if(utxo.contract == order.pair.bid) {
				total_bid += utxo.amount;
			} else {
				throw std::logic_error("invalid bid");
			}
			change_addr = utxo.address;
		} else {
			utxo_list.emplace_back(entry->key, entry->output);
		}
	}
	if(!change_addr) {
		throw std::logic_error("have no change address");
	}
	uint64_t total_ask = 0;
	for(const auto& out : order.tx->outputs) {
		if(addr_set.count(out.address)) {
			if(out.contract == order.pair.ask) {
				total_ask += out.amount;
			} else {
				throw std::logic_error("invalid ask");
			}
		}
	}
	if(total_bid < order.bid) {
		throw std::logic_error("insufficient bid amount");
	}
	if(total_ask != order.ask) {
		throw std::logic_error("ask amount mismatch");
	}

	auto copy = vnx::clone(order.tx);
	if(total_bid > order.bid && order.pair.bid != addr_t()) {
		// token change output
		tx_out_t out;
		out.address = *change_addr;
		out.contract = order.pair.bid;
		out.amount = total_bid - order.bid;
		copy->outputs.push_back(out);
	}

	spend_options_t options;
	options.utxo_map = utxo_list;
	auto tx = wallet->sign_off(index, copy, true, options);
	if(!tx) {
		throw std::logic_error("failed to sign off");
	}

	keys.clear();
	for(const auto& in : tx->inputs) {
		keys.push_back(in.prev);
	}
	wallet->reserve(index, keys);

	auto trade = LocalTrade::create();
	trade->id = tx->id;
	trade->pair = order.pair;
	trade->bid = order.bid;
	trade->ask = order.ask;

	auto method = Server_execute::create();
	method->tx = tx;
	send_request(peer, method,
		[this, request_id, index, keys, trade, tx](std::shared_ptr<const vnx::Value> result) {
			if(auto ex = std::dynamic_pointer_cast<const vnx::Exception>(result)) {
				wallet->release(index, keys);
				vnx_async_return(request_id, ex);
				try {
					trade->failed = true;
					trade->message = ex->what;
					vnx::write(trade_log->out, trade);
					trade_log->flush();
				} catch(const std::exception& ex) {
					log(WARN) << ex.what();
				}
				trade_history.emplace(trade->height, trade);
			} else {
				wallet->mark_spent(index, keys);
				execute_async_return(request_id, tx->id);
				pending_trades[tx->id] = trade;
			}
		});
}

void Client::match_async(const std::string& server, const trade_order_t& order, const vnx::request_id_t& request_id) const
{
	auto peer = get_server(server);
	auto method = Server_match::create();
	method->order = order;
	send_request(peer, method, std::bind(&Client::vnx_async_return, this, request_id, std::placeholders::_1));
}

void Client::get_trade_pairs_async(const std::string& server, const vnx::request_id_t& request_id) const
{
	auto peer = get_server(server);
	auto method = Server_get_trade_pairs::create();
	send_request(peer, method, std::bind(&Client::vnx_async_return, this, request_id, std::placeholders::_1));
}

void Client::get_orders_async(const std::string& server, const trade_pair_t& pair, const int32_t& limit, const vnx::request_id_t& request_id) const
{
	auto peer = get_server(server);
	auto method = Server_get_orders::create();
	method->pair = pair;
	method->limit = limit;
	send_request(peer, method, std::bind(&Client::vnx_async_return, this, request_id, std::placeholders::_1));
}

void Client::get_trade_history_async(const std::string& server, const trade_pair_t& pair, const int32_t& limit, const vnx::request_id_t& request_id) const
{
	auto peer = get_server(server);
	auto method = Server_get_history::create();
	method->pair = pair;
	method->limit = limit;
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

void Client::get_min_trade_async(const std::string& server, const trade_pair_t& pair, const vnx::request_id_t& request_id) const
{
	auto peer = get_server(server);
	auto method = Server_get_min_trade::create();
	method->pair = pair;
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
			if(auto tx = value->tx) {
				try {
					auto ret = Client_approve_return::create();
					ret->_ret_0 = approve(tx);
					send_to(client, ret);
				}
				catch(const std::exception& ex) {
					auto ret = Server_reject::create();
					ret->txid = tx->id;
					send_to(client, ret);
					log(WARN) << "approve() failed with: " << ex.what();
				}
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

void Client::http_request_async(std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
								const vnx::request_id_t& request_id) const
{
	http->http_request(request, sub_path, request_id);
}

void Client::http_request_chunk_async(	std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
										const int64_t& offset, const int64_t& max_bytes, const vnx::request_id_t& request_id) const
{
	throw std::logic_error("not implemented");
}


} // exchange
} // mmx
