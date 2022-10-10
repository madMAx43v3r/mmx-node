/*
 * Router.cpp
 *
 *  Created on: Dec 17, 2021
 *      Author: mad
 */

#include <mmx/Router.h>
#include <mmx/hash_t.hpp>
#include <mmx/skey_t.hpp>
#include <mmx/pubkey_t.hpp>
#include <mmx/utils.h>

#include <mmx/Return.hxx>
#include <mmx/Request.hxx>
#include <mmx/Router_get_id.hxx>
#include <mmx/Router_get_id_return.hxx>
#include <mmx/Router_get_info.hxx>
#include <mmx/Router_get_info_return.hxx>
#include <mmx/Router_sign_msg.hxx>
#include <mmx/Router_sign_msg_return.hxx>
#include <mmx/Router_get_peers.hxx>
#include <mmx/Router_get_peers_return.hxx>
#include <mmx/Node_get_height.hxx>
#include <mmx/Node_get_height_return.hxx>
#include <mmx/Node_get_synced_height.hxx>
#include <mmx/Node_get_synced_height_return.hxx>
#include <mmx/Node_get_header.hxx>
#include <mmx/Node_get_header_return.hxx>
#include <mmx/Node_get_header_at.hxx>
#include <mmx/Node_get_header_at_return.hxx>
#include <mmx/Node_get_block.hxx>
#include <mmx/Node_get_block_return.hxx>
#include <mmx/Node_get_block_at.hxx>
#include <mmx/Node_get_block_at_return.hxx>
#include <mmx/Node_get_block_hash.hxx>
#include <mmx/Node_get_block_hash_return.hxx>
#include <mmx/Node_get_tx_ids_at.hxx>
#include <mmx/Node_get_tx_ids_at_return.hxx>

#include <vnx/vnx.h>
#include <vnx/NoSuchMethod.hxx>
#include <vnx/OverflowException.hxx>

#include <algorithm>


