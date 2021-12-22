/*
 * Router.cpp
 *
 *  Created on: Dec 17, 2021
 *      Author: mad
 */

#include <mmx/Router.h>
#include <mmx/utils.h>

#include <mmx/Return.hxx>
#include <mmx/Request.hxx>
#include <mmx/Router_get_peers.hxx>
#include <mmx/Router_get_peers_return.hxx>
#include <mmx/Node_get_height.hxx>
#include <mmx/Node_get_height_return.hxx>
#include <mmx/Node_get_block.hxx>
#include <mmx/Node_get_block_return.hxx>
#include <mmx/Node_get_block_at.hxx>
#include <mmx/Node_get_block_at_return.hxx>
#include <mmx/Node_get_block_hash.hxx>
#include <mmx/Node_get_block_hash_return.hxx>

#include <vnx/vnx.h>


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
	subscribe(input_vdfs, max_queue_ms);
	subscribe(input_blocks, max_queue_ms);
	subscribe(input_verified_vdfs, max_queue_ms);
	subscribe(input_transactions, max_queue_ms);

	peer_set = seed_peers;

	vnx::File known_peers("known_peers.dat");
	if(known_peers.exists()) {
		std::vector<std::string> peers;
		try {
			known_peers.open("rb");
			vnx::read_generic(known_peers.in, peers);
			known_peers.close();
		}
		catch(const std::exception& ex) {
			log(WARN) << "Failed to read peers from file: " << ex.what();
		}
		peer_set.insert(peers.begin(), peers.end());
	}
	log(INFO) << "Loaded " << peer_set.size() << " known peers";

	node = std::make_shared<NodeAsyncClient>(node_server);
	node->vnx_set_non_blocking(true);
	add_async_client(node);

	threads = std::make_shared<vnx::ThreadPool>(num_peers_out);

	set_timer_millis(info_interval_ms, std::bind(&Router::print_stats, this));
	set_timer_millis(update_interval_ms, std::bind(&Router::update, this));
	set_timer_millis(connect_interval_ms, std::bind(&Router::connect, this));
	set_timer_millis(discover_interval * 1000, std::bind(&Router::discover, this));

	connect();

	Super::main();

	try {
		known_peers.open("wb");
		vnx::write_generic(known_peers.out, peer_set);
		known_peers.close();
	}
	catch(const std::exception& ex) {
		log(WARN) << "Failed to write peers to file: " << ex.what();
	}

	threads->close();
}

std::vector<std::string> Router::get_peers(const uint32_t& max_count) const
{
	std::set<std::string> res;
	if(max_count < peer_set.size()) {
		// return a random subset
		const std::vector<std::string> tmp(peer_set.begin(), peer_set.end());
		for(size_t i = 0; i < peer_set.size() && res.size() < max_count; ++i) {
			res.insert(tmp[size_t(::rand()) % tmp.size()]);
		}
	} else {
		res = peer_set;
	}
	return std::vector<std::string>(res.begin(), res.end());
}

void Router::get_blocks_at_async(const uint32_t& height, const vnx::request_id_t& request_id) const
{
	auto& job = sync_jobs[request_id];
	job.height = height;
	((Router*)this)->update();
}

void Router::handle(std::shared_ptr<const Block> block)
{
	if(!block->proof) {
		return;
	}
	if(seen_hashes.insert(block->hash).second) {
		log(INFO) << "Broadcasting block " << block->height;
		send_all(block);
	}
}

void Router::handle(std::shared_ptr<const Transaction> tx)
{
	if(seen_hashes.insert(tx->id).second) {
		log(INFO) << "Broadcasting transaction " << tx->id;
		send_all(tx);
	}
}

void Router::handle(std::shared_ptr<const ProofOfTime> proof)
{
	const auto vdf_iters = proof->start + proof->get_num_iters();

	if(vnx_sample->topic == input_vdfs)
	{
		if(vdf_iters > verified_vdf_iters) {
			if(seen_hashes.insert(proof->calc_hash()).second) {
				log(INFO) << "Broadcasting VDF for " << vdf_iters;
				send_all(proof);
			}
		}
	}
	else if(vnx_sample->topic == input_verified_vdfs)
	{
		verified_vdf_iters = std::max(vdf_iters, verified_vdf_iters);
	}
}

uint32_t Router::send_request(uint64_t client, std::shared_ptr<const vnx::Value> method)
{
	auto req = Request::create();
	req->id = next_request_id++;
	req->method = method;
	send_to(client, req);
	return_map[req->id] = nullptr;
	return req->id;
}

