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
#include <mmx/Router_sign_msg.hxx>
#include <mmx/Router_sign_msg_return.hxx>
#include <mmx/Router_get_peers.hxx>
#include <mmx/Router_get_peers_return.hxx>
#include <mmx/Node_get_height.hxx>
#include <mmx/Node_get_height_return.hxx>
#include <mmx/Node_get_synced_height.hxx>
#include <mmx/Node_get_synced_height_return.hxx>
#include <mmx/Node_get_block.hxx>
#include <mmx/Node_get_block_return.hxx>
#include <mmx/Node_get_block_at.hxx>
#include <mmx/Node_get_block_at_return.hxx>
#include <mmx/Node_get_block_hash.hxx>
#include <mmx/Node_get_block_hash_return.hxx>

#include <vnx/vnx.h>
#include <vnx/NoSuchMethod.hxx>
#include <vnx/OverflowException.hxx>


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
	if(max_sync_peers < min_sync_peers) {
		throw std::logic_error("max_sync_peers < min_sync_peers");
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
	subscribe(input_vdfs, max_queue_ms);
	subscribe(input_blocks, max_queue_ms);
	subscribe(input_verified_vdfs, max_queue_ms);
	subscribe(input_verified_proof, max_queue_ms);
	subscribe(input_transactions, max_queue_ms);

	peer_set = seed_peers;
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

	threads = new vnx::ThreadPool(num_threads);

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

template<typename T>
std::vector<T> get_subset(const std::set<T>& candidates, const size_t max_count)
{
	std::set<T> result;
	if(max_count < candidates.size()) {
		// return a random subset
		const std::vector<T> tmp(candidates.begin(), candidates.end());
		for(size_t i = 0; i < 2 * candidates.size() && result.size() < max_count; ++i) {
			result.insert(tmp[vnx::rand64() % tmp.size()]);
		}
	} else {
		result = candidates;
	}
	return std::vector<T>(result.begin(), result.end());
}

std::vector<std::string> Router::get_peers(const uint32_t& max_count) const
{
	std::set<std::string> valid;
	for(const auto& entry : peer_map) {
		const auto& addr = entry.second.address;
		if(is_public_address(addr)) {
			valid.insert(addr);
		}
	}
	return get_subset(valid, max_count);
}

std::vector<std::string> Router::get_known_peers() const
{
	return std::vector<std::string>(peer_set.begin(), peer_set.end());
}

std::vector<std::string> Router::get_connected_peers() const
{
	std::vector<std::string> res;
	for(const auto& entry : peer_map) {
		res.push_back(entry.second.address);
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
		peer.address = state.address;
		peer.height = state.height;
		peer.credits = state.credits;
		peer.tx_credits = state.tx_credits;
		peer.ping_ms = state.ping_ms;
		peer.bytes_send = state.bytes_send;
		peer.bytes_recv = state.bytes_recv;
		peer.is_synced = state.is_synced;
		peer.is_blocked = state.is_blocked;
		peer.is_outbound = state.is_outbound;
		peer.recv_timeout_ms = now_ms - state.last_receive_ms;
		peer.connect_time_ms = now_ms - state.connected_since_ms;
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
	auto& job = sync_jobs[request_id];
	job.height = height;
	job.start_time_ms = vnx::get_wall_time_millis();
	((Router*)this)->process();
}

void Router::fetch_block_at_async(const std::string& address, const uint32_t& height, const vnx::request_id_t& request_id) const
{
	auto& job = fetch_jobs[request_id];
	job.height = height;
	job.from_peer = address;
	job.start_time_ms = vnx::get_wall_time_millis();
	((Router*)this)->process();
}

void Router::handle(std::shared_ptr<const Block> block)
{
	if(!block->proof) {
		return;
	}
	if(relay_msg_hash(block->hash, block_credits)) {
		log(INFO) << "Broadcasting block for height " << block->height;
		send_all(block);
		block_counter++;
	}
	for(const auto& tx : block->tx_list) {
		auto iter = hash_info.find(tx->id);
		if(iter != hash_info.end()) {
			auto& info = iter->second;
			if(!info.received_from.empty()) {
				if(auto peer = find_peer(info.received_from.front())) {
					peer->tx_credits += tx_credits;
				}
			}
		}
	}
}

void Router::handle(std::shared_ptr<const Transaction> tx)
{
	if(relay_msg_hash(tx->id)) {
		log(INFO) << "Broadcasting transaction " << tx->id;
		send_all(tx);
		tx_counter++;
	}
}

void Router::handle(std::shared_ptr<const ProofOfTime> proof)
{
	if(proof->height > verified_vdf_height) {
		if(relay_msg_hash(proof->calc_hash(), vdf_credits)) {
			if(vnx_sample && vnx_sample->topic == input_vdfs) {
				log(INFO) << "Broadcasting VDF for height " << proof->height;
			}
			send_all(proof);
			vdf_counter++;
		}
	}
	verified_vdf_height = std::max(proof->height, verified_vdf_height);
}

void Router::handle(std::shared_ptr<const ProofResponse> value)
{
	if(auto proof = value->proof) {
		if(relay_msg_hash(proof->calc_hash())) {
			if(vnx::get_pipe(value->farmer_addr)) {
				log(INFO) << "Broadcasting proof for height " << value->request->height << " with score " << value->score;
			}
			send_all(value);
			proof_counter++;
		}
		const auto farmer_id = hash_t(proof->farmer_key);
		farmer_credits[farmer_id] += proof_credits;
	}
}

uint32_t Router::send_request(peer_t& peer, std::shared_ptr<const vnx::Value> method, bool reliable)
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

	for(auto& entry : peer_map) {
		auto& peer = entry.second;
		peer.credits = std::min(peer.credits, max_node_credits);
		peer.tx_credits = std::min(peer.tx_credits + tx_credits, max_node_tx_credits);

		// check pending transactions
		while(!peer.tx_queue.empty()) {
			const auto& tx = peer.tx_queue.front();
			if(peer.tx_credits >= tx_relay_cost) {
				if(relay_msg_hash(tx->id)) {
					peer.tx_credits -= tx_relay_cost;
					relay(peer.client, tx);
					tx_counter++;
				}
			} else {
				auto iter = hash_info.find(tx->id);
				if(iter != hash_info.end() && !iter->second.did_relay) {
					break;
				}
			}
			peer.tx_queue.pop();
		}
	}
	for(auto& entry : farmer_credits) {
		entry.second = std::min(entry.second, max_farmer_credits);
	}

	// clear seen hashes
	if(hash_queue.size() > max_hash_cache) {
		const auto num_clear = hash_queue.size() - max_hash_cache;
		for(size_t i = 0; i < num_clear && !hash_queue.empty(); ++i) {
			const auto hash = hash_queue.front();
			hash_info.erase(hash);
			hash_queue.pop();
		}
	}

	// check if we lost sync due to response timeout
	{
		size_t num_peers = 0;
		for(const auto& entry : peer_map) {
			const auto& peer = entry.second;
			if(peer.is_synced && now_ms - peer.last_receive_ms < sync_loss_delay * 1000) {
				num_peers++;
			}
		}
		if(num_peers < min_sync_peers) {
			if(is_connected) {
				log(WARN) << "Lost sync with network due to timeout!";
				is_connected = false;
				node->start_sync();
			}
		} else {
			is_connected = true;
		}
	}

	if(synced_peers.size() >= min_sync_peers)
	{
		// check for sync job timeouts
		for(auto& entry : sync_jobs) {
			auto& job = entry.second;
			if(now_ms - job.start_time_ms > fetch_timeout_ms) {
				for(auto client : job.pending) {
					synced_peers.erase(client);
				}
				{
					const auto height = job.height;
					job = sync_job_t();
					job.height = height;
				}
				job.start_time_ms = now_ms;
				log(WARN) << "Timeout on sync job for height " << job.height << ", trying again ...";
			}
		}
	}

	// check for fetch job timeouts
	for(auto iter = fetch_jobs.begin(); iter != fetch_jobs.end();) {
		const auto& job = iter->second;
		if(now_ms - job.start_time_ms > fetch_timeout_ms) {
			vnx_async_return_ex_what(iter->first, "fetch timeout");
			iter = fetch_jobs.erase(iter);
			continue;
		}
		iter++;
	}

	process();
}

bool Router::process(std::shared_ptr<const Return> ret)
{
	const auto now_ms = vnx::get_wall_time_millis();

	bool did_consume = false;
	for(auto iter = sync_jobs.begin(); iter != sync_jobs.end();)
	{
		auto& job = iter->second;
		if(ret) {
			// check for any returns
			auto iter = job.request_map.find(ret->id);
			if(iter != job.request_map.end()) {
				if(auto result = std::dynamic_pointer_cast<const Node_get_block_hash_return>(ret->result)) {
					if(auto hash = result->_ret_0) {
						job.hash_map[*hash].insert(iter->second);
						job.succeeded.insert(iter->second);
					} else {
						job.failed.insert(iter->second);
					}
				}
				else if(auto result = std::dynamic_pointer_cast<const Node_get_block_return>(ret->result)) {
					if(auto block = result->_ret_0) {
						job.blocks[block->hash] = block;
						job.succeeded.insert(iter->second);
					} else {
						job.failed.insert(iter->second);
						synced_peers.erase(iter->second);
					}
				}
				else if(auto result = std::dynamic_pointer_cast<const vnx::OverflowException>(ret->result)) {
					synced_peers.erase(iter->second);
				}
				else {
					job.failed.insert(iter->second);
				}
				job.pending.erase(iter->second);
				job.request_map.erase(iter);
				did_consume = true;
			}
		}
		if(job.state == FETCH_HASHES)
		{
			if(job.succeeded.size() < min_sync_peers && job.failed.size() < min_sync_peers)
			{
				auto peers = synced_peers;
				for(auto client : job.failed) {
					peers.erase(client);
				}
				for(auto client : job.pending) {
					peers.erase(client);
				}
				for(auto client : job.succeeded) {
					peers.erase(client);
				}
				for(auto client : get_subset(peers, min_sync_peers))
				{
					if(job.succeeded.size() + job.pending.size() + job.failed.size() >= max_sync_peers) {
						break;
					}
					auto req = Node_get_block_hash::create();
					req->height = job.height;
					const auto id = send_request(client, req);
					job.request_map[id] = client;
					job.pending.insert(client);
				}
			} else {
				log(DEBUG) << "Got " << job.hash_map.size() << " block hashes for height "
						<< job.height << " from " << job.succeeded.size() << " peers, " << job.failed.size() << " failed";
				job.failed.clear();
				job.pending.clear();
				job.succeeded.clear();
				job.request_map.clear();
				job.state = FETCH_BLOCKS;
				job.start_time_ms = now_ms;
			}
		}
		if(job.state == FETCH_BLOCKS)
		{
			size_t num_clients = 0;
			for(const auto& entry : job.hash_map) {
				num_clients += entry.second.size();
			}
			if(num_clients > 0 && job.failed.size() >= num_clients) {
				log(WARN) << "Fetching " << job.hash_map.size() << " blocks from " << num_clients << " peers failed, trying again ...";
				const auto height = job.height;
				job = sync_job_t();
				job.height = height;
				job.start_time_ms = now_ms;
			}
			else if(job.blocks.size() < job.hash_map.size())
			{
				for(const auto& entry : job.hash_map) {
					if(!job.blocks.count(entry.first))
					{
						size_t num_pending = 0;
						for(auto client : entry.second) {
							if(job.pending.count(client)) {
								num_pending++;
							}
						}
						const auto max_pending = std::max<size_t>((now_ms - job.start_time_ms) / update_interval_ms, 1);
						if(num_pending < max_pending)
						{
							auto clients = entry.second;
							for(auto id : job.failed) {
								clients.erase(id);
							}
							for(auto id : job.pending) {
								clients.erase(id);
							}
							for(auto client : get_subset(clients, max_pending - num_pending))
							{
								auto req = Node_get_block::create();
								req->hash = entry.first;
								const auto id = send_request(client, req);
								job.request_map[id] = client;
								job.pending.insert(client);
							}
						}
					}
				}
			} else {
				if(!job.hash_map.empty()) {
					log(DEBUG) << "Got " << job.blocks.size() << " / " << job.hash_map.size() << " blocks for height " << job.height << " by fetching "
							<< job.succeeded.size() + job.pending.size() + job.failed.size() << " times, " << job.failed.size() << " failed";
				}
				// we are done with the job
				std::vector<std::shared_ptr<const Block>> blocks;
				for(const auto& entry : job.blocks) {
					blocks.push_back(entry.second);
				}
				get_blocks_at_async_return(iter->first, blocks);
				iter = sync_jobs.erase(iter);
				continue;
			}
		}
		iter++;
	}
	for(auto iter = fetch_jobs.begin(); iter != fetch_jobs.end();)
	{
		auto& job = iter->second;
		if(ret) {
			// check for any returns
			if(job.request_map.count(ret->id)) {
				if(auto result = std::dynamic_pointer_cast<const Node_get_block_at_return>(ret->result)) {
					fetch_block_at_async_return(iter->first, result->_ret_0);
				}
				else if(auto result = std::dynamic_pointer_cast<const vnx::Exception>(ret->result)) {
					vnx_async_return(iter->first, result);
				}
				else {
					vnx_async_return_ex_what(iter->first, "request failed");
				}
				iter = fetch_jobs.erase(iter);
				did_consume = true;
				continue;
			}
		}
		if(job.request_map.empty()) {
			peer_t* peer = nullptr;
			for(auto& entry : peer_map) {
				if(entry.second.address == job.from_peer) {
					peer = &entry.second;
					break;
				}
			}
			if(!peer) {
				vnx_async_return_ex_what(iter->first, "no such peer");
				iter = fetch_jobs.erase(iter);
				continue;
			}
			auto req = Node_get_block_at::create();
			req->height = job.height;
			const auto id = send_request(*peer, req);
			job.request_map[id] = peer->client;
		}
		iter++;
	}
	return did_consume;
}

void Router::connect()
{
	if(synced_peers.size() < num_peers_out)
	{
		std::set<std::string> peers;
		for(const auto& address : peer_set) {
			bool connected = false;
			for(const auto& entry : peer_map) {
				if(address == entry.second.address) {
					connected = true;
				}
			}
			if(!connected && !block_peers.count(address) && !connecting_peers.count(address)) {
				peers.insert(address);
			}
		}
		for(const auto& address : get_subset(peers, num_peers_out))
		{
			if(connecting_peers.size() >= num_threads) {
				break;
			}
			log(DEBUG) << "Trying to connect to " << address;

			connecting_peers.insert(address);
			threads->add_task(std::bind(&Router::connect_task, this, address));
		}
	}
	else if(synced_peers.size() > num_peers_out + 1)
	{
		for(auto client : get_subset(synced_peers, synced_peers.size() - num_peers_out)) {
			if(auto peer = find_peer(client)) {
				if(peer->is_outbound) {
					log(INFO) << "Disconnecting from " << peer->address << " to reduce connections";
					disconnect(client);
				}
			}
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
		send_all(req, false);
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
					const auto height = *sync_height - params->finality_delay;
					node->get_block_hash(height,
						[this, height](const vnx::optional<hash_t>& hash) {
							if(hash) {
								fork_check.height = height;
								fork_check.our_hash = *hash;
								fork_check.hash_count.clear();
								fork_check.request_map.clear();
								auto req = Node_get_block_hash::create();
								req->height = height;
								for(auto& entry : peer_map) {
									auto& peer = entry.second;
									if(peer.is_synced && peer.is_outbound) {
										const auto id = send_request(peer, req);
										fork_check.request_map[id] = peer.client;
									}
								}
							}
						});
				}
			}
		});
}

