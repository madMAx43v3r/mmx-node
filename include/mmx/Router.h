/*
 * Router.h
 *
 *  Created on: Dec 17, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_ROUTER_H_
#define INCLUDE_MMX_ROUTER_H_

#include <mmx/RouterBase.hxx>
#include <mmx/NodeAsyncClient.hxx>

#include <vnx/Buffer.h>
#include <vnx/Output.hpp>
#include <vnx/ThreadPool.h>


namespace mmx {

class Router : public RouterBase {
public:
	Router(const std::string& _vnx_name);

protected:
	void init() override;

	void main() override;

	std::vector<std::string> get_peers(const uint32_t& max_count) const override;

	std::vector<std::shared_ptr<const Block>> get_blocks_at(const uint32_t& height) const override;

	void handle(std::shared_ptr<const Block> value);

	void handle(std::shared_ptr<const Transaction> value);

	void handle(std::shared_ptr<const ProofOfTime> value);

private:
	struct peer_t {
		bool is_outbound = false;
		std::string address;
		vnx::Buffer buffer;
		uint32_t msg_size = 0;
		std::unordered_set<vnx::Hash64> type_codes;
	};

	void update();

	void discover();

	void add_peer(const std::string& address, const int sock);

	void connect_task(const std::string& peer) noexcept;

	void print_stats();

	void on_vdf(uint64_t client, std::shared_ptr<const ProofOfTime> proof);

	void on_block(uint64_t client, std::shared_ptr<const Block> block);

	void on_transaction(uint64_t client, std::shared_ptr<const Transaction> tx);

	std::shared_ptr<vnx::Buffer> serialize(std::shared_ptr<const vnx::Value> msg);

	void send_type_code(uint64_t client, peer_t& peer, const vnx::TypeCode* type_code);

	void relay(uint64_t source, std::shared_ptr<const vnx::Value> msg);

	void send_to(uint64_t client, std::shared_ptr<const vnx::Value> msg);

	void send_all(std::shared_ptr<const vnx::Value> msg);

	void on_msg(uint64_t client, std::shared_ptr<const vnx::Value> msg);

	void on_buffer(uint64_t client, void*& buffer, size_t& max_bytes) override;

	bool on_read(uint64_t client, size_t num_bytes) override;

	void on_connect(uint64_t client) override;

	void on_disconnect(uint64_t client) override;

private:
	std::set<std::string> peer_set;
	std::set<std::string> pending_peers;
	std::unordered_set<hash_t> seen_hashes;
	std::unordered_map<uint64_t, peer_t> peer_map;

	std::shared_ptr<NodeAsyncClient> node;
	std::shared_ptr<vnx::ThreadPool> threads;
	std::shared_ptr<const ChainParams> params;

	uint64_t verified_iters = 0;

	size_t tx_counter = 0;
	size_t vdf_counter = 0;
	size_t block_counter = 0;

};


} // mmx

#endif /* INCLUDE_MMX_ROUTER_H_ */
