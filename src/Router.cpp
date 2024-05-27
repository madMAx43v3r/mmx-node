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
#include <mmx/Node_get_block_hash_ex.hxx>
#include <mmx/Node_get_block_hash_ex_return.hxx>
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
	{
		const auto max_block_size = to_value(params->max_block_size, params);
		tx_upload_bandwidth = max_tx_upload * max_block_size / params->block_time;
		max_pending_cost_value = max_pending_cost * max_block_size;
	}
	log(INFO) << "Global TX upload limit: " << tx_upload_bandwidth << " MMX/s";
	log(INFO) << "Peer TX pending limit: " << max_pending_cost_value << " MMX";

	subscribe(input_vdfs, max_queue_ms);
	subscribe(input_verified_vdfs, max_queue_ms);
	subscribe(input_verified_proof, max_queue_ms);
	subscribe(input_verified_blocks, max_queue_ms);
	subscribe(input_verified_transactions, max_queue_ms);
	subscribe(input_transactions, max_queue_ms);

	node_id = hash_t::random();
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
	}
	log(INFO) << "Node ID: " << node_id;
	log(INFO) << "Loaded " << peer_set.size() << " known peers";

	node = std::make_shared<NodeAsyncClient>(node_server);
	node->vnx_set_non_blocking(true);
	add_async_client(node);

	http = std::make_shared<vnx::addons::HttpInterface<Router>>(this, vnx_name);
	add_async_client(http);

	if(open_port) {
		upnp_mapper = upnp_start_mapping(port, "MMX Node");
		log(upnp_mapper ? INFO : WARN) << "UPnP supported: " << (upnp_mapper ? "yes" : "no");
	}

	set_timer_millis(send_interval_ms, std::bind(&Router::send, this));
	set_timer_millis(query_interval_ms, std::bind(&Router::query, this));
	set_timer_millis(update_interval_ms, std::bind(&Router::update, this));
	set_timer_millis(connect_interval_ms, std::bind(&Router::connect, this));
	set_timer_millis(discover_interval * 1000, std::bind(&Router::discover, this));
	set_timer_millis(fork_check_interval * 1000, std::bind(&Router::exec_fork_check, this));
	set_timer_millis(5 * 60 * 1000, std::bind(&Router::save_data, this));

	connect();

	Super::main();

	save_data();

	if(upnp_mapper) {
		upnp_mapper->stop();
	}
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

static
bool is_valid_address(const std::string& addr)
{
	if(addr.substr(0, 4) == "127." || addr == "0.0.0.0") {
		return false;
	}
	return true;
}