namespace mmx {

Router::Router(const std::string& _vnx_name)
	:	RouterBase(_vnx_name)
{
	params = get_params();
	port = params->port;
}

void Router::init()
{
	Super::init();
	vnx::open_pipe(vnx_name, this, max_queue_ms);
}

void Router::main()
{
	if(min_sync_peers < 1) {
		throw std::logic_error("min_sync_peers < 1");
	}
	if(num_peers_out < min_sync_peers) {
		throw std::logic_error("num_peers_out < min_sync_peers");
	}
	if(max_connections >= 0) {
		if(num_peers_out > uint32_t(max_connections)) {
			throw std::logic_error("num_peers_out > max_connections");
		}
		if(min_sync_peers > uint32_t(max_connections)) {
			throw std::logic_error("min_sync_peers > max_connections");
		}
	}
	tx_upload_bandwidth = max_tx_upload * to_value(params->max_block_cost, params) / params->block_time;
	max_pending_cost_value = max_pending_cost * to_value(params->max_block_cost, params);

	log(INFO) << "Global TX upload limit: " << tx_upload_bandwidth << " MMX/s";
	log(INFO) << "Peer TX pending limit: " << max_pending_cost_value << " MMX";

	subscribe(input_vdfs, max_queue_ms);
	subscribe(input_verified_vdfs, max_queue_ms);
	subscribe(input_verified_proof, max_queue_ms);
	subscribe(input_verified_blocks, max_queue_ms);
	subscribe(input_verified_transactions, max_queue_ms);
	subscribe(input_transactions, max_queue_ms);

	{
		vnx::File file(storage_path + "known_peers.dat");
		if(file.exists()) {
			std::vector<std::string> peers;
			try {
				file.open("rb");
				vnx::read_generic(file.in, peers);
				file.close();
			}
			catch(const std::exception& ex) {
				log(WARN) << "Failed to read peers from file: " << ex.what();
			}
			peer_set.insert(peers.begin(), peers.end());
		}
		log(INFO) << "Loaded " << peer_set.size() << " known peers";
	}
	{
		vnx::File file(storage_path + "farmer_credits.dat");
		if(file.exists()) {
			try {
				file.open("rb");
				vnx::read_generic(file.in, farmer_credits);
				file.close();
			}
			catch(const std::exception& ex) {
				log(WARN) << "Failed to read farmer credits from file: " << ex.what();
			}
		}
		log(INFO) << "Loaded " << farmer_credits.size() << " farmer credits";
	}
	{
		vnx::File file(storage_path + "node_sk.dat");
		if(file.exists()) {
			try {
				file.open("rb");
				vnx::read_generic(file.in, node_sk);
				file.close();
			}
			catch(const std::exception& ex) {
				log(WARN) << "Failed to read node key from file: " << ex.what();
			}
		}
		if(node_sk == skey_t()) {
			node_sk = hash_t::random();
			try {
				file.open("wb");
				vnx::write_generic(file.out, node_sk);
				file.close();
			}
			catch(const std::exception& ex) {
				log(WARN) << "Failed to write node key to file: " << ex.what();
			}
		}
		node_key = pubkey_t::from_skey(node_sk);
		node_id = node_key.get_addr();
		log(INFO) << "Our Node ID: " << node_id;
	}

	node = std::make_shared<NodeAsyncClient>(node_server);
	node->vnx_set_non_blocking(true);
	add_async_client(node);

	http = std::make_shared<vnx::addons::HttpInterface<Router>>(this, vnx_name);
	add_async_client(http);

	connect_threads = new vnx::ThreadPool(-1);

	set_timer_millis(send_interval_ms, std::bind(&Router::send, this));
	set_timer_millis(query_interval_ms, std::bind(&Router::query, this));
	set_timer_millis(update_interval_ms, std::bind(&Router::update, this));
	set_timer_millis(connect_interval_ms, std::bind(&Router::connect, this));
	set_timer_millis(discover_interval * 1000, std::bind(&Router::discover, this));
	set_timer_millis(5 * 60 * 1000, std::bind(&Router::save_data, this));

	connect();

	Super::main();

	save_data();
}

hash_t Router::get_id() const
{
	return node_id;
}

node_info_t Router::get_info() const
{
	node_info_t info;
	info.id = node_id;
	info.version = node_version;
	info.type = mode;
	return info;
}

std::pair<pubkey_t, signature_t> Router::sign_msg(const hash_t& msg) const
{
	return std::make_pair(node_key, signature_t::sign(node_sk, hash_t(msg.bytes)));
}

static
bool is_public_address(const std::string& addr)
{
	if(addr.substr(0, 3) == "10." || addr.substr(0, 4) == "127." || addr.substr(0, 8) == "192.168.") {
		return false;
	}
	return true;
}

template<typename T, typename R>
std::vector<T> get_subset(const std::set<T>& candidates, const size_t max_count, R& engine)
{
	std::vector<T> result(candidates.begin(), candidates.end());
	if(max_count < result.size()) {
		std::shuffle(result.begin(), result.end(), engine);
		result.resize(max_count);
	}
	return result;
}

std::vector<std::string> Router::get_peers(const uint32_t& max_count) const
{
	auto peers = get_known_peers();
	const auto connected = get_connected_peers();
	peers.insert(peers.end(), connected.begin(), connected.end());

	std::set<std::string> valid;
	for(const auto& addr : peers) {
		if(is_public_address(addr)) {
			valid.insert(addr);
		}
	}
	return get_subset(valid, max_count, rand_engine);
}

std::vector<std::string> Router::get_known_peers() const
{
	return std::vector<std::string>(peer_set.begin(), peer_set.end());
}

std::vector<std::string> Router::get_connected_peers() const
{
	std::vector<std::string> res;
	for(const auto& entry : peer_map) {
		res.push_back(entry.second->address);
	}
	return res;
}

std::shared_ptr<const PeerInfo> Router::get_peer_info() const
{
	const auto now_ms = vnx::get_wall_time_millis();

	auto info = PeerInfo::create();
	for(const auto& entry : peer_map) {
		const auto& state = entry.second;
		peer_info_t peer;
		peer.id = entry.first;
		peer.type = state->info.type;
		peer.address = state->address;
		peer.height = state->height;
		peer.version = state->info.version;
		peer.credits = state->credits;
		peer.ping_ms = state->ping_ms;
		peer.bytes_send = state->bytes_send;
		peer.bytes_recv = state->bytes_recv;
		peer.pending_cost = state->pending_cost;
		peer.is_synced = state->is_synced;
		peer.is_blocked = state->is_blocked;
		peer.is_outbound = state->is_outbound;
		peer.recv_timeout_ms = now_ms - state->last_receive_ms;
		peer.connect_time_ms = now_ms - state->connected_since_ms;
		info->peers.push_back(peer);
	}
	std::sort(info->peers.begin(), info->peers.end(),
		[](const peer_info_t& lhs, const peer_info_t& rhs) -> bool {
			return lhs.connect_time_ms > rhs.connect_time_ms;
		});
	return info;
}

std::vector<std::pair<std::string, uint32_t>> Router::get_farmer_credits() const
{
	std::vector<std::pair<std::string, uint32_t>> res;
	for(const auto& entry : farmer_credits) {
		res.emplace_back(entry.first.to_string(), entry.second);
	}
	return res;
}

void Router::get_blocks_at_async(const uint32_t& height, const vnx::request_id_t& request_id) const
{
	auto job = std::make_shared<sync_job_t>();
	job->height = height;
	job->start_time_ms = vnx::get_wall_time_millis();
	sync_jobs[request_id] = job;
	((Router*)this)->process();
}

void Router::fetch_block_async(const hash_t& hash, const vnx::optional<std::string>& address, const vnx::request_id_t& request_id) const
{
	auto job = std::make_shared<fetch_job_t>();
	job->hash = hash;
	job->from_peer = address;
	job->start_time_ms = vnx::get_wall_time_millis();
	fetch_jobs[request_id] = job;
	((Router*)this)->process();
}

void Router::fetch_block_at_async(const uint32_t& height, const std::string& address, const vnx::request_id_t& request_id) const
{
	auto job = std::make_shared<fetch_job_t>();
	job->height = height;
	job->from_peer = address;
	job->start_time_ms = vnx::get_wall_time_millis();
	fetch_jobs[request_id] = job;
	((Router*)this)->process();
}

void Router::handle(std::shared_ptr<const Block> block)
{
	if(block->farmer_sig) {
		const auto& hash = block->content_hash;
		if(relay_msg_hash(hash, block_credits)) {
			log(INFO) << "Broadcasting block for height " << block->height;
			broadcast(block, hash, {node_type_e::FULL_NODE, node_type_e::LIGHT_NODE});
			block_counter++;
		}
	}
	verified_peak_height = block->height;
}

void Router::handle(std::shared_ptr<const Transaction> tx)
{
	const auto& hash = tx->content_hash;
	if(relay_msg_hash(hash)) {
		if(vnx_sample->topic == input_transactions) {
			broadcast(tx, hash, {node_type_e::FULL_NODE});
		} else {
			relay(tx, hash, {node_type_e::FULL_NODE});
		}
		tx_counter++;
	}
}

void Router::handle(std::shared_ptr<const ProofOfTime> value)
{
	if(value->height > verified_peak_height) {
		const auto& hash = value->content_hash;
		if(relay_msg_hash(hash, vdf_credits)) {
			if(vnx_sample && vnx_sample->topic == input_vdfs) {
				log(INFO) << "Broadcasting VDF for height " << value->height;
			}
			broadcast(value, hash, {node_type_e::FULL_NODE, node_type_e::LIGHT_NODE});
			vdf_counter++;
		}
	}
}

void Router::handle(std::shared_ptr<const ProofResponse> value_)
{
	if(!value_->proof || !value_->request) {
		return;
	}
	auto value = vnx::clone(value_);
	value->harvester.clear();
	value->content_hash = value->calc_hash(true);

	const auto& hash = value->content_hash;
	if(relay_msg_hash(hash, proof_credits)) {
		if(vnx::get_pipe(value->farmer_addr)) {
			log(INFO) << "Broadcasting proof for height " << value->request->height << " with score " << value->proof->score;
		}
		broadcast(value, hash, {node_type_e::FULL_NODE});
		proof_counter++;
	}
	const auto farmer_id = hash_t(value->proof->farmer_key);
	farmer_credits[farmer_id] += proof_credits;
}

uint32_t Router::send_request(std::shared_ptr<peer_t> peer, std::shared_ptr<const vnx::Value> method, bool reliable)
{
	auto req = Request::create();
	req->id = next_request_id++;
	req->method = method;
	send_to(peer, req, reliable);
	return req->id;
}

uint32_t Router::send_request(uint64_t client, std::shared_ptr<const vnx::Value> method, bool reliable)
{
	auto req = Request::create();
	req->id = next_request_id++;
	req->method = method;
	send_to(client, req, reliable);
	return req->id;
}

void Router::update()
{
	const auto now_ms = vnx::get_wall_time_millis();

	for(const auto& entry : peer_map) {
		const auto& peer = entry.second;
		peer->credits = std::min(peer->credits, max_node_credits);
		peer->pending_cost = std::max<double>(peer->pending_cost, 0) * 0.99;

		// clear old hashes
		while(peer->hash_queue.size() > max_sent_cache) {
			peer->sent_hashes.erase(peer->hash_queue.front());
			peer->hash_queue.pop();
		}
		// check for timeout
		if(connection_timeout_ms > 0 && now_ms - std::max(peer->last_receive_ms, peer->connected_since_ms) > connection_timeout_ms) {
			log(INFO) << "Peer " << peer->address << " timed out";
			disconnect(entry.first);
		}
		// check for manual disconnect
		if(disconnect_interval && !peer->is_outbound && (now_ms - peer->connected_since_ms) / 1000 > disconnect_interval) {
			log(INFO) << "Disconnecting peer " << peer->address << " due to interval exceeded";
			disconnect(entry.first);
		}
	}

	// limit farmer credits
	for(auto& entry : farmer_credits) {
		entry.second = std::min(entry.second, max_farmer_credits);
	}

	// clear seen hashes
	while(hash_queue.size() > max_hash_cache) {
		hash_info.erase(hash_queue.front());
		hash_queue.pop();
	}

	// check if we lost sync due to response timeout
	{
		size_t num_peers = 0;
		for(const auto& entry : peer_map) {
			const auto& peer = entry.second;
			if(peer->is_synced && now_ms - peer->last_receive_ms < sync_loss_delay * 1000) {
				num_peers++;
			}
		}
		if(num_peers < min_sync_peers) {
			node->get_synced_height([this](const vnx::optional<uint32_t>& height) {
				if(is_connected && height) {
					log(WARN) << "Lost sync with network due to lost peers!";
					is_connected = false;
					node->start_sync();
				}
			});
		} else {
			is_connected = true;
		}
	}

	if(synced_peers.size() >= min_sync_peers)
	{
		// check for sync job timeouts
		for(auto& entry : sync_jobs) {
			auto& job = entry.second;
			if(now_ms - std::max(job->start_time_ms, job->last_recv_ms) > fetch_timeout_ms) {
				for(auto client : job->pending) {
					synced_peers.erase(client);
				}
				{
					const auto height = job->height;
					job = std::make_shared<sync_job_t>();
					job->height = height;
				}
				job->start_time_ms = now_ms;
				log(WARN) << "Timeout on sync job for height " << job->height << ", trying again ...";
			}
		}
	}

	// check for fetch job timeouts
	for(auto iter = fetch_jobs.begin(); iter != fetch_jobs.end();) {
		const auto job = iter->second;
		if(now_ms - std::max(job->start_time_ms, job->last_recv_ms) > fetch_timeout_ms) {
			vnx_async_return_ex_what(iter->first, "fetch timeout");
			iter = fetch_jobs.erase(iter);
		} else {
			iter++;
		}
	}

	process();
}

bool Router::process(std::shared_ptr<const Return> ret)
{
	const auto now_ms = vnx::get_wall_time_millis();

	bool did_consume = false;
	for(auto& entry : sync_jobs)
	{
		const auto& request_id = entry.first;
		auto& job = entry.second;
		if(ret) {
			// check for any returns
			auto iter = job->request_map.find(ret->id);
			if(iter != job->request_map.end()) {
				const auto client = iter->second;
				if(auto result = std::dynamic_pointer_cast<const Node_get_block_at_return>(ret->result)) {
					if(auto block = result->_ret_0) {
						if(block->is_valid()) {
							job->blocks[block->content_hash] = block;
							job->succeeded.insert(client);
						} else {
							ban_peer(client, "they sent us an invalid block");
						}
					} else {
						job->failed.insert(client);
					}
				} else if(auto result = std::dynamic_pointer_cast<const vnx::Exception>(ret->result)) {
					synced_peers.erase(client);
				}
				job->pending.erase(client);
				job->request_map.erase(iter);
				job->last_recv_ms = now_ms;
				did_consume = true;
			} else {
				continue;
			}
		}
		// check for disconnects
		for(auto iter = job->pending.begin(); iter != job->pending.end();) {
			if(synced_peers.count(*iter)) {
				iter++;
			} else {
				iter = job->pending.erase(iter);
			}
		}
		const auto num_returns = job->failed.size() + job->succeeded.size();
		if(num_returns < min_sync_peers) {
			const auto num_left = 1 + min_sync_peers - num_returns;
			if(job->pending.size() < num_left) {
				auto clients = synced_peers;
				for(auto id : job->failed) {
					clients.erase(id);
				}
				for(auto id : job->succeeded) {
					clients.erase(id);
				}
				for(auto client : get_subset(clients, num_left, rand_engine))
				{
					auto req = Node_get_block_at::create();
					req->height = job->height;
					const auto id = send_request(client, req);
					job->request_map[id] = client;
					job->pending.insert(client);
				}
			}
		} else {
			uint32_t max_block_size = 0;
			for(const auto& entry : job->blocks) {
				max_block_size = std::max(entry.second->tx_cost, max_block_size);
			}
			log(DEBUG) << "Got " << job->blocks.size() << " blocks for height " << job->height << " by fetching "
					<< job->succeeded.size() + job->failed.size() << " times, " << job->failed.size() << " failed"
					<< ", size = " << to_value(max_block_size, params) << " MMX"
					<< ", took " << (now_ms - job->start_time_ms) / 1e3 << " sec";
			// we are done with the job
			std::vector<std::shared_ptr<const Block>> blocks;
			for(const auto& entry : job->blocks) {
				blocks.push_back(entry.second);
			}
			job->is_done = true;
			get_blocks_at_async_return(request_id, blocks);
		}
	}
	for(auto iter = sync_jobs.begin(); iter != sync_jobs.end();) {
		if(iter->second->is_done) {
			iter = sync_jobs.erase(iter);
		} else {
			iter++;
		}
	}

	for(const auto& entry : fetch_jobs)
	{
		const auto& request_id = entry.first;
		const auto& job = entry.second;
		if(ret) {
			// check for any returns
			auto iter = job->request_map.find(ret->id);
			if(iter != job->request_map.end()) {
				const auto client = iter->second;
				if(auto result = std::dynamic_pointer_cast<const Node_get_block_at_return>(ret->result)) {
					if(job->height) {
						job->is_done = true;
						fetch_block_at_async_return(request_id, result->_ret_0);
					}
				} else if(auto result = std::dynamic_pointer_cast<const Node_get_block_return>(ret->result)) {
					if(job->hash) {
						if(result->_ret_0 || job->from_peer) {
							job->is_done = true;
							fetch_block_async_return(request_id, result->_ret_0);
							log(DEBUG) << "Got block " << *job->hash << " by fetching "
									<< job->pending.size() + job->failed.size() << " times, " << job->failed.size() << " failed, took"
									<< (now_ms - job->start_time_ms) / 1e3 << " sec";
						} else {
							job->failed.insert(client);
						}
					}
				} else if(job->from_peer) {
					job->is_done = true;
					vnx_async_return_ex_what(request_id, "request failed");
				} else {
					job->failed.insert(client);
				}
				job->pending.erase(client);
				job->request_map.erase(iter);
				job->last_recv_ms = now_ms;
				did_consume = true;
			} else {
				continue;
			}
		}
		if(job->is_done) {
			continue;
		}
		// check for disconnects
		for(auto iter = job->pending.begin(); iter != job->pending.end();) {
			if(synced_peers.count(*iter)) {
				iter++;
			} else {
				iter = job->pending.erase(iter);
			}
		}
		if(auto address = job->from_peer) {
			if(job->request_map.empty()) {
				std::shared_ptr<peer_t> peer;
				for(const auto& entry : peer_map) {
					if(entry.second->address == *address) {
						peer = entry.second;
						break;
					}
				}
				if(!peer) {
					job->is_done = true;
					vnx_async_return_ex_what(request_id, "no such peer");
					continue;
				}
				if(auto hash = job->hash) {
					auto req = Node_get_block::create();
					req->hash = *hash;
					const auto id = send_request(peer, req);
					job->request_map[id] = peer->client;
				}
				if(auto height = job->height) {
					auto req = Node_get_block_at::create();
					req->height = *height;
					const auto id = send_request(peer, req);
					job->request_map[id] = peer->client;
				}
			}
		} else if(auto hash = job->hash) {
			auto clients = synced_peers;
			for(auto id : job->failed) {
				clients.erase(id);
			}
			for(auto id : job->pending) {
				clients.erase(id);
			}
			if(clients.empty() && job->pending.empty()) {
				job->is_done = true;
				fetch_block_async_return(request_id, nullptr);
				continue;
			}
			if(job->pending.size() < min_sync_peers) {
				for(auto client : get_subset(clients, min_sync_peers - job->pending.size(), rand_engine)) {
					auto req = Node_get_block::create();
					req->hash = *hash;
					const auto id = send_request(client, req);
					job->request_map[id] = client;
					job->pending.insert(client);
				}
			}
		} else {
			job->is_done = true;
			vnx_async_return_ex_what(request_id, "invalid request");
		}
	}
	for(auto iter = fetch_jobs.begin(); iter != fetch_jobs.end();) {
		if(iter->second->is_done) {
			iter = fetch_jobs.erase(iter);
		} else {
			iter++;
		}
	}
	return did_consume;
}

void Router::connect()
{
	const auto now_ms = vnx::get_wall_time_millis();
	const auto now_sec = now_ms / 1000;

	// connect to fixed peers
	for(const auto& address : fixed_peers) {
		bool connected = false;
		for(const auto& entry : peer_map) {
			if(address == entry.second->address) {
				connected = true;
			}
		}
		if(!connected && !connecting_peers.count(address)) {
			connecting_peers.insert(address);
			connect_threads->add_task(std::bind(&Router::connect_task, this, address));
		}
	}

	// connect to new peers
	{
		std::set<std::string> try_peers;
		std::set<std::string> all_peers = peer_set;

		for(const auto& entry : peer_retry_map) {
			all_peers.insert(entry.first);
		}
		all_peers.insert(seed_peers.begin(), seed_peers.end());

		for(const auto& address : all_peers)
		{
			if(synced_peers.size() >= num_peers_out) {
				auto iter = peer_retry_map.find(address);
				if(iter != peer_retry_map.end()) {
					if(now_sec < iter->second) {
						continue;	// wait before trying again
					}
				}
			}
			bool connected = false;
			for(const auto& entry : peer_map) {
				if(address == entry.second->address) {
					connected = true; break;
				}
			}
			if(!connected && !block_peers.count(address) && !connecting_peers.count(address)) {
				try_peers.insert(address);
			}
		}

		for(const auto& address : get_subset(try_peers, num_peers_out, rand_engine))
		{
			if(connecting_peers.size() >= 4 * num_peers_out) {
				break;
			}
			log(DEBUG) << "Trying to connect to " << address;

			peer_retry_map.erase(address);
			connecting_peers.insert(address);
			connect_threads->add_task(std::bind(&Router::connect_task, this, address));
		}
	}

	std::set<std::shared_ptr<peer_t>> outbound_synced;
	std::set<std::shared_ptr<peer_t>> outbound_not_synced;
	for(const auto& entry : peer_map) {
		const auto& peer = entry.second;
		if(peer->is_outbound && !fixed_peers.count(peer->address)) {
			if(peer->is_synced) {
				outbound_synced.insert(peer);
			}
			else if(now_ms - peer->connected_since_ms > connection_timeout_ms) {
				outbound_not_synced.insert(peer);
			}
		}
	}

	// disconnect if we have too many outbound synced peers
	{
		size_t num_disconnect = 0;
		if(outbound_synced.size() > num_peers_out + 1) {
			num_disconnect = outbound_synced.size() - num_peers_out;
		}
		for(const auto& peer : get_subset(outbound_synced, num_disconnect, rand_engine)) {
			log(DEBUG) << "Disconnecting from " << peer->address << " to reduce connections to synced peers";
			disconnect(peer->client);
		}
	}

	// disconnect if we have too many outbound non-synced peers
	{
		size_t num_disconnect = 0;
		if(outbound_not_synced.size() > num_peers_out) {
			num_disconnect = outbound_not_synced.size() - num_peers_out;
		}
		for(const auto& peer : get_subset(outbound_not_synced, num_disconnect, rand_engine)) {
			log(DEBUG) << "Disconnecting from " << peer->address << " to reduce connections to non-synced peers";
			disconnect(peer->client);
		}
	}
}

void Router::query()
{
	const auto now_ms = vnx::get_wall_time_millis();
	{
		auto req = Request::create();
		req->id = next_request_id++;
		req->method = Node_get_synced_height::create();
		send_all(req, {node_type_e::FULL_NODE, node_type_e::LIGHT_NODE}, false);
	}
	last_query_ms = now_ms;

	// check if we forked off
	node->get_synced_height(
		[this](const vnx::optional<uint32_t>& sync_height) {
			if(sync_height) {
				hash_t major_hash;
				size_t major_count = 0;
				for(const auto& entry : fork_check.hash_count) {
					if(entry.second > major_count) {
						major_hash = entry.first;
						major_count = entry.second;
					}
				}
				if(major_count > 0 && major_hash == fork_check.our_hash) {
					log(INFO) << "A majority of " << major_count << " / " << fork_check.request_map.size() << " peers agree at height " << fork_check.height;
				}
				if(major_count >= min_sync_peers && major_hash != fork_check.our_hash)
				{
					log(WARN) << "We forked from the network, a majority of "
							<< major_count << " / " << fork_check.request_map.size() << " peers disagree at height " << fork_check.height << "!";
					node->start_sync();
					fork_check.hash_count.clear();
				}
				else {
					const auto height = *sync_height - params->infuse_delay;
					node->get_block_hash(height,
						[this, height](const vnx::optional<hash_t>& hash) {
							if(hash) {
								fork_check.height = height;
								fork_check.our_hash = *hash;
								fork_check.hash_count.clear();
								fork_check.request_map.clear();
								auto req = Node_get_block_hash::create();
								req->height = height;
								for(const auto& entry : peer_map) {
									const auto& peer = entry.second;
									// TODO: only consider outbound peers for mainnet
									if(peer->is_synced && peer->info.type == node_type_e::FULL_NODE) {
										const auto id = send_request(peer, req);
										fork_check.request_map[id] = peer->client;
									}
								}
							}
						});
				}
				is_synced = true;
			} else {
				is_synced = false;
			}
		});
}

void Router::discover()
{
	if(peer_set.size() >= max_peer_set) {
		return;
	}
	auto method = Router_get_peers::create();
	method->max_count = 4 * num_peers_out;
	auto req = Request::create();
	req->id = next_request_id++;
	req->method = method;
	send_all(req, {node_type_e::FULL_NODE}, false);
}

void Router::save_data()
{
	{
		vnx::File file(storage_path + "known_peers.dat");
		try {
			file.open("wb");
			const auto peers = get_known_peers();
			vnx::write_generic(file.out, std::set<std::string>(peers.begin(), peers.end()));
			file.close();
		}
		catch(const std::exception& ex) {
			log(WARN) << "Failed to write peers to file: " << ex.what();
		}
	}
	{
		vnx::File file(storage_path + "farmer_credits.dat");
		try {
			file.open("wb");
			vnx::write_generic(file.out, farmer_credits);
			file.close();
		}
		catch(const std::exception& ex) {
			log(WARN) << "Failed to write farmer credits to file: " << ex.what();
		}
	}
}

void Router::add_peer(const std::string& address, const int sock)
{
	connecting_peers.erase(address);

	if(sock >= 0) {
		const auto client = add_client(sock, address);
		if(auto peer = find_peer(client)) {
			peer->is_outbound = true;
		}
		peer_set.insert(address);
	} else {
		peer_set.erase(address);
		peer_retry_map[address] = vnx::get_wall_time_seconds() + peer_retry_interval * 60;
	}
}

void Router::connect_task(const std::string& address) noexcept
{
	vnx::TcpEndpoint peer;
	peer.host_name = address;
	peer.port = params->port;
	int sock = -1;
	try {
		sock = peer.open();
		peer.connect(sock);
	}
	catch(const std::exception& ex) {
		if(sock >= 0) {
			peer.close(sock);
			sock = -1;
		}
		if(show_warnings) {
			log(WARN) << "Connecting to peer " << address << " failed with: " << ex.what();
		}
	}
	if(vnx::do_run()) {
		add_task(std::bind(&Router::add_peer, this, address, sock));
	}
}

void Router::print_stats()
{
	log(INFO) << float(tx_counter * 1000) / stats_interval_ms
			  << " tx/s, " << float(vdf_counter * 1000) / stats_interval_ms
			  << " vdf/s, " << float(proof_counter * 1000) / stats_interval_ms
			  << " proof/s, " << float(block_counter * 1000) / stats_interval_ms
			  << " block/s, " << synced_peers.size() << " / " <<  peer_map.size() << " / " << peer_set.size()
			  << " peers, " << float(tx_upload_sum * 1000) / stats_interval_ms << " MMX/s tx upload, "
			  << tx_drop_counter << " / " << vdf_drop_counter << " / " << proof_drop_counter << " / " << block_drop_counter << " dropped";
	tx_counter = 0;
	tx_upload_sum = 0;
	vdf_counter = 0;
	proof_counter = 0;
	block_counter = 0;
	upload_counter = 0;
	tx_drop_counter = 0;
	vdf_drop_counter = 0;
	proof_drop_counter = 0;
	block_drop_counter = 0;
}

void Router::ban_peer(uint64_t client, const std::string& reason)
{
	if(auto peer = find_peer(client)) {
		block_peers.insert(peer->address);
		disconnect(client);
		log(WARN) << "Banned peer " << peer->address << " because: " << reason;
	}
}

void Router::on_vdf(uint64_t client, std::shared_ptr<const ProofOfTime> value)
{
	if(!value->is_valid(params)) {
		return;
	}
	const auto hash = value->content_hash;
	const auto is_new = receive_msg_hash(hash, client);
	if(is_new) {
		try {
			value->validate();
		} catch(const std::exception& ex) {
			ban_peer(client, std::string("they sent us an invalid VDF: ") + ex.what());
			return;
		}
	}
	if(auto peer = find_peer(client)) {
		if(peer->credits >= vdf_relay_cost) {
			if(relay_msg_hash(hash)) {
				peer->credits -= vdf_relay_cost;
				relay(value, hash, {node_type_e::FULL_NODE, node_type_e::LIGHT_NODE});
				vdf_counter++;
			}
		} else {
			log(DEBUG) << "Peer " << peer->address << " has insufficient credits to relay VDF for height " << value->height << ", verifying first.";
		}
	}
	if(is_new) {
		publish(value, output_vdfs);
	}
}

void Router::on_block(uint64_t client, std::shared_ptr<const Block> block)
{
	if(!block->is_valid() || !block->farmer_sig || block->tx_cost > params->max_block_cost) {
		return;
	}
	const auto hash = block->content_hash;
	const auto is_new = receive_msg_hash(hash, client);
	if(is_new) {
		try {
			block->validate();
		} catch(const std::exception& ex) {
			ban_peer(client, std::string("they sent us an invalid block: ") + ex.what());
			return;
		}
	}
	const auto farmer_id = hash_t(block->proof->farmer_key);
	const auto iter = farmer_credits.find(farmer_id);
	if(iter != farmer_credits.end()) {
		if(iter->second >= block_relay_cost) {
			if(relay_msg_hash(hash)) {
				iter->second -= block_relay_cost;
				relay(block, hash, {node_type_e::FULL_NODE, node_type_e::LIGHT_NODE});
				block_counter++;
			}
		} else {
			log(DEBUG) << "A farmer has insufficient credits to relay block at height " << block->height << ", verifying first.";
		}
	} else {
		log(DEBUG) << "Got block from an unknown farmer at height " << block->height << ", verifying first.";
	}
	if(is_new) {
		publish(block, output_blocks);
	}
}

void Router::on_proof(uint64_t client, std::shared_ptr<const ProofResponse> value)
{
	if(!value->is_valid()) {
		return;
	}
	const auto hash = value->content_hash;
	const auto is_new = receive_msg_hash(hash, client);
	if(is_new) {
		try {
			value->validate();
		} catch(const std::exception& ex) {
			ban_peer(client, std::string("they sent us invalid proof: ") + ex.what());
			return;
		}
	}
	const auto farmer_id = hash_t(value->proof->farmer_key);
	const auto iter = farmer_credits.find(farmer_id);
	if(iter != farmer_credits.end()) {
		if(iter->second >= proof_relay_cost) {
			if(relay_msg_hash(hash)) {
				iter->second -= proof_relay_cost;
				relay(value, hash, {node_type_e::FULL_NODE});
				proof_counter++;
			}
		} else {
			log(DEBUG) << "A farmer has insufficient credits to relay proof for height "
					<< value->request->height << " with score " << value->proof->score << ", verifying first.";
		}
	} else {
		log(DEBUG) << "Got proof from an unknown farmer at height " << value->request->height
				<< " with score " << value->proof->score << ", verifying first.";
	}
	if(is_new) {
		publish(value, output_proof);
	}
}

void Router::on_transaction(uint64_t client, std::shared_ptr<const Transaction> tx)
{
	if(!tx->is_valid(params) || tx->exec_result) {
		return;
	}
	if(receive_msg_hash(tx->content_hash, client)) {
		publish(tx, output_transactions);
	}
}

void Router::on_recv_note(uint64_t client, std::shared_ptr<const ReceiveNote> note)
{
	if(auto peer = find_peer(client)) {
		if(peer->sent_hashes.insert(note->hash).second) {
			peer->hash_queue.push(note->hash);
		}
		auto iter = peer->pending_map.find(note->hash);
		if(iter != peer->pending_map.end()) {
			peer->pending_cost -= iter->second;
			peer->pending_map.erase(iter);
		}
	}
}

void Router::recv_notify(const hash_t& msg_hash)
{
	auto note = ReceiveNote::create();
	note->time = vnx::get_wall_time_micros();
	note->hash = msg_hash;
	for(const auto& entry : peer_map) {
		send_to(entry.second, note, true);
	}
}

void Router::send()
{
	const auto now = vnx::get_wall_time_micros();
	tx_upload_credits += tx_upload_bandwidth * send_interval_ms / 1000;
	tx_upload_credits = std::min(tx_upload_credits, tx_upload_bandwidth);

	for(const auto& entry : peer_map) {
		const auto& peer = entry.second;
		for(auto iter = peer->send_queue.begin(); iter != peer->send_queue.end() && iter->first < now;) {
			const auto& item = iter->second;
			if(!peer->sent_hashes.count(item.hash)) {
				if(send_to(peer, item.value, item.reliable)) {
					if(peer->sent_hashes.insert(item.hash).second) {
						peer->hash_queue.push(item.hash);
					}
				}
			}
			iter = peer->send_queue.erase(iter);
		}
	}
}

void Router::send_to(std::vector<std::shared_ptr<peer_t>> peers, std::shared_ptr<const vnx::Value> msg, const hash_t& msg_hash, bool reliable)
{
	std::shuffle(peers.begin(), peers.end(), rand_engine);

	const auto now = vnx::get_wall_time_micros();
	const auto interval = (relay_target_ms * 1000) / (1 + peers.size());
	for(size_t i = 0; i < peers.size(); ++i) {
		send_item_t item;
		item.hash = msg_hash;
		item.value = msg;
		item.reliable = reliable;
		peers[i]->send_queue.emplace(now + interval * i, item);
	}
}

void Router::relay(std::shared_ptr<const vnx::Value> msg, const hash_t& msg_hash, const std::set<node_type_e>& filter)
{
	if(do_relay) {
		broadcast(msg, msg_hash, filter, false);
	}
}

void Router::broadcast(std::shared_ptr<const vnx::Value> msg, const hash_t& msg_hash, const std::set<node_type_e>& filter, bool reliable)
{
	std::vector<std::shared_ptr<peer_t>> peers;
	for(const auto& entry : peer_map) {
		const auto& peer = entry.second;
		if(filter.empty() || filter.count(peer->info.type)) {
			peers.push_back(peer);
		}
	}
	send_to(peers, msg, msg_hash, reliable);
}

bool Router::send_to(uint64_t client, std::shared_ptr<const vnx::Value> msg, bool reliable)
{
	if(auto peer = find_peer(client)) {
		return send_to(peer, msg, reliable);
	}
	return false;
}

bool Router::send_to(std::shared_ptr<peer_t> peer, std::shared_ptr<const vnx::Value> msg, bool reliable)
{
	if(!reliable && peer->is_blocked) {
		switch(msg->get_type_hash()) {
			case Block::VNX_TYPE_ID: block_drop_counter++; break;
			case Transaction::VNX_TYPE_ID: tx_drop_counter++; break;
			case ProofOfTime::VNX_TYPE_ID: vdf_drop_counter++; break;
			case ProofResponse::VNX_TYPE_ID: proof_drop_counter++; break;
			default:
				if(auto req = std::dynamic_pointer_cast<const Request>(msg)) {
					auto ret = Return::create();
					ret->id = req->id;
					ret->result = vnx::OverflowException::create();
					add_task(std::bind(&Router::on_return, this, peer->client, ret));
				}
		}
		return false;
	}
	if(!reliable) {
		if(auto tx = std::dynamic_pointer_cast<const Transaction>(msg)) {
			if(peer->pending_cost >= max_pending_cost_value) {
				tx_drop_counter++;
				return false;
			}
			std::uniform_real_distribution<double> dist(0, 1);
			if(dist(rand_engine) > tx_upload_credits / tx_upload_bandwidth) {
				tx_drop_counter++;
				return false;
			}
			const auto cost = to_value(tx->static_cost, params);
			peer->pending_cost += cost;
			peer->pending_map[tx->content_hash] = cost;

			tx_upload_credits -= cost;
			tx_upload_credits = std::max(tx_upload_credits, 0.);
			tx_upload_sum += cost;
		}
	}
	Super::send_to(peer, msg);
	return true;
}

void Router::send_all(std::shared_ptr<const vnx::Value> msg, const std::set<node_type_e>& filter, bool reliable)
{
	for(const auto& entry : peer_map) {
		const auto& peer = entry.second;
		if(filter.empty() || filter.count(peer->info.type)) {
			send_to(peer, msg, reliable);
		}
	}
}

template<typename R, typename T>
void Router::send_result(uint64_t client, uint32_t id, const T& value)
{
	auto ret = Return::create();
	ret->id = id;
	auto result = R::create();
	result->_ret_0 = value;
	ret->result = result;
	send_to(client, ret);
}

void Router::on_error(uint64_t client, uint32_t id, const vnx::exception& ex)
{
	auto ret = Return::create();
	ret->id = id;
	ret->result = ex.value();
	send_to(client, ret);
}

void Router::on_request(uint64_t client, std::shared_ptr<const Request> msg)
{
	const auto method = msg->method;
	if(!method) {
		return;
	}
	switch(method->get_type_hash())
	{
		case Router_get_id::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Router_get_id>(method)) {
				send_result<Router_get_id_return>(client, msg->id, get_id());
			}
			break;
		case Router_get_info::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Router_get_info>(method)) {
				send_result<Router_get_info_return>(client, msg->id, get_info());
			}
			break;
		case Router_get_peers::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Router_get_peers>(method)) {
				send_result<Router_get_peers_return>(client, msg->id, get_peers(value->max_count));
			}
			break;
		case Node_get_height::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Node_get_height>(method)) {
				node->get_height(
						[=](const uint32_t& height) {
							send_result<Node_get_height_return>(client, msg->id, height);
						},
						std::bind(&Router::on_error, this, client, msg->id, std::placeholders::_1));
			}
			break;
		case Node_get_synced_height::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Node_get_synced_height>(method)) {
				node->get_synced_height(
						[=](const vnx::optional<uint32_t>& height) {
							send_result<Node_get_synced_height_return>(client, msg->id, height);
						},
						std::bind(&Router::on_error, this, client, msg->id, std::placeholders::_1));
			}
			break;
		case Node_get_block::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Node_get_block>(method)) {
				node->get_block(value->hash,
						[=](std::shared_ptr<const Block> block) {
							upload_counter++;
							send_result<Node_get_block_return>(client, msg->id, block);
						},
						std::bind(&Router::on_error, this, client, msg->id, std::placeholders::_1));
			}
			break;
		case Node_get_block_at::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Node_get_block_at>(method)) {
				node->get_block_at(value->height,
						[=](std::shared_ptr<const Block> block) {
							upload_counter++;
							send_result<Node_get_block_at_return>(client, msg->id, block);
						},
						std::bind(&Router::on_error, this, client, msg->id, std::placeholders::_1));
			}
			break;
		case Node_get_block_hash::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Node_get_block_hash>(method)) {
				node->get_block_hash(value->height,
						[=](const vnx::optional<hash_t>& hash) {
							send_result<Node_get_block_hash_return>(client, msg->id, hash);
						},
						std::bind(&Router::on_error, this, client, msg->id, std::placeholders::_1));
			}
			break;
		case Node_get_header::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Node_get_header>(method)) {
				node->get_header(value->hash,
						[=](std::shared_ptr<const BlockHeader> block) {
							send_result<Node_get_header_return>(client, msg->id, block);
						},
						std::bind(&Router::on_error, this, client, msg->id, std::placeholders::_1));
			}
			break;
		case Node_get_header_at::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Node_get_header_at>(method)) {
				node->get_header_at(value->height,
						[=](std::shared_ptr<const BlockHeader> block) {
							send_result<Node_get_header_at_return>(client, msg->id, block);
						},
						std::bind(&Router::on_error, this, client, msg->id, std::placeholders::_1));
			}
			break;
		case Node_get_tx_ids_at::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Node_get_tx_ids_at>(method)) {
				node->get_tx_ids_at(value->height,
						[=](const std::vector<hash_t>& ids) {
							send_result<Node_get_tx_ids_at_return>(client, msg->id, ids);
						},
						std::bind(&Router::on_error, this, client, msg->id, std::placeholders::_1));
			}
			break;
		default: {
			auto ret = Return::create();
			ret->id = msg->id;
			ret->result = vnx::NoSuchMethod::create();
			send_to(client, ret);
		}
	}
}

