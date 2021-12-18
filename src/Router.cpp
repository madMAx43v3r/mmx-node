/*
 * Router.cpp
 *
 *  Created on: Dec 17, 2021
 *      Author: mad
 */

#include <mmx/Router.h>
#include <mmx/utils.h>

#include <mmx/Router_get_peers.hxx>
#include <mmx/Router_get_peers_return.hxx>

#include <vnx/vnx.h>


namespace mmx {

Router::Router(const std::string& _vnx_name)
	:	RouterBase(_vnx_name)
{
}

void Router::init()
{
	vnx::open_pipe(vnx_name, this, 1000);
}

void Router::main()
{
	subscribe(input_vdfs, 1000);
	subscribe(input_blocks, 1000);
	subscribe(input_verified_vdfs, 1000);
	subscribe(input_transactions, 1000);

	params = get_params();
	peer_set.insert(seed_peers.begin(), seed_peers.end());

	threads = std::make_shared<vnx::ThreadPool>(min_peers);

	set_timer_millis(info_interval_ms, std::bind(&Router::print_stats, this));
	set_timer_millis(update_interval_ms, std::bind(&Router::update, this));
	set_timer_millis(discover_interval * 1000, std::bind(&Router::discover, this));

	Super::main();

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

std::vector<std::shared_ptr<const Block>> Router::get_blocks_at(const uint32_t& height) const
{
	// TODO
	return {};
}

void Router::handle(std::shared_ptr<const Block> block)
{
	if(seen_hashes.insert(block->hash).second) {
		send_all(block);
	}
}

void Router::handle(std::shared_ptr<const Transaction> tx)
{
	if(seen_hashes.insert(tx->id).second) {
		send_all(tx);
	}
}

void Router::handle(std::shared_ptr<const ProofOfTime> proof)
{
	const auto vdf_iters = proof->start + proof->get_num_iters();

	if(vnx_sample->topic == input_vdfs)
	{
		if(vdf_iters > verified_iters) {
			send_all(proof);
		}
	}
	else if(vnx_sample->topic == input_verified_vdfs)
	{
		verified_iters = std::max(vdf_iters, verified_iters);
	}
}

void Router::update()
{
	if(peer_map.size() + pending_peers.size() < min_peers)
	{
		std::set<std::string> peers = peer_set;
		for(const auto& entry : peer_map) {
			peers.erase(entry.second.address);
		}
		peers.erase(pending_peers.begin(), pending_peers.end());

		std::unordered_set<std::string> new_peers;
		const std::vector<std::string> tmp(peers.begin(), peers.end());
		const auto new_count = min_peers - peer_map.size() - pending_peers.size();
		for(size_t i = 0; i < 2 * new_count && new_peers.size() < std::min(new_count, tmp.size()); ++i) {
			new_peers.insert(tmp[size_t(::rand()) % tmp.size()]);
		}
		for(const auto& peer : new_peers) {
			threads->add_task(std::bind(&Router::connect_task, this, peer));
		}
	}
}

void Router::discover()
{
	auto req = Router_get_peers::create();
	req->max_count = min_peers;
	send_all(req);
}

void Router::add_peer(const std::string& address, const int sock)
{
	const auto id = add_client(sock);

	auto& peer = peer_map[id];
	peer.address = address;

	log(INFO) << "Connected to " << address;
}

void Router::connect_task(const std::string& address) noexcept
{
	try {
		vnx::TcpEndpoint peer;
		peer.host_name = address;
		peer.port = params->tcp_port;
		const auto sock = peer.open();
		peer.connect(sock);
		add_task(std::bind(&Router::add_peer, this, address, sock));
	}
	catch(const std::exception& ex) {
		if(show_warnings) {
			log(WARN) << "Connecting to " << address << " failed with: " << ex.what();
		}
	}
}

void Router::print_stats()
{
	log(INFO) << "tps = " << float(tx_counter * 1000) / info_interval_ms
			  << " / sec, vdfs = " << float(vdf_counter * 1000) / info_interval_ms
			  << " / sec, blocks = " << float(block_counter * 1000) / info_interval_ms
			  << " / sec, peers = " << peer_map.size() << " / " << peer_set.size();
	tx_counter = 0;
	vdf_counter = 0;
	block_counter = 0;
}

void Router::on_vdf(uint64_t client, std::shared_ptr<const ProofOfTime> proof)
{
	const auto vdf_iters = proof->start + proof->get_num_iters();

	if(vdf_iters > verified_iters) {
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

std::shared_ptr<vnx::Buffer> Router::serialize(std::shared_ptr<const vnx::Value> msg)
{
	vnx::Memory tmp;
	{
		vnx::MemoryOutputStream stream(&tmp);
		vnx::TypeOutput out(&stream);
		vnx::write(out, uint16_t(vnx::CODE_UINT32));
		vnx::write(out, uint32_t(0));
		vnx::write(out, msg);
		out.flush();
	}
	auto buffer = std::make_shared<vnx::Buffer>(tmp);
	if(buffer->size() > max_msg_size) {
		return nullptr;
	}
	*((uint32_t*)buffer->data(2)) = buffer->size();
	return buffer;
}

void Router::relay(uint64_t source, std::shared_ptr<const vnx::Value> msg)
{
	std::unordered_set<uint64_t> peers;
	{
		// random subset
		std::vector<uint64_t> tmp;
		for(const auto& entry : peer_map) {
			if(entry.first != source) {
				tmp.push_back(entry.first);
			}
		}
		const auto max_count = std::min<size_t>(tmp.size(), msg_fanout);
		for(uint32_t i = 0; i < 3 * msg_fanout && peers.size() < max_count; ++i) {
			peers.insert(tmp[size_t(::rand()) % tmp.size()]);
		}
	}
	if(auto data = serialize(msg)) {
		for(auto client : peers) {
			Super::send_to(client, data);
		}
	}
}

void Router::send_to(uint64_t client, std::shared_ptr<const vnx::Value> msg)
{
	if(auto data = serialize(msg)) {
		Super::send_to(client, data);
	}
}

void Router::send_all(std::shared_ptr<const vnx::Value> msg)
{
	if(auto data = serialize(msg)) {
		for(const auto& entry : peer_map) {
			Super::send_to(entry.first, data);
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
	case Router_get_peers::VNX_TYPE_ID:
		if(auto value = std::dynamic_pointer_cast<const Router_get_peers>(msg)) {
			auto ret = Router_get_peers_return::create();
			ret->_ret_0 = get_peers(value->max_count);
			send_to(client, ret);
		}
		break;
	case Router_get_peers_return::VNX_TYPE_ID:
		if(auto value = std::dynamic_pointer_cast<const Router_get_peers_return>(msg)) {
			peer_set.insert(value->_ret_0.begin(), value->_ret_0.end());
		}
		break;
	}
}

void Router::on_buffer(uint64_t client, void*& buffer, size_t& max_bytes)
{
	auto& peer = peer_map[client];
	const auto offset = peer.buffer.size();
	if(peer.msg_size == 0) {
		if(offset > 6) {
			throw std::logic_error("offset > 6");
		}
		peer.buffer.reserve(6);
		max_bytes = 6 - offset;
	} else {
		max_bytes = peer.msg_size - offset;
	}
	buffer = peer.buffer.data(offset);
}

bool Router::on_read(uint64_t client, size_t num_bytes)
{
	auto& peer = peer_map[client];
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
		vnx::BufferInputStream stream(&peer.buffer);
		vnx::TypeInput in(&stream);
		in.read(6);
		if(auto value = vnx::read(in)) {
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
		peer.msg_size = 0;
	}
	return true;
}

void Router::on_connect(uint64_t client)
{
	if(!peer_map.count(client))
	{
		auto& peer = peer_map[client];
		peer.is_inbound = true;
		peer.address = "TODO";
	//	peer_set.insert(peer.address);

		log(INFO) << "New peer connected from " << peer.address;
	}
	auto req = Router_get_peers::create();
	req->max_count = min_peers;
	send_to(client, req);
}

void Router::on_disconnect(uint64_t client)
{
	auto iter = peer_map.find(client);
	if(iter != peer_map.end()) {
		log(INFO) << "Peer " << iter->second.address << " disconnected";
		peer_map.erase(iter);
	}
}


} // mmx
