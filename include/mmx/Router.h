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

	void discover() override;

	hash_t get_id() const override;

	node_info_t get_info() const override;

	std::pair<pubkey_t, signature_t> sign_msg(const hash_t& msg) const override;

	std::vector<std::string> get_peers(const uint32_t& max_count) const override;

	std::vector<std::string> get_known_peers() const override;

	std::vector<std::string> get_connected_peers() const override;

	std::shared_ptr<const PeerInfo> get_peer_info() const override;

	std::vector<std::pair<std::string, uint32_t>> get_farmer_credits() const override;

	void get_blocks_at_async(const uint32_t& height, const vnx::request_id_t& request_id) const override;

	void fetch_block_at_async(const std::string& address, const uint32_t& height, const vnx::request_id_t& request_id) const override;

	void handle(std::shared_ptr<const Block> value);

	void handle(std::shared_ptr<const Transaction> value);

	void handle(std::shared_ptr<const ProofOfTime> value);

	void handle(std::shared_ptr<const ProofResponse> value);

private:
	struct peer_t {
		bool is_synced = false;
		bool is_blocked = false;
		bool is_outbound = false;
		uint32_t height = 0;
		uint32_t msg_size = 0;
		uint32_t credits = 0;
		uint32_t tx_credits = 0;
		int32_t ping_ms = 0;
		int64_t last_receive_ms = 0;
		int64_t connected_since_ms = 0;
		uint64_t client = 0;
		uint64_t bytes_send = 0;
		uint64_t bytes_recv = 0;
		hash_t challenge;
		node_info_t info;
		std::string address;
		vnx::optional<hash_t> node_id;
		std::queue<std::shared_ptr<const Transaction>> tx_queue;

		vnx::Memory data;
		vnx::Buffer buffer;
		vnx::BufferInputStream in_stream;
		vnx::MemoryOutputStream out_stream;
		vnx::TypeInput in;
		vnx::TypeOutput out;
		peer_t() : in_stream(&buffer), out_stream(&data), in(&in_stream), out(&out_stream) {}
	};

	struct hash_info_t {
		std::vector<uint64_t> received_from;
		bool is_valid = false;
		bool is_rewarded = false;
		bool did_relay = false;
	};

	enum sync_state_e {
		FETCH_HASHES,
		FETCH_BLOCKS
	};

	struct sync_job_t {
		uint32_t height = 0;
		sync_state_e state = FETCH_HASHES;
		int64_t start_time_ms = 0;
		std::unordered_set<uint64_t> failed;
		std::unordered_set<uint64_t> pending;
		std::unordered_set<uint64_t> succeeded;
		std::unordered_map<uint32_t, uint64_t> request_map;				// [request id, client]
		std::unordered_map<hash_t, std::set<uint64_t>> hash_map;		// [block hash => clients who have it]
		std::unordered_map<hash_t, std::shared_ptr<const Block>> blocks;
	};

	struct fetch_job_t {
		uint32_t height = 0;
		std::string from_peer;
		vnx::optional<hash_t> hash;
		int64_t start_time_ms = 0;
		std::unordered_map<uint32_t, uint64_t> request_map;				// [request id, client]
	};

	void update();

	bool process(std::shared_ptr<const Return> ret = nullptr);

	void connect();

	void query();

	void save_data();

	void add_peer(const std::string& address, const int sock);

	void connect_task(const std::string& peer) noexcept;

	void print_stats() override;

	uint32_t send_request(std::shared_ptr<peer_t> peer, std::shared_ptr<const vnx::Value> method, bool reliable = true);

	uint32_t send_request(uint64_t client, std::shared_ptr<const vnx::Value> method, bool reliable = true);

	void on_vdf(uint64_t client, std::shared_ptr<const ProofOfTime> proof);

	void on_block(uint64_t client, std::shared_ptr<const Block> block);

	void on_proof(uint64_t client, std::shared_ptr<const ProofResponse> response);

	void on_transaction(uint64_t client, std::shared_ptr<const Transaction> tx);

	void relay(uint64_t source, std::shared_ptr<const vnx::Value> msg, const std::set<node_type_e>& filter);

	void send_to(uint64_t client, std::shared_ptr<const vnx::Value> msg, bool reliable = true);

	void send_to(std::shared_ptr<peer_t> peer, std::shared_ptr<const vnx::Value> msg, bool reliable = true);

	void send_all(std::shared_ptr<const vnx::Value> msg, const std::set<node_type_e>& filter, bool reliable = true);

	template<typename R, typename T>
	void send_result(uint64_t client, uint32_t id, const T& value);

	void on_error(uint64_t client, uint32_t id, const vnx::exception& ex);

	void on_request(uint64_t client, std::shared_ptr<const Request> msg);

	void on_return(uint64_t client, std::shared_ptr<const Return> msg);

	void on_msg(uint64_t client, std::shared_ptr<const vnx::Value> msg);

	void on_buffer(uint64_t client, void*& buffer, size_t& max_bytes) override;

	bool on_read(uint64_t client, size_t num_bytes) override;

	void on_pause(uint64_t client) override;

	void on_resume(uint64_t client) override;

	void on_connect(uint64_t client, const std::string& address) override;

	void on_disconnect(uint64_t client) override;

	std::shared_ptr<peer_t> get_peer(uint64_t client);

	std::shared_ptr<peer_t> find_peer(uint64_t client);

	bool relay_msg_hash(const hash_t& hash, uint32_t credits = 0);

	bool receive_msg_hash(const hash_t& hash, uint64_t client, uint32_t credits = 0);

private:
	bool is_connected = false;

	hash_t node_id;
	skey_t node_sk;
	pubkey_t node_key;
	std::set<std::string> peer_set;
	std::set<std::string> self_addrs;
	std::set<std::string> connecting_peers;

	std::set<uint64_t> synced_peers;
	std::unordered_map<uint64_t, std::shared_ptr<peer_t>> peer_map;

	std::queue<hash_t> hash_queue;
	std::unordered_map<hash_t, hash_info_t> hash_info;

	std::map<hash_t, uint32_t> farmer_credits;

	mutable std::unordered_map<vnx::request_id_t, sync_job_t> sync_jobs;
	mutable std::unordered_map<vnx::request_id_t, fetch_job_t> fetch_jobs;

	struct {
		uint32_t height = -1;
		hash_t our_hash;
		std::unordered_map<hash_t, size_t> hash_count;
		std::unordered_map<uint32_t, uint64_t> request_map;
	} fork_check;

	vnx::ThreadPool* threads = nullptr;
	std::shared_ptr<NodeAsyncClient> node;
	std::shared_ptr<const ChainParams> params;

	uint32_t next_request_id = 0;
	uint32_t verified_vdf_height = 0;
	int64_t last_query_ms = 0;

	size_t tx_counter = 0;
	size_t vdf_counter = 0;
	size_t block_counter = 0;
	size_t proof_counter = 0;
	size_t upload_counter = 0;

	size_t drop_counter = 0;
	size_t tx_drop_counter = 0;
	size_t vdf_drop_counter = 0;
	size_t proof_drop_counter = 0;
	size_t block_drop_counter = 0;

};


} // mmx

#endif /* INCLUDE_MMX_ROUTER_H_ */