void Router::on_return(uint64_t client, std::shared_ptr<const Return> msg)
{
	if(process(msg)) {
		return;
	}
	const auto result = msg->result;
	if(!result) {
		return;
	}
	switch(result->get_type_hash())
	{
		case Router_get_id_return::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Router_get_id_return>(result)) {
				if(value->_ret_0 == get_id()) {
					if(auto peer = find_peer(client)) {
						log(INFO) << "Discovered our own address (or duplicate node ID?): " << peer->address;
						self_addrs.insert(peer->address);
						block_peers.insert(peer->address);
					}
					disconnect(client);
				}
			}
			break;
		case Router_get_info_return::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Router_get_info_return>(result)) {
				if(auto peer = find_peer(client)) {
					peer->info = value->_ret_0;
				}
			}
			break;
		case Router_sign_msg_return::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Router_sign_msg_return>(result)) {
				if(auto peer = find_peer(client)) {
					const auto& pair = value->_ret_0;
					if(pair.second.verify(pair.first, hash_t(peer->challenge.bytes))) {
						peer->node_id = pair.first.get_addr();
					} else {
						log(WARN) << "Peer " << peer->address << " failed to verify identity!";
					}
				}
			}
			break;
		case Router_get_peers_return::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Router_get_peers_return>(result)) {
				for(const auto& address : value->_ret_0) {
					peer_retry_map.emplace(address, 0);
				}
			}
			break;
		case Node_get_height_return::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Node_get_height_return>(result)) {
				if(auto peer = find_peer(client)) {
					peer->height = value->_ret_0;
					peer->last_receive_ms = vnx::get_wall_time_millis();
				}
			}
			break;
		case Node_get_synced_height_return::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Node_get_synced_height_return>(result)) {
				if(auto peer = find_peer(client)) {
					if(auto height = value->_ret_0) {
						if(!peer->is_synced) {
							log(INFO) << "Peer " << peer->address << " is synced at height " << *height;
						}
						peer->height = *height;
						peer->is_synced = true;
						if(peer->info.type == node_type_e::FULL_NODE) {
							synced_peers.insert(client);
						}
					}
					else {
						if(peer->is_synced) {
							log(INFO) << "Peer " << peer->address << " is not synced";
						}
						peer->is_synced = false;
						synced_peers.erase(client);

						// check their height
						send_request(client, Node_get_height::create());
					}
					const auto now_ms = vnx::get_wall_time_millis();
					if(last_query_ms) {
						peer->ping_ms = now_ms - last_query_ms;
					}
					peer->last_receive_ms = now_ms;
				}
			}
			break;
		case Node_get_block_hash_return::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Node_get_block_hash_return>(result)) {
				auto iter = fork_check.request_map.find(msg->id);
				if(iter != fork_check.request_map.end()) {
					if(iter->second == client) {
						if(auto hash = value->_ret_0) {
							fork_check.hash_count[*hash]++;
						}
					}
				}
			}
			break;
	}
}