void Router::discover()
{
	auto method = Router_get_peers::create();
	method->max_count = num_peers_out;
	auto req = Request::create();
	req->id = next_request_id++;
	req->method = method;
	send_all(req, false);
}

void Router::save_data()
{
	{
		vnx::File file(storage_path + "known_peers.dat");
		try {
			file.open("wb");
			vnx::write_generic(file.out, peer_set);
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
		if(synced_peers.size() >= num_peers_out) {
			vnx::TcpEndpoint().close(sock);
			return;
		}
		const auto client = add_client(sock, address);

		auto& peer = peer_map[client];
		peer.is_outbound = true;
	}
	else if(!seed_peers.count(address)) {
		peer_set.erase(address);
	}
}

void Router::connect_task(const std::string& address) noexcept
{
	vnx::TcpEndpoint peer;
	peer.host_name = address;
	peer.port = params->port;
	auto sock = peer.open();
	try {
		peer.connect(sock);
	}
	catch(const std::exception& ex) {
		if(show_warnings) {
			log(WARN) << "Connecting to peer " << address << " failed with: " << ex.what();
		}
		peer.close(sock);
		sock = -1;
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
			  << " peers, " << upload_counter << " upload, "
			  << tx_drop_counter << " / " << vdf_drop_counter << " / " << proof_drop_counter << " / " << block_drop_counter << " dropped";
	tx_counter = 0;
	vdf_counter = 0;
	proof_counter = 0;
	block_counter = 0;
	upload_counter = 0;
	tx_drop_counter = 0;
	vdf_drop_counter = 0;
	proof_drop_counter = 0;
	block_drop_counter = 0;
}

void Router::on_vdf(uint64_t client, std::shared_ptr<const ProofOfTime> proof)
{
	const auto peer = find_peer(client);
	if(!peer) {
		return;
	}
	const auto hash = proof->calc_hash();

	const bool has_credits = peer->credits >= vdf_relay_cost;
	if(has_credits) {
		if(relay_msg_hash(hash)) {
			peer->credits -= vdf_relay_cost;
			relay(client, proof);
			vdf_counter++;
		}
	}
	if(!receive_msg_hash(hash, client)) {
		return;
	}
	if(!has_credits) {
		log(INFO) << "Peer " << peer->address << " has insufficient credits to relay VDF for height " << proof->height << ", verifying first.";
	}
	publish(proof, output_vdfs);
}

void Router::on_block(uint64_t client, std::shared_ptr<const Block> block)
{
	if(!receive_msg_hash(block->hash, client, block_relay_cost)) {
		return;
	}
	const auto proof = block->proof;
	if(!proof || !block->is_valid() || !block->farmer_sig
		|| !proof->local_sig.verify(proof->local_key, proof->calc_hash())
		|| !block->farmer_sig->verify(proof->farmer_key, block->hash))
	{
		if(auto peer = find_peer(client)) {
			block_peers.insert(peer->address);
			disconnect(client);
			log(WARN) << "Banned peer " << peer->address << " because they sent us an invalid block.";
		}
		return;
	}
	const auto farmer_id = hash_t(proof->farmer_key);
	const auto iter = farmer_credits.find(farmer_id);
	if(iter != farmer_credits.end()) {
		if(iter->second >= block_relay_cost) {
			if(relay_msg_hash(block->hash)) {
				iter->second -= block_relay_cost;
				relay(client, block);
				block_counter++;
			}
		} else {
			log(WARN) << "A farmer has insufficient credits to relay block at height " << block->height << ", verifying first.";
		}
	} else {
		log(INFO) << "Got block from an unknown farmer at height " << block->height << ", verifying first.";
	}
	publish(block, output_blocks);
}

void Router::on_proof(uint64_t client, std::shared_ptr<const ProofResponse> response)
{
	const auto proof = response->proof;
	const auto request = response->request;
	if(!proof || !request) {
		return;
	}
	const auto hash = proof->calc_hash();
	if(!receive_msg_hash(hash, client, proof_relay_cost)) {
		return;
	}
	if(!proof->local_sig.verify(proof->local_key, hash)) {
		if(auto peer = find_peer(client)) {
			block_peers.insert(peer->address);
			disconnect(client);
			log(WARN) << "Banned peer " << peer->address << " because they sent us invalid proof.";
		}
		return;
	}
	const auto farmer_id = hash_t(proof->farmer_key);
	const auto iter = farmer_credits.find(farmer_id);
	if(iter != farmer_credits.end()) {
		if(iter->second >= proof_relay_cost) {
			if(relay_msg_hash(hash)) {
				iter->second -= proof_relay_cost;
				relay(client, response);
				proof_counter++;
			}
		} else {
			log(DEBUG) << "A farmer has insufficient credits to relay proof for height " << request->height << " with score " << response->score << ", verifying first.";
		}
	} else {
		log(DEBUG) << "Got proof from an unknown farmer at height " << request->height << " with score " << response->score << ", verifying first.";
	}
	publish(response, output_proof);
}

void Router::on_transaction(uint64_t client, std::shared_ptr<const Transaction> tx)
{
	const auto peer = find_peer(client);
	if(!peer) {
		return;
	}
	if(!tx->is_valid()) {
		block_peers.insert(peer->address);
		disconnect(client);
		log(WARN) << "Banned peer " << peer->address << " because they sent us an invalid transaction.";
		return;
	}
	const bool has_credits = peer->tx_credits >= tx_relay_cost;
	if(has_credits) {
		if(relay_msg_hash(tx->id)) {
			peer->tx_credits -= tx_relay_cost;
			relay(client, tx);
			tx_counter++;
		}
	}
	if(!receive_msg_hash(tx->id, client)) {
		return;
	}
	if(!has_credits) {
		peer->tx_queue.push(tx);
	}
	publish(tx, output_transactions);
}

void Router::relay(uint64_t source, std::shared_ptr<const vnx::Value> msg)
{
	for(auto& entry : peer_map) {
		auto& peer = entry.second;
		if(entry.first != source) {
			send_to(peer, msg, false);
		}
	}
}

void Router::send_to(uint64_t client, std::shared_ptr<const vnx::Value> msg, bool reliable)
{
	if(auto peer = find_peer(client)) {
		send_to(*peer, msg, reliable);
	}
}

void Router::send_to(peer_t& peer, std::shared_ptr<const vnx::Value> msg, bool reliable)
{
	if(peer.is_blocked && !reliable) {
		switch(msg->get_type_code()->type_hash) {
			case Block::VNX_TYPE_ID: block_drop_counter++; break;
			case Transaction::VNX_TYPE_ID: tx_drop_counter++; break;
			case ProofOfTime::VNX_TYPE_ID: vdf_drop_counter++; break;
			case ProofResponse::VNX_TYPE_ID: proof_drop_counter++; break;
			case Request::VNX_TYPE_ID:
				if(auto req = std::dynamic_pointer_cast<const Request>(msg)) {
					auto ret = Return::create();
					ret->id = req->id;
					ret->result = vnx::OverflowException::create();
					add_task(std::bind(&Router::on_return, this, peer.client, ret));
				}
				break;
		}
		drop_counter++;
		return;
	}
	auto& out = peer.out;
	vnx::write(out, uint16_t(vnx::CODE_UINT32));
	vnx::write(out, uint32_t(0));
	vnx::write(out, msg);
	out.flush();

	auto buffer = std::make_shared<vnx::Buffer>(peer.data);
	peer.data.clear();

	if(buffer->size() > max_msg_size) {
		return;
	}
	*((uint32_t*)buffer->data(2)) = buffer->size() - 6;

	peer.bytes_send += buffer->size();
	Super::send_to(peer.client, buffer);
}

void Router::send_all(std::shared_ptr<const vnx::Value> msg, bool reliable)
{
	for(auto& entry : peer_map) {
		send_to(entry.second, msg, reliable);
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
						log(INFO) << "Discovered our own address: " << peer->address;
						self_addrs.insert(peer->address);
						block_peers.insert(peer->address);
					}
					disconnect(client);
				}
			}
			break;
		case Router_sign_msg_return::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Router_sign_msg_return>(result)) {
				if(auto peer = find_peer(client)) {
					const auto& pair = value->_ret_0;
					if(pair.second.verify(pair.first, hash_t(peer->challenge.bytes))) {
						peer->node_id = pair.first.get_addr();
						// TODO: load credits
					} else {
						log(WARN) << "Peer " << peer->address << " failed to verify identity!";
					}
				}
			}
			break;
		case Router_get_peers_return::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Router_get_peers_return>(result)) {
				peer_set.insert(value->_ret_0.begin(), value->_ret_0.end());
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
						synced_peers.insert(client);
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
	}
}