static
bool is_public_address(const std::string& addr)
{
	if(!is_valid_address(addr) || addr.substr(0, 3) == "10." || addr.substr(0, 8) == "192.168.") {
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
		peer.ping_ms = state->ping_ms;
		peer.bytes_send = state->bytes_send;
		peer.bytes_recv = state->bytes_recv;
		peer.pending_cost = state->pending_cost;
		peer.compression_ratio = state->bytes_send_raw / double(state->bytes_send);
		peer.is_synced = state->is_synced;
		peer.is_paused = state->is_paused;
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

void Router::kick_peer(const std::string& address)
{
	for(auto peer : find_peers(address)) {
		ban_peer(peer->client, "kicked manually");
	}
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
	job->callback = std::bind(&Router::fetch_block_async_return, this, request_id, std::placeholders::_1);
	job->start_time_ms = vnx::get_wall_time_millis();
	fetch_jobs[request_id] = job;
	((Router*)this)->process();
}

void Router::fetch_block_at_async(const uint32_t& height, const std::string& address, const vnx::request_id_t& request_id) const
{
	auto job = std::make_shared<fetch_job_t>();
	job->height = height;
	job->from_peer = address;
	job->callback = std::bind(&Router::fetch_block_at_async_return, this, request_id, std::placeholders::_1);
	job->start_time_ms = vnx::get_wall_time_millis();
	fetch_jobs[request_id] = job;
	((Router*)this)->process();
}

void Router::handle(std::shared_ptr<const Block> block)
{
	if(block->farmer_sig) {
		const auto& hash = block->content_hash;
		const auto is_ours = !hash_info.count(hash);
		// block does not give any credits (to avoid denial of service attack by farmer)
		if(relay_msg_hash(hash)) {
			if(is_ours) {
				log(INFO) << "Broadcasting block for height " << block->height;
			}
			broadcast(block, hash, {node_type_e::FULL_NODE, node_type_e::LIGHT_NODE}, is_ours);
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
			broadcast(tx, hash, {node_type_e::FULL_NODE}, true, true);
		} else {
			relay(tx, hash, {node_type_e::FULL_NODE});
		}
		tx_counter++;
	}
}

void Router::handle(std::shared_ptr<const ProofOfTime> value)
{
	const bool is_ours = vnx_sample && vnx_sample->topic == input_vdfs;
	if(is_ours && value->height <= verified_peak_height) {
		return;
	}
	const auto& hash = value->content_hash;
	if(relay_msg_hash(hash)) {
		if(is_ours) {
			log(INFO) << u8"\U0000231B Broadcasting VDF for height " << value->height;
		}
		broadcast(value, hash, {node_type_e::FULL_NODE, node_type_e::LIGHT_NODE}, is_ours);
		vdf_counter++;
	}
	auto& did_reward = hash_info[hash].did_reward;
	if(!did_reward) {
		did_reward = true;
		auto& credits = timelord_credits[value->timelord_key];
		credits = std::min(credits + vdf_credits, max_credits);
	}
}

void Router::handle(std::shared_ptr<const ProofResponse> value)
{
	if(!value->proof || !value->request) {
		return;
	}
	const auto& hash = value->content_hash;
	if(relay_msg_hash(hash)) {
		bool is_ours = false;
		if(vnx::get_pipe(value->farmer_addr)) {
			is_ours = true;
			log(INFO) << "Broadcasting proof for height " << value->request->height << " with score " << value->proof->score;
		}
		auto copy = vnx::clone(value);
		copy->harvester.clear();			// clear local information
		copy->farmer_addr = vnx::Hash64();	// clear local information
		broadcast(copy, hash, {node_type_e::FULL_NODE}, is_ours);
		proof_counter++;
	}
	auto& did_reward = hash_info[hash].did_reward;
	if(!did_reward) {
		did_reward = true;
		auto& credits = farmer_credits[value->proof->farmer_key];
		credits = std::min(credits + proof_credits, max_credits);
	}
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
		peer->pending_cost = std::max<double>(peer->pending_cost, 0) * 0.999;

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
					log(WARN) << "Lost sync with network due to loss of synced peers or timeout!";
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
				log(WARN) << "Timeout on sync job for height " << job->height << ": "
						<< job->got_hash.size() << " reply, " << job->num_fetch << " fetch, "
						<< job->failed.size() << " failed, " << job->pending.size() << " pending, "
						<< job->succeeded.size() << " succeeded";
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
		const auto elapsed_ms = now_ms - job->start_time_ms;
		if(ret) {
			// check for any returns
			auto iter = job->request_map.find(ret->id);
			if(iter != job->request_map.end()) {
				const auto client = iter->second;
				if(auto result = std::dynamic_pointer_cast<const Node_get_block_hash_ex_return>(ret->result)) {
					if(auto hash = result->_ret_0) {
						if(job->blocks.count(hash->second)) {
							job->succeeded.insert(client);
						}
						job->got_hash[client] = *hash;
					} else {
						job->failed.insert(client);
					}
				}
				else if(auto result = std::dynamic_pointer_cast<const Node_get_block_return>(ret->result)) {
					if(auto block = result->_ret_0) {
						const auto& hash = block->content_hash;
						if(block->is_valid()) {
							for(const auto& entry : job->got_hash) {
								if(entry.second.second == hash) {
									job->succeeded.insert(entry.first);
								}
							}
							job->succeeded.insert(client);
							job->blocks[hash] = block;
						} else {
							ban_peer(client, "they sent us an invalid block");
						}
						job->pending_blocks.erase(hash);
					}
					if(!job->succeeded.count(client)) {
						job->failed.insert(client);
					}
				}
				else if(auto result = std::dynamic_pointer_cast<const vnx::Exception>(ret->result)) {
					auto got_hash = job->got_hash.find(client);
					if(got_hash != job->got_hash.end()) {
						job->pending_blocks.erase(got_hash->second.second);
					}
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
			const auto client = *iter;
			if(synced_peers.count(client)) {
				iter++;
			} else {
				auto iter2 = job->got_hash.find(client);
				if(iter2 != job->got_hash.end()) {
					job->pending_blocks.erase(iter2->second.second);
					job->got_hash.erase(iter2);
				}
				iter = job->pending.erase(iter);
			}
		}
		const auto num_returns = job->failed.size() + job->succeeded.size();
		if(num_returns < min_sync_peers) {
			// fetch block hashes
			const auto max_pending = min_sync_peers + 3;
			const auto num_pending = job->pending.size() + job->got_hash.size();
			if(num_pending < max_pending) {
				std::set<uint64_t> clients;
				for(auto client : synced_peers) {
					if(!job->failed.count(client) && !job->pending.count(client)
						&& !job->succeeded.count(client) && !job->got_hash.count(client))
					{
						clients.insert(client);
					}
				}
				for(const auto client : get_subset(clients, max_pending - num_pending, rand_engine)) {
					auto req = Node_get_block_hash_ex::create();
					req->height = job->height;
					const auto id = send_request(client, req);
					job->request_map[id] = client;
					job->pending.insert(client);
				}
			}
			// fetch blocks
			std::set<std::pair<uint64_t, std::pair<hash_t, hash_t>>> clients;
			for(const auto& entry : job->got_hash) {
				const auto& hash = entry.second;
				if(!job->blocks.count(hash.second)) {
					const auto client = entry.first;
					if(!job->failed.count(client) && !job->pending.count(client) && !job->succeeded.count(client)) {
						clients.emplace(client, hash);
					}
				}
			}
			for(const auto& entry : get_subset(clients, clients.size(), rand_engine)) {
				const auto client = entry.first;
				const auto& hash = entry.second;
				auto pending = job->pending_blocks.find(hash.second);
				if(pending == job->pending_blocks.end() || now_ms > pending->second) {
					auto req = Node_get_block::create();
					req->hash = hash.first;
					const auto id = send_request(client, req);
					job->request_map[id] = client;
					job->pending.insert(client);
					job->pending_blocks[hash.second] = now_ms + fetch_timeout_ms / 8;
					job->num_fetch++;
				}
			}
		} else {
			uint32_t max_block_size = 0;
			for(const auto& entry : job->blocks) {
				max_block_size = std::max(entry.second->static_cost, max_block_size);
			}
			log(DEBUG) << "Got " << job->blocks.size() << " blocks for height " << job->height << " by fetching "
					<< job->num_fetch << " times, " << job->got_hash.size() << " reply, " << job->failed.size() << " failed"
					<< ", size = " << to_value(max_block_size, params) << " MMX" << ", took " << elapsed_ms / 1e3 << " sec";
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
						job->callback(result->_ret_0);
					}
				} else if(auto result = std::dynamic_pointer_cast<const Node_get_block_return>(ret->result)) {
					if(job->hash) {
						if(result->_ret_0 || job->from_peer) {
							job->is_done = true;
							job->callback(result->_ret_0);
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
				{
					const auto peers = find_peers(*address);
					if(!peers.empty()) {
						peer = peers[0];
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
				job->callback(nullptr);
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

void Router::connect_to(const std::string& address)
{
	if(connect_tasks.count(address)) {
		return;
	}
	log(DEBUG) << "Trying to connect to " << address;

	vnx::TcpEndpoint peer;
	peer.host_name = address;
	peer.port = params->port;

	try {
		const auto client = connect_client(peer);
		connect_tasks[address] = client;
	}
	catch(const std::exception& ex) {
		if(show_warnings) {
			log(WARN) << "Connecting to peer " << address << " failed with: " << ex.what();
		}
	}
}

void Router::connect()
{
	const auto now_ms = vnx::get_wall_time_millis();
	const auto now_sec = now_ms / 1000;

	// connect to fixed peers
	for(const auto& address : fixed_peers) {
		if(!peer_addr_map.count(address)) {
			connect_to(address);
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
			if(outbound_synced.size() >= num_peers_out) {
				auto iter = peer_retry_map.find(address);
				if(iter != peer_retry_map.end()) {
					if(now_sec < iter->second) {
						continue;	// wait before trying again
					}
				} else {
					// randomize re-checking
					peer_retry_map[address] = now_sec + vnx::rand64() % (peer_retry_interval * 60);
					continue;
				}
			}
			if(!peer_addr_map.count(address) && !block_peers.count(address) && !connect_tasks.count(address)) {
				try_peers.insert(address);
			}
		}

		for(const auto& address : get_subset(try_peers, num_peers_out, rand_engine))
		{
			if(connect_tasks.size() >= 2 * num_peers_out) {
				break;
			}
			if(is_valid_address(address)) {
				connect_to(address);
				peer_retry_map.erase(address);
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
}

void Router::exec_fork_check()
{
	node->get_synced_height(
		[this](const vnx::optional<uint32_t>& sync_height) {
			if(sync_height) {
				hash_t major_hash;
				size_t major_count = 0;
				size_t total_count = 0;
				for(const auto& entry : fork_check.hash_count) {
					if(entry.second > major_count) {
						major_hash = entry.first;
						major_count = entry.second;
					}
					total_count += entry.second;
				}
				if(major_count > 0 && major_hash == fork_check.our_hash) {
					log(INFO) << "A majority of " << major_count << " / " << total_count << " peers agree at height " << fork_check.height;
				}
				if(major_count >= min_sync_peers && major_hash != fork_check.our_hash)
				{
					log(WARN) << "We forked from the network, a majority of "
							<< major_count << " / " << total_count << " peers disagree at height " << fork_check.height << "!";
					node->start_sync();
					fork_check.hash_count.clear();
				}
				else {
					const auto height = *sync_height - params->commit_delay / 2;
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
									if(peer->is_outbound && peer->is_synced && peer->info.type == node_type_e::FULL_NODE) {
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
	if(peer_set.size() < max_peer_set) {
		auto req = Request::create();
		req->id = next_request_id++;
		req->method = Router_get_peers::create();
		send_all(req, {node_type_e::FULL_NODE}, false);
	}

	// check peers and disconnect forks
	node->get_synced_height(
		[this](const vnx::optional<uint32_t>& sync_height) {
			if(sync_height) {
				auto method = Node_get_block_hash::create();
				method->height = *sync_height - 2 * params->commit_delay;
				node->get_block_hash(method->height,
					[this, method](const vnx::optional<hash_t>& hash) {
						if(hash) {
							auto req = Request::create();
							req->id = next_request_id++;
							req->method = method;
							send_all(req, {node_type_e::FULL_NODE, node_type_e::LIGHT_NODE}, false);

							peer_check.height = method->height;
							peer_check.our_hash = *hash;
							peer_check.request_id = req->id;
						}
					});
			}
		});
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
		std::ofstream file(storage_path + "farmer_credits.txt", std::ios::trunc);
		for(const auto& entry : farmer_credits) {
			file << entry.first << "\t" << entry.second << std::endl;
		}
	}
	{
		std::ofstream file(storage_path + "timelord_credits.txt", std::ios::trunc);
		for(const auto& entry : timelord_credits) {
			file << entry.first << "\t" << entry.second << std::endl;
		}
	}
}

void Router::print_stats()
{
	log(INFO) << float(tx_counter * 1000) / stats_interval_ms
			  << " tx/s, " << float(vdf_counter * 1000) / stats_interval_ms
			  << " vdf/s, " << float(proof_counter * 1000) / stats_interval_ms
			  << " proof/s, " << float(block_counter * 1000) / stats_interval_ms
			  << " block/s, " << synced_peers.size() << " / " <<  peer_map.size() << " / " << peer_set.size()
			  << " peers, " << farmer_credits.size() << " farmers, " << timelord_credits.size() << " timelords, "
			  << float(tx_upload_sum * 1000) / stats_interval_ms << " MMX/s tx upload, "
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
	if(value->height + params->commit_delay < verified_peak_height) {
		return; // prevent replay attack of old signed data
	}
	if(!value->is_valid(params)) {
		ban_peer(client, std::string("they sent us an invalid VDF"));
		return;
	}
	const auto hash = value->content_hash;
	if(receive_msg_hash(hash, client)) {
		try {
			value->validate();
		} catch(const std::exception& ex) {
			ban_peer(client, std::string("they sent us an invalid VDF: ") + ex.what());
			return;
		}
		auto iter = timelord_credits.find(value->timelord_key);
		if(iter != timelord_credits.end()) {
			if(iter->second >= vdf_relay_cost) {
				if(relay_msg_hash(hash)) {
					iter->second -= vdf_relay_cost;
					relay(value, hash, {node_type_e::FULL_NODE, node_type_e::LIGHT_NODE});
					vdf_counter++;
				}
			} else {
				log(DEBUG) << "Timelord " << value->timelord_key << " has insufficient credits to relay VDF for height " << value->height << ", verifying first.";
			}
		}
		publish(value, output_vdfs);
	}
}

void Router::on_block(uint64_t client, std::shared_ptr<const Block> block)
{
	if(block->height + params->commit_delay < verified_peak_height) {
		return; // prevent replay attack of old signed data
	}
	if(!block->is_valid() || !block->proof || !block->farmer_sig || block->static_cost > params->max_block_size) {
		ban_peer(client, std::string("they sent us an invalid block"));
		return;
	}
	const auto hash = block->content_hash;
	if(receive_msg_hash(hash, client)) {
		try {
			block->validate();
		} catch(const std::exception& ex) {
			ban_peer(client, std::string("they sent us an invalid block: ") + ex.what());
			return;
		}
		const auto farmer_key = block->proof->farmer_key;

		auto iter = farmer_credits.find(farmer_key);
		if(iter != farmer_credits.end()) {
			if(iter->second >= block_relay_cost) {
				if(relay_msg_hash(hash)) {
					iter->second -= block_relay_cost;
					relay(block, hash, {node_type_e::FULL_NODE, node_type_e::LIGHT_NODE});
					block_counter++;
				}
			} else {
				log(DEBUG) << "Farmer " << farmer_key << " has insufficient credits to relay block for height " << block->height << ", verifying first.";
			}
		}
		publish(block, output_blocks);
	}
}

void Router::on_proof(uint64_t client, std::shared_ptr<const ProofResponse> value)
{
	if(!value->is_valid()) {
		ban_peer(client, std::string("they sent us an invalid proof"));
		return;
	}
	if(receive_msg_hash(value->content_hash, client)) {
		publish(value, output_proof);
	}
}

void Router::on_transaction(uint64_t client, std::shared_ptr<const Transaction> tx)
{
	if(!tx->is_valid(params) || tx->exec_result) {
		ban_peer(client, std::string("they sent us an invalid tx"));
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
			peer->pending_cost = std::max<double>(peer->pending_cost - iter->second, 0);
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

void Router::send_to(	std::vector<std::shared_ptr<peer_t>> peers, std::shared_ptr<const vnx::Value> msg,
						const hash_t& msg_hash, bool reliable)
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

void Router::broadcast(	std::shared_ptr<const vnx::Value> msg, const hash_t& msg_hash,
						const std::set<node_type_e>& filter, bool reliable, bool synced_only)
{
	std::vector<std::shared_ptr<peer_t>> peers;
	for(const auto& entry : peer_map) {
		const auto& peer = entry.second;
		if(!synced_only || peer->is_synced) {
			if(filter.empty() || filter.count(peer->info.type)) {
				peers.push_back(peer);
			}
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
	if(!peer->is_valid) {
		return false;
	}
	if(!reliable && peer->is_blocked) {
		bool drop = true;
		switch(msg->get_type_hash()) {
			case Block::VNX_TYPE_ID:
			case ProofOfTime::VNX_TYPE_ID:
			case ProofResponse::VNX_TYPE_ID:
				if(peer->write_queue_size < priority_queue_size) {
					drop = false;
				}
		}
		if(drop) {
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
			if(peer->pending_map.emplace(tx->content_hash, cost).second) {
				peer->pending_cost += cost;
			}
			tx_upload_credits = std::max(tx_upload_credits - cost, 0.);
			tx_upload_sum += cost;
		}
	}
	return Super::send_to(peer, msg);
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
	if(auto peer = find_peer(client)) {
		if(peer->is_blocked) {
			auto ret = Return::create();
			ret->id = msg->id;
			ret->result = vnx::OverflowException::create();
			send_to(client, ret);
			peer->is_paused = true;
			return;
		}
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
		case Node_get_block_hash_ex::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Node_get_block_hash_ex>(method)) {
				node->get_block_hash_ex(value->height,
						[=](const vnx::optional<std::pair<hash_t, hash_t>>& hash) {
							send_result<Node_get_block_hash_ex_return>(client, msg->id, hash);
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
				const auto peer = find_peer(client);
				if(peer) {
					peer->node_id = value->_ret_0;
				}
				if(value->_ret_0 == get_id()) {
					if(peer) {
						log(INFO) << "Discovered our own address: " << peer->address;
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
		case Router_get_peers_return::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Router_get_peers_return>(result)) {
				size_t i = 0;
				for(const auto& address : value->_ret_0) {
					if(is_valid_address(address)) {
						peer_retry_map.emplace(address, 0);
					}
					if(++i >= 10) {
						break;
					}
				}
			}
			break;
		case Node_get_height_return::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Node_get_height_return>(result)) {
				if(auto peer = find_peer(client)) {
					peer->height = value->_ret_0;
				}
			}
			break;
		case Node_get_synced_height_return::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Node_get_synced_height_return>(result)) {
				if(auto peer = find_peer(client)) {
					if(last_query_ms) {
						peer->ping_ms = vnx::get_wall_time_millis() - last_query_ms;
					}
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
				if(msg->id == peer_check.request_id) {
					const auto hash = value->_ret_0;
					if(!hash || *hash != peer_check.our_hash) {
						if(auto peer = find_peer(client)) {
							if(peer->is_synced) {
								log(INFO) << "Peer " << peer->address << " is on different chain at height " << peer_check.height;
								disconnect(client);
							}
						}
					}
				}
			}
			break;
	}
}

void Router::on_msg(uint64_t client, std::shared_ptr<const vnx::Value> msg)
{
	if(auto peer = find_peer(client)) {
		peer->last_receive_ms = vnx::get_wall_time_millis();
	}
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
	}
}

void Router::on_resume(uint64_t client)
{
	if(auto peer = find_peer(client)) {
		if(peer->is_paused) {
			resume(client);		// continue receiving as well again
			peer->is_paused = false;
		}
		peer->is_blocked = false;
	}
}

void Router::on_connect(uint64_t client, const std::string& address)
{
	if(block_peers.count(address)) {
		disconnect(client);
		return;
	}
	auto peer = std::make_shared<peer_t>();
	peer->client = client;
	peer->address = address;
	peer->info.type = node_type_e::FULL_NODE;	// assume full node
	peer->connected_since_ms = vnx::get_wall_time_millis();
	{
		const auto it = connect_tasks.find(address);
		if(it != connect_tasks.end() && it->second == client) {
			// we connected to them
			peer->is_outbound = true;
			connect_tasks.erase(it);
		}
	}
	peer_map[client] = peer;
	peer_addr_map.emplace(address, peer);

	send_request(peer, Router_get_id::create());
	send_request(peer, Router_get_info::create());
	send_request(peer, Node_get_synced_height::create());

	if(peer_set.size() < max_peer_set) {
		send_request(peer, Router_get_peers::create());
	}
	log(DEBUG) << "Connected to peer " << peer->address;
}

void Router::on_disconnect(uint64_t client, const std::string& address)
{
	if(auto peer = find_peer(client)) {
		peer->is_valid = false;
	}
	// async processing to allow for() loops over peer_map, etc
	add_task([this, client, address]() {
		if(auto peer = find_peer(client)) {
			const auto range = peer_addr_map.equal_range(peer->address);
			for(auto iter = range.first; iter != range.second; ++iter) {
				if(iter->second == peer) {
					peer_addr_map.erase(iter);
					break;
				}
			}
			log(DEBUG) << "Peer " << address << " disconnected";
		}
		{
			const auto it = connect_tasks.find(address);
			if(it != connect_tasks.end() && it->second == client) {
				log(DEBUG) << "Failed to connect to " << it->first;
				connect_tasks.erase(it);
			}
		}
		peer_map.erase(client);
		synced_peers.erase(client);
		peer_retry_map[address] = vnx::get_wall_time_seconds() + peer_retry_interval * 60;
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

std::vector<std::shared_ptr<Router::peer_t>> Router::find_peers(const std::string& address) const
{
	std::vector<std::shared_ptr<Router::peer_t>> out;
	const auto range = peer_addr_map.equal_range(address);
	for(auto iter = range.first; iter != range.second; ++iter) {
		out.push_back(iter->second);
	}
	return out;
}

bool Router::relay_msg_hash(const hash_t& hash)
{
	const auto ret = hash_info.emplace(hash, hash_info_t());
	if(ret.second) {
		recv_notify(hash);
		hash_queue.push(hash);
	}
	auto& did_relay = ret.first->second.did_relay;
	if(!did_relay) {
		did_relay = true;
		return true;
	}
	return false;
}

bool Router::receive_msg_hash(const hash_t& hash, uint64_t client)
{
	const auto ret = hash_info.emplace(hash, hash_info_t());
	if(ret.second) {
		recv_notify(hash);
		hash_queue.push(hash);
	}
	if(auto peer = find_peer(client)) {
		if(peer->sent_hashes.insert(hash).second) {
			peer->hash_queue.push(hash);
		}
	}
	return ret.second;
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