void Router::on_msg(uint64_t client, std::shared_ptr<const vnx::Value> msg)
{
	switch(msg->get_type_hash())
	{
	case ProofOfTime::VNX_TYPE_ID:
		if(auto value = std::dynamic_pointer_cast<const ProofOfTime>(msg)) {
			on_vdf(client, value);
		}
		break;
	case ProofResponse::VNX_TYPE_ID:
		if(auto value = std::dynamic_pointer_cast<const ProofResponse>(msg)) {
			on_proof(client, value);
		}
		break;
	case Block::VNX_TYPE_ID:
		if(auto value = std::dynamic_pointer_cast<const Block>(msg)) {
			on_block(client, value);
		}
		break;
	case Transaction::VNX_TYPE_ID:
		if(auto value = std::dynamic_pointer_cast<const Transaction>(msg)) {
			on_transaction(client, value);
		}
		break;
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
	case ReceiveNote::VNX_TYPE_ID:
		if(auto value = std::dynamic_pointer_cast<const ReceiveNote>(msg)) {
			on_recv_note(client, value);
		}
		break;
	}
}

void Router::on_pause(uint64_t client)
{
	if(auto peer = find_peer(client)) {
		peer->is_blocked = true;
		if(!peer->is_outbound) {
			pause(client);		// pause incoming traffic
		}
	}
}

