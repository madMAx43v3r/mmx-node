/*
 * Node.h
 *
 *  Created on: Dec 7, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_NODE_H_
#define INCLUDE_MMX_NODE_H_

#include <mmx/NodeBase.hxx>
#include <mmx/Block.hxx>
#include <mmx/ChainParams.hxx>
#include <mmx/RouterAsyncClient.hxx>
#include <mmx/TimeLordAsyncClient.hxx>
#include <mmx/operation/Mutate.hxx>
#include <mmx/txio_key_t.hpp>
#include <mmx/txio_entry_t.hpp>
#include <mmx/txout_entry_t.hpp>
#include <mmx/OCL_VDF.h>
#include <mmx/utils.h>

#include <vnx/ThreadPool.h>
#include <vnx/rocksdb/table.h>
#include <vnx/rocksdb/multi_table.h>
#include <vnx/addons/HttpInterface.h>

#include <uint128_t.h>
#include <unordered_set>


namespace mmx {

class Node : public NodeBase {
public:
	Node(const std::string& _vnx_value);

protected:
	void init() override;

	void main() override;

	std::shared_ptr<const ChainParams> get_params() const override;

	std::shared_ptr<const NetworkInfo> get_network_info() const override;

	uint32_t get_height() const override;

	vnx::optional<uint32_t> get_synced_height() const override;

	std::shared_ptr<const Block> get_block(const hash_t& hash) const override;

	std::shared_ptr<const Block> get_block_at(const uint32_t& height) const override;

	std::shared_ptr<const BlockHeader> get_header(const hash_t& hash) const override;

	std::shared_ptr<const BlockHeader> get_header_at(const uint32_t& height) const override;

	vnx::optional<hash_t> get_block_hash(const uint32_t& height) const override;

	vnx::optional<uint32_t> get_tx_height(const hash_t& id) const override;

	std::vector<hash_t> get_tx_ids_at(const uint32_t& height) const override;

	vnx::optional<tx_info_t> get_tx_info(const hash_t& id) const override;

	std::shared_ptr<const Transaction> get_transaction(const hash_t& id, const bool& include_pending = false) const override;

	std::vector<std::shared_ptr<const Transaction>> get_transactions(const std::vector<hash_t>& ids) const override;

	std::vector<tx_entry_t> get_history(const std::vector<addr_t>& addresses, const int32_t& since) const override;

	std::shared_ptr<const Contract> get_contract(const addr_t& address) const override;

	std::shared_ptr<const Contract> get_contract_for(const addr_t& address) const override;

	std::vector<std::shared_ptr<const Contract>> get_contracts(const std::vector<addr_t>& addresses) const override;

	std::map<addr_t, std::shared_ptr<const Contract>> get_contracts_owned(const std::vector<addr_t>& owners) const override;

	void add_block(std::shared_ptr<const Block> block) override;

	void add_transaction(std::shared_ptr<const Transaction> tx, const vnx::bool_t& pre_validate = false) override;

	uint128 get_balance(const addr_t& address, const addr_t& currency, const uint32_t& min_confirm) const override;

	uint128 get_total_balance(const std::vector<addr_t>& addresses, const addr_t& currency, const uint32_t& min_confirm) const override;

	std::map<addr_t, uint128> get_balances(const addr_t& address, const uint32_t& min_confirm) const override;

	std::map<addr_t, uint128> get_total_balances(const std::vector<addr_t>& addresses, const uint32_t& min_confirm) const override;

	std::map<std::pair<addr_t, addr_t>, uint128> get_all_balances(const std::vector<addr_t>& addresses, const uint32_t& min_confirm) const override;

	uint128 get_total_supply(const addr_t& currency) const override;

	uint128 get_virtual_plot_balance(const addr_t& plot_id, const vnx::optional<hash_t>& block_hash) const override;

	void on_stuck_timeout();

	void start_sync(const vnx::bool_t& force) override;

	void http_request_async(std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
							const vnx::request_id_t& request_id) const override;

	void http_request_chunk_async(	std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
									const int64_t& offset, const int64_t& max_bytes, const vnx::request_id_t& request_id) const override;

	void handle(std::shared_ptr<const Block> block) override;

	void handle(std::shared_ptr<const Transaction> tx) override;

	void handle(std::shared_ptr<const ProofOfTime> proof) override;

	void handle(std::shared_ptr<const ProofResponse> value) override;

private:
	struct vdf_point_t {
		uint32_t height = -1;
		uint64_t vdf_start = 0;
		uint64_t vdf_iters = 0;
		int64_t recv_time = 0;
		std::array<hash_t, 2> input;
		std::array<hash_t, 2> output;
		vnx::optional<hash_t> infused;
		std::shared_ptr<const ProofOfTime> proof;
	};

	struct fork_t {
		bool is_invalid = false;
		bool is_verified = false;
		bool is_vdf_verified = false;
		bool is_proof_verified = false;
		int64_t recv_time = 0;
		std::weak_ptr<fork_t> prev;
		std::shared_ptr<const Block> block;
		std::shared_ptr<const vdf_point_t> vdf_point;
		std::shared_ptr<const BlockHeader> diff_block;
	};

	struct tx_map_t {
		uint32_t height = -1;
		std::shared_ptr<const Transaction> tx;
	};

	struct tx_pool_t {
		bool did_validate = false;
		std::shared_ptr<const Transaction> tx;
	};

	struct tx_data_t : tx_pool_t {
		bool invalid = false;
		uint64_t fees = 0;
		uint64_t cost = 0;
	};

	void update();

	void check_vdfs();

	void print_stats();

	bool recv_height(const uint32_t& height);

	void add_fork(std::shared_ptr<fork_t> fork);

	void add_dummy_blocks(std::shared_ptr<const BlockHeader> prev);

	void validate_pool();

	std::vector<tx_data_t> validate_pending(const uint64_t verify_limit, const uint64_t select_limit, bool only_new);

	std::shared_ptr<const Block> make_block(std::shared_ptr<const BlockHeader> prev, std::shared_ptr<const ProofResponse> response);

	void sync_more();

	void sync_result(const uint32_t& height, const std::vector<std::shared_ptr<const Block>>& blocks);

	void fetch_block(const hash_t& hash);

	void fetch_result(const hash_t& hash, std::shared_ptr<const Block> block);

	std::shared_ptr<const BlockHeader> fork_to(const hash_t& state);

	std::shared_ptr<const BlockHeader> fork_to(std::shared_ptr<fork_t> fork_head);

	std::shared_ptr<fork_t> find_best_fork() const;

	std::vector<std::shared_ptr<fork_t>> get_fork_line(std::shared_ptr<fork_t> fork_head = nullptr) const;

	std::unordered_set<addr_t> get_revokations(const hash_t& txid) const;

	void validate(std::shared_ptr<const Block> block) const;

	void validate(std::shared_ptr<const Transaction> tx) const;

	std::shared_ptr<const Context> create_exec_context(
			std::shared_ptr<const Context> base, std::shared_ptr<const Contract> contract, std::shared_ptr<const Transaction> tx) const;

	void validate(	std::shared_ptr<const Transaction> tx,
					std::shared_ptr<const Context> context,
					std::vector<txout_t>& outputs,
					std::vector<txout_t>& exec_outputs,
					balance_cache_t& balance_cache,
					std::unordered_map<addr_t, uint128_t>& amounts,
					std::unordered_map<addr_t, std::shared_ptr<Contract>>& contract_state) const;

	std::shared_ptr<const Transaction>
	validate(	std::shared_ptr<const Transaction> tx, std::shared_ptr<const Context> context,
				std::shared_ptr<const Block> base, uint64_t& fee_amount) const;

	void validate_diff_adjust(const uint64_t& block, const uint64_t& prev) const;

	void commit(std::shared_ptr<const Block> block) noexcept;

	void purge_tree();

	void verify_proof(std::shared_ptr<fork_t> fork, const hash_t& vdf_output) const;

	void verify_proof(std::shared_ptr<const ProofOfSpace> proof, const hash_t& challenge, std::shared_ptr<const BlockHeader> diff_block) const;

	void verify_vdf(std::shared_ptr<const ProofOfTime> proof) const;

	void verify_vdf(std::shared_ptr<const ProofOfTime> proof, const uint32_t chain) const;

	void verify_vdf_success(std::shared_ptr<const ProofOfTime> proof, std::shared_ptr<vdf_point_t> point);

	void verify_vdf_failed(std::shared_ptr<const ProofOfTime> proof);

	void verify_vdf_task(std::shared_ptr<const ProofOfTime> proof) const noexcept;

	void check_vdf_task(std::shared_ptr<fork_t> fork, std::shared_ptr<const BlockHeader> prev, std::shared_ptr<const BlockHeader> infuse) const noexcept;

	void apply(std::shared_ptr<const Block> block, int64_t* file_offset = nullptr) noexcept;

	void apply(	std::shared_ptr<const Block> block,
				std::shared_ptr<const Transaction> tx,
				std::unordered_set<addr_t>& addr_set,
				std::unordered_set<hash_t>& revoke_set,
				std::set<std::pair<addr_t, addr_t>>& balance_set) noexcept;

	bool revert() noexcept;

	void revert(const uint32_t height, std::shared_ptr<const Block> block) noexcept;

	std::shared_ptr<const BlockHeader> get_root() const;

	std::shared_ptr<const BlockHeader> get_peak() const;

	std::shared_ptr<const BlockHeader> get_block_ex(const hash_t& hash, bool full_block) const;

	std::shared_ptr<const BlockHeader> get_block_at_ex(const uint32_t& height, bool full_block) const;

	std::shared_ptr<fork_t> find_fork(const hash_t& hash) const;

	std::shared_ptr<fork_t> find_prev_fork(std::shared_ptr<fork_t> fork, const size_t distance = 1) const;

	std::shared_ptr<const BlockHeader> find_prev_header(
			std::shared_ptr<const BlockHeader> block, const size_t distance = 1, bool clamped = false) const;

	std::shared_ptr<const BlockHeader> find_diff_header(std::shared_ptr<const BlockHeader> block, uint32_t offset = 0) const;

	hash_t get_challenge(std::shared_ptr<const BlockHeader> block, const hash_t& vdf_challenge, uint32_t offset = 0) const;

	bool find_vdf_challenge(std::shared_ptr<const BlockHeader> block, hash_t& vdf_challenge, uint32_t offset = 0) const;

	std::shared_ptr<Node::vdf_point_t> find_vdf_point(const uint32_t height, const uint64_t vdf_start, const uint64_t vdf_iters,
			const std::array<hash_t, 2>& input, const std::array<hash_t, 2>& output) const;

	std::shared_ptr<Node::vdf_point_t> find_next_vdf_point(std::shared_ptr<const BlockHeader> block) const;

	std::vector<std::shared_ptr<const ProofResponse>> find_proof(const hash_t& challenge) const;

	uint64_t calc_block_reward(std::shared_ptr<const BlockHeader> block) const;

	std::shared_ptr<const BlockHeader> read_block(vnx::File& file, bool full_block = true, int64_t* file_offset = nullptr) const;

	void write_block(std::shared_ptr<const Block> block, int64_t* file_offset = nullptr);

private:
	hash_t state_hash;

	vnx::rocksdb::multi_table<uint32_t, addr_t> addr_log;								// [height => address]
	vnx::rocksdb::multi_table<std::pair<addr_t, uint32_t>, txout_entry_t> recv_log;		// [[address, height] => entry]
	vnx::rocksdb::multi_table<std::pair<addr_t, uint32_t>, txio_entry_t> spend_log;		// [[address, height] => entry]

	vnx::rocksdb::multi_table<uint32_t, hash_t> revoke_log;								// [height => txid]
	vnx::rocksdb::multi_table<std::pair<hash_t, uint32_t>, std::pair<addr_t, hash_t>> revoke_map;	// [[org txid, height]] => [address, txid]]

	vnx::rocksdb::table<addr_t, std::shared_ptr<const Contract>> contract_cache;		// [addr, contract] (finalized only)
	vnx::rocksdb::multi_table<std::pair<addr_t, uint32_t>, vnx::Object> mutate_log;		// [[addr, height] => method] (finalized only)
	vnx::rocksdb::multi_table<addr_t, addr_t> owner_map;								// [owner => contract]

	std::map<std::pair<addr_t, addr_t>, uint128_t> balance_map;						// [[addr, currency] => balance]
	vnx::rocksdb::table<std::pair<addr_t, addr_t>, std::pair<uint128, uint32_t>> balance_table;		// [[addr, currency] => [balance, height]]

	std::unordered_map<hash_t, tx_pool_t> tx_pool;									// [txid => transaction] (non-executed only)
	std::unordered_map<hash_t, std::shared_ptr<fork_t>> fork_tree;					// [block hash => fork] (pending only)
	std::multimap<uint32_t, std::shared_ptr<fork_t>> fork_index;					// [height => fork] (pending only)
	std::map<uint32_t, std::shared_ptr<const BlockHeader>> history;					// [height => block header] (finalized only)

	std::multimap<uint32_t, std::shared_ptr<vdf_point_t>> verified_vdfs;			// [height => output]
	std::multimap<uint32_t, std::shared_ptr<const ProofOfTime>> pending_vdfs;		// [height => proof]

	std::unordered_multimap<uint32_t, hash_t> challenge_map;								// [height => challenge]
	std::unordered_multimap<hash_t, std::shared_ptr<const ProofResponse>> proof_map;		// [challenge => proof]
	std::map<std::pair<hash_t, hash_t>, hash_t> created_blocks;								// [[prev hash, proof hash] => hash]

	bool is_synced = false;
	std::shared_ptr<vnx::File> block_chain;
	mutable vnx::rocksdb::table<hash_t, uint32_t> hash_index;							// [block hash => height]
	mutable vnx::rocksdb::table<hash_t, std::pair<int64_t, uint32_t>> tx_index;			// [txid => [file offset, height]] (finalized only)
	mutable vnx::rocksdb::table<uint32_t, std::pair<int64_t, hash_t>> block_index;		// [height => [file offset, block hash]]
	mutable vnx::rocksdb::multi_table<uint32_t, hash_t> tx_log;							// [height => txid] (finalized only)

	uint32_t sync_pos = 0;									// current sync height
	uint32_t sync_retry = 0;
	double max_sync_pending = 0;
	std::set<uint32_t> sync_pending;						// set of heights
	vnx::optional<uint32_t> sync_peak;						// max height we can sync
	std::unordered_set<hash_t> fetch_pending;				// block hash

	std::shared_ptr<vnx::Timer> stuck_timer;
	std::shared_ptr<vnx::Timer> update_timer;

	std::shared_ptr<const ChainParams> params;
	mutable std::shared_ptr<const NetworkInfo> network;
	std::shared_ptr<RouterAsyncClient> router;
	std::shared_ptr<TimeLordAsyncClient> timelord;
	std::shared_ptr<vnx::addons::HttpInterface<Node>> http;

	mutable std::mutex vdf_mutex;
	std::unordered_set<uint32_t> vdf_verify_pending;		// height
	std::shared_ptr<OCL_VDF> opencl_vdf[2];
	std::shared_ptr<vnx::ThreadPool> vdf_threads;

	friend class vnx::addons::HttpInterface<Node>;

};


} // mmx

#endif /* INCLUDE_MMX_NODE_H_ */