void Router::update()
{
	for(auto iter = sync_jobs.begin(); iter != sync_jobs.end();)
	{
		auto& job = iter->second;
		for(auto iter = job.pending.begin(); iter != job.pending.end();)
		{
			// check for any returns
			auto iter2 = return_map.find(iter->second);
			if(iter2 != return_map.end()) {
				if(auto value = iter2->second) {
					return_map.erase(iter2);
					if(auto ret = std::dynamic_pointer_cast<const Node_get_block_hash_return>(value)) {
						if(auto hash = ret->_ret_0.get()) {
							job.hash_map[*hash].insert(iter->first);
							job.succeeded.insert(iter->first);
						} else {
							job.failed.insert(iter->first);
						}
						iter = job.pending.erase(iter);
						continue;
					}
					if(auto ret = std::dynamic_pointer_cast<const Node_get_block_return>(value)) {
						if(auto block = ret->_ret_0) {
							job.blocks[block->hash] = block;
							job.succeeded.insert(iter->first);
						} else {
							job.failed.insert(iter->first);
						}
						iter = job.pending.erase(iter);
						continue;
					}
				}
			}
			iter++;
		}
		const auto num_peers_try = std::min<size_t>(max_sync_peers, std::max<size_t>(outgoing_peers.size(), min_sync_peers));

		if(job.state == FETCH_HASHES)
		{
			if((job.succeeded.size() < min_sync_peers) && (job.succeeded.size() + job.failed.size() < num_peers_try))
			{
				for(auto client : outgoing_peers) {
					if(job.succeeded.size() + job.pending.size() + job.failed.size() >= max_sync_peers) {
						break;
					}
					if(!job.succeeded.count(client) && !job.pending.count(client) && !job.failed.count(client))
					{
						auto req = Node_get_block_hash::create();
						req->height = job.height;
						job.pending[client] = send_request(client, req);
					}
				}
			} else {
				for(const auto& entry : job.pending) {
					return_map.erase(entry.second);
				}
				job.failed.clear();
				job.pending.clear();
				job.succeeded.clear();
				job.state = FETCH_BLOCKS;
			}
		}
		if(job.state == FETCH_BLOCKS)
		{
			if(job.blocks.size() < job.hash_map.size()
				&& (job.succeeded.size() < min_sync_peers) && (job.succeeded.size() + job.failed.size() < num_peers_try))
			{
				for(const auto& entry : job.hash_map) {
					if(!job.blocks.count(entry.first))
					{
						size_t num_pending = 0;
						for(auto client : entry.second) {
							if(!job.succeeded.count(client) && !job.failed.count(client))
							{
								if(!job.pending.count(client)) {
									auto req = Node_get_block::create();
									req->hash = entry.first;
									job.pending[client] = send_request(client, req);
								}
								if(++num_pending >= min_sync_peers) {
									break;
								}
							}
						}
					}
				}
			}
			else {
				// we are done with the job
				std::vector<std::shared_ptr<const Block>> blocks;
				for(const auto& entry : job.blocks) {
					blocks.push_back(entry.second);
				}
				get_blocks_at_async_return(iter->first, blocks);

				for(const auto& entry : job.pending) {
					return_map.erase(entry.second);
				}
				iter = sync_jobs.erase(iter);
				continue;
			}
		}
		iter++;
	}
}

void Router::connect()
{
	for(const auto& address : get_peers(num_peers_out))
	{
		if(outgoing_peers.size() + connecting_peers.size() >= num_peers_out) {
			break;
		}
		if(connecting_peers.count(address)) {
			continue;
		}
		bool connected = false;
		for(const auto& entry : peer_map) {
			if(address == entry.second.address) {
				connected = true;
			}
		}
		if(connected) {
			continue;
		}
		connecting_peers.insert(address);
		threads->add_task(std::bind(&Router::connect_task, this, address));
	}
}

void Router::discover()
{
	auto method = Router_get_peers::create();
	method->max_count = num_peers_out;
	auto req = Request::create();
	req->id = next_request_id++;
	req->method = method;
	send_all(req);
}

void Router::add_peer(const std::string& address, const int sock)
{
	connecting_peers.erase(address);

	if(sock >= 0) {
		const auto id = add_client(sock, address);

		auto& peer = peer_map[id];
		peer.is_outbound = true;
		outgoing_peers.insert(id);
	}
	else if(!seed_peers.count(address)) {
		peer_set.erase(address);
	}
}

void Router::connect_task(const std::string& address) noexcept
{
	try {
		vnx::TcpEndpoint peer;
		peer.host_name = address;
		peer.port = params->port;
		const auto sock = peer.open();
		peer.connect(sock);
		add_task(std::bind(&Router::add_peer, this, address, sock));
	}
	catch(const std::exception& ex) {
		if(show_warnings) {
			log(WARN) << "Connecting to peer " << address << " failed with: " << ex.what();
		}
		add_task(std::bind(&Router::add_peer, this, address, -1));
	}
}