void Router::on_resume(uint64_t client)
{
	if(auto peer = find_peer(client)) {
		peer->is_blocked = false;
		if(!peer->is_outbound) {
			resume(client);		// resume incoming traffic
		}
	}
}

void Router::on_connect(uint64_t client, const std::string& address)
{
	if(block_peers.count(address)) {
		disconnect(client);
		return;
	}
	const auto seed = vnx::rand64();

	auto peer = std::make_shared<peer_t>();
	peer->client = client;
	peer->address = address;
	peer->info.type = node_type_e::FULL_NODE;	// assume full node
	peer->challenge = hash_t(&seed, sizeof(seed));
	peer->connected_since_ms = vnx::get_wall_time_millis();
	peer_map[client] = peer;

	send_request(peer, Router_get_id::create());
	send_request(peer, Router_get_info::create());
	send_request(peer, Node_get_synced_height::create());

	auto req = Router_sign_msg::create();
	req->msg = peer->challenge;
	send_request(peer, req);

	log(INFO) << "Connected to peer " << peer->address;
}

void Router::on_disconnect(uint64_t client)
{
	if(auto peer = find_peer(client)) {
		log(INFO) << "Peer " << peer->address << " disconnected";
		peer_retry_map[peer->address] = vnx::get_wall_time_seconds() + peer_retry_interval * 60;
	}
	synced_peers.erase(client);

	add_task([this, client]() {
		peer_map.erase(client);
	});
}