void Router::on_buffer(uint64_t client, void*& buffer, size_t& max_bytes)
{
	auto& peer = get_peer(client);
	const auto offset = peer.buffer.size();
	if(peer.msg_size == 0) {
		if(offset > 6) {
			throw std::logic_error("offset > 6");
		}
		peer.buffer.reserve(6);
		max_bytes = 6 - offset;
	} else {
		max_bytes = (6 + peer.msg_size) - offset;
	}
	buffer = peer.buffer.data(offset);
}

bool Router::on_read(uint64_t client, size_t num_bytes)
{
	auto& peer = get_peer(client);
	peer.bytes_recv += num_bytes;
	peer.buffer.resize(peer.buffer.size() + num_bytes);

	if(peer.msg_size == 0) {
		if(peer.buffer.size() >= 6) {
			uint16_t code = 0;
			vnx::read_value(peer.buffer.data(), code);
			vnx::read_value(peer.buffer.data(2), peer.msg_size, &code);
			if(peer.msg_size > max_msg_size) {
				throw std::logic_error("message too large");
			}
			if(peer.msg_size > 0) {
				peer.buffer.reserve(6 + peer.msg_size);
			} else {
				peer.buffer.clear();
			}
		}
	}
	else if(peer.buffer.size() == 6 + peer.msg_size)
	{
		peer.in.read(6);
		if(auto value = vnx::read(peer.in)) {
			try {
				on_msg(client, value);
			}
			catch(const std::exception& ex) {
				if(show_warnings) {
					log(WARN) << "on_msg() failed with: " << ex.what();
				}
			}
		}
		peer.buffer.clear();
		peer.in_stream.reset();
		peer.msg_size = 0;
	}
	return true;
}