void Router::print_stats()
{
	log(INFO) << float(tx_counter * 1000) / info_interval_ms
			  << " tx/s, " << float(vdf_counter * 1000) / info_interval_ms
			  << " vdf/s, " << float(block_counter * 1000) / info_interval_ms
			  << " blocks/s, " << outgoing_peers.size() << " / " <<  peer_map.size() << " / " << peer_set.size()
			  << " peers, " << return_map.size() << " pending";
	tx_counter = 0;
	vdf_counter = 0;
	block_counter = 0;
}

void Router::on_vdf(uint64_t client, std::shared_ptr<const ProofOfTime> proof)
{
	if(!seen_hashes.insert(proof->calc_hash()).second) {
		return;
	}
	const auto vdf_iters = proof->start + proof->get_num_iters();

	if(vdf_iters > verified_vdf_iters) {
		publish(proof, output_vdfs);
	}
	relay(client, proof);
	vdf_counter++;
}

void Router::on_block(uint64_t client, std::shared_ptr<const Block> block)
{
	if(!seen_hashes.insert(block->hash).second) {
		return;
	}
	if(!block->is_valid()) {
		return;
	}
	publish(block, output_blocks);
	relay(client, block);
	block_counter++;
}

void Router::on_transaction(uint64_t client, std::shared_ptr<const Transaction> tx)
{
	if(!seen_hashes.insert(tx->id).second) {
		return;
	}
	if(!tx->is_valid()) {
		return;
	}
	relay(client, tx);
	publish(tx, output_transactions);
	tx_counter++;
}

void Router::relay(uint64_t source, std::shared_ptr<const vnx::Value> msg)
{
	for(auto& entry : peer_map) {
		if(!entry.second.is_outbound && entry.first != source) {
			send_to(entry.first, entry.second, msg);
		}
	}
}

void Router::send_to(uint64_t client, std::shared_ptr<const vnx::Value> msg)
{
	send_to(client, get_peer(client), msg);
}

void Router::send_to(uint64_t client, peer_t& peer, std::shared_ptr<const vnx::Value> msg)
{
	if(peer.is_blocked) {
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
	Super::send_to(client, buffer);
}

void Router::send_all(std::shared_ptr<const vnx::Value> msg)
{
	for(auto& entry : peer_map) {
		auto& peer = entry.second;
		if(!peer.is_blocked) {
			send_to(entry.first, peer, msg);
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

void Router::on_request(uint64_t client, std::shared_ptr<const Request> msg)
{
	const auto method = msg->method;
	if(!method) {
		return;
	}
	switch(method->get_type_hash())
	{
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
					});
		}
		break;
	case Node_get_block::VNX_TYPE_ID:
		if(auto value = std::dynamic_pointer_cast<const Node_get_block>(method)) {
			node->get_block(value->hash,
					[=](std::shared_ptr<const Block> block) {
						upload_counter++;
						send_result<Node_get_block_return>(client, msg->id, block);
					});
		}
		break;
	case Node_get_block_at::VNX_TYPE_ID:
		if(auto value = std::dynamic_pointer_cast<const Node_get_block_at>(method)) {
			node->get_block_at(value->height,
					[=](std::shared_ptr<const Block> block) {
						upload_counter++;
						send_result<Node_get_block_at_return>(client, msg->id, block);
					});
		}
		break;
	case Node_get_block_hash::VNX_TYPE_ID:
		if(auto value = std::dynamic_pointer_cast<const Node_get_block_hash>(method)) {
			node->get_block_hash(value->height,
					[=](const vnx::optional<hash_t>& hash) {
						send_result<Node_get_block_hash_return>(client, msg->id, hash);
					});
		}
		break;
	}
}

void Router::on_return(uint64_t client, std::shared_ptr<const Return> msg)
{
	const auto result = msg->result;
	if(!result) {
		return;
	}
	switch(result->get_type_hash())
	{
		case Router_get_peers_return::VNX_TYPE_ID:
			if(auto value = std::dynamic_pointer_cast<const Router_get_peers_return>(result)) {
				peer_set.insert(value->_ret_0.begin(), value->_ret_0.end());
			}
			break;
		default: {
			auto iter = return_map.find(msg->id);
			if(iter != return_map.end()) {
				iter->second = result;
				update();
			}
		}
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
	get_peer(client).is_blocked = true;
}

void Router::on_resume(uint64_t client)
{
	get_peer(client).is_blocked = false;
}

void Router::on_connect(uint64_t client, const std::string& address)
{
	auto& peer = peer_map[client];
	peer.address = address;
	peer_set.insert(address);

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
	outgoing_peers.erase(client);
}

Router::peer_t& Router::get_peer(uint64_t client)
{
	auto iter = peer_map.find(client);
	if(iter != peer_map.end()) {
		return iter->second;
	}
	throw std::logic_error("no such peer");
}


} // mmx