std::shared_ptr<Router::Super::peer_t> Router::get_peer_base(uint64_t client) const
{
	return get_peer(client);
}

std::shared_ptr<Router::peer_t> Router::get_peer(uint64_t client) const
{
	if(auto peer = find_peer(client)) {
		return peer;
	}
	throw std::logic_error("no such peer");
}

std::shared_ptr<Router::peer_t> Router::find_peer(uint64_t client) const
{
	auto iter = peer_map.find(client);
	if(iter != peer_map.end()) {
		return iter->second;
	}
	return nullptr;
}

bool Router::relay_msg_hash(const hash_t& hash, uint32_t credits)
{
	const auto ret = hash_info.emplace(hash, hash_info_t());
	if(ret.second) {
		hash_queue.push(hash);
	}
	auto& info = ret.first->second;
	if(!info.did_notify) {
		recv_notify(hash);
		info.did_notify = true;
	}
	info.is_valid = true;

	if(credits && !info.is_rewarded && info.received_from != uint64_t(-1))
	{
		if(auto peer = find_peer(info.received_from)) {
			peer->credits += credits;
		}
		info.is_rewarded = true;
	}
	if(!info.did_relay) {
		info.did_relay = true;
		return true;
	}
	return false;
}

bool Router::receive_msg_hash(const hash_t& hash, uint64_t client)
{
	const auto ret = hash_info.emplace(hash, hash_info_t());
	if(ret.second) {
		hash_queue.push(hash);
	}
	auto& info = ret.first->second;
	if(!info.did_notify) {
		recv_notify(hash);
		info.did_notify = true;
	}
	const bool is_new = info.received_from == uint64_t(-1);
	if(is_new) {
		info.received_from = client;
	}
	if(auto peer = find_peer(client)) {
		if(peer->sent_hashes.insert(hash).second) {
			peer->hash_queue.push(hash);
		}
	}
	return is_new;
}

void Router::http_request_async(std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
								const vnx::request_id_t& request_id) const
{
	http->http_request(request, sub_path, request_id, vnx_request->session);
}

void Router::http_request_chunk_async(	std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
										const int64_t& offset, const int64_t& max_bytes, const vnx::request_id_t& request_id) const
{
	throw std::logic_error("not implemented");
}


} // mmx