void Router::on_pause(uint64_t client)
{
	auto& peer = get_peer(client);
	peer.is_blocked = true;

	if(!peer.is_outbound) {
		pause(client);		// pause incoming traffic
	}
}

void Router::on_resume(uint64_t client)
{
	auto& peer = get_peer(client);
	peer.is_blocked = false;

	if(!peer.is_outbound) {
		resume(client);		// resume incoming traffic
	}
}

void Router::on_connect(uint64_t client, const std::string& address)
{
	if(block_peers.count(address)) {
		disconnect(client);
		return;
	}
	const auto seed = vnx::rand64();

	auto& peer = peer_map[client];
	peer.client = client;
	peer.address = address;
	peer.challenge = hash_t(&seed, sizeof(seed));
	peer.connected_since_ms = vnx::get_wall_time_millis();
	peer_set.insert(address);

	send_request(peer, Router_get_id::create());
	send_request(peer, Node_get_synced_height::create());

	auto req = Router_sign_msg::create();
	req->msg = peer.challenge;
	send_request(peer, req);

	log(INFO) << "Connected to peer " << peer.address;
}

void Router::on_disconnect(uint64_t client)
{
	if(!vnx_task) {
		// cannot modify peer_map in a nested loop
		add_task(std::bind(&Router::on_disconnect, this, client));
		return;
	}
	auto iter = peer_map.find(client);
	if(iter != peer_map.end()) {
		log(INFO) << "Peer " << iter->second.address << " disconnected";
		peer_map.erase(iter);
	}
	synced_peers.erase(client);
}

Router::peer_t& Router::get_peer(uint64_t client)
{
	if(auto peer = find_peer(client)) {
		return *peer;
	}
	throw std::logic_error("no such peer");
}

Router::peer_t* Router::find_peer(uint64_t client)
{
	auto iter = peer_map.find(client);
	if(iter != peer_map.end()) {
		return &iter->second;
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
	info.is_valid = true;

	if(credits && !info.is_rewarded && !info.received_from.empty())
	{
		if(auto peer = find_peer(info.received_from.front())) {
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

bool Router::receive_msg_hash(const hash_t& hash, uint64_t client, uint32_t credits)
{
	const auto ret = hash_info.emplace(hash, hash_info_t());
	if(ret.second) {
		hash_queue.push(hash);
	}
	auto& info = ret.first->second;
	const bool is_new = info.received_from.empty();
	if(std::find(info.received_from.begin(), info.received_from.end(), client) == info.received_from.end())
	{
		if(credits && info.is_valid && !is_new) {
			if(auto peer = find_peer(client)) {
				peer->credits += credits;
			}
		}
		info.received_from.push_back(client);
	}
	return is_new;
}


} // mmx
