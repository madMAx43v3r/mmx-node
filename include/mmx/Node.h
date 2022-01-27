/*
 * Node.h
 *
 *  Created on: Dec 7, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_NODE_H_
#define INCLUDE_MMX_NODE_H_

#include <mmx/NodeBase.hxx>
#include <mmx/ChainParams.hxx>
#include <mmx/RouterAsyncClient.hxx>
#include <mmx/utxo_t.hpp>
#include <mmx/stxo_t.hpp>
#include <mmx/txio_key_t.hpp>
#include <mmx/OCL_VDF.h>

#include <vnx/ThreadPool.h>
#include <vnx/rocksdb/table.h>
#include <vnx/rocksdb/multi_table.h>
#include <vnx/addons/HttpInterface.h>

#include <shared_mutex>


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

	vnx::optional<txo_info_t> get_txo_info(const txio_key_t& key) const override;

	std::vector<vnx::optional<txo_info_t>> get_txo_infos(const std::vector<txio_key_t>& keys) const override;

	std::shared_ptr<const Transaction> get_transaction(const hash_t& id, const vnx::bool_t& include_pending = false) const override;

	std::vector<std::shared_ptr<const Transaction>> get_transactions(const std::vector<hash_t>& ids) const override;

	std::vector<tx_entry_t> get_history_for(const std::vector<addr_t>& addresses, const int32_t& since) const override;

	std::shared_ptr<const Contract> get_contract(const addr_t& address) const override;

	std::vector<std::shared_ptr<const Contract>> get_contracts(const std::vector<addr_t>& addresses) const override;

	std::map<addr_t, std::shared_ptr<const Contract>> get_contracts_owned(const std::vector<addr_t>& owners) const override;

	void add_block(std::shared_ptr<const Block> block) override;

	void add_transaction(std::shared_ptr<const Transaction> tx, const vnx::bool_t& pre_validate = false) override;

	uint64_t get_balance(const addr_t& address, const addr_t& contract, const uint32_t& min_confirm) const override;

	uint64_t get_total_balance(const std::vector<addr_t>& addresses, const addr_t& contract, const uint32_t& min_confirm) const override;

	std::map<addr_t, uint64_t> get_total_balances(const std::vector<addr_t>& addresses, const uint32_t& min_confirm) const override;

	uint64_t get_total_supply(const addr_t& contract) const override;

	std::vector<utxo_entry_t> get_utxo_list(const std::vector<addr_t>& addresses, const uint32_t& min_confirm) const override;

	std::vector<stxo_entry_t> get_stxo_list(const std::vector<addr_t>& addresses) const override;

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
		bool is_finalized = false;
		bool is_vdf_verified = false;
		bool is_proof_verified = false;
		bool has_weak_proof = false;
		bool has_dummy_block = false;
		uint32_t proof_score = -1;
		int32_t buffer_delta = 0;
		int32_t weight_buffer = 0;
		int64_t recv_time = 0;
		uint128_t weight = 0;
		uint128_t total_weight = 0;
		std::weak_ptr<fork_t> prev;
		std::shared_ptr<const Block> block;
		std::shared_ptr<const vdf_point_t> vdf_point;
		std::shared_ptr<const BlockHeader> diff_block;
	};

	struct change_log_t {
		hash_t prev_state;
		vnx::optional<hash_t> tx_base;
		std::vector<hash_t> tx_added;
		std::unordered_map<txio_key_t, utxo_t> utxo_added;			// [utxo key => utxo]
		std::unordered_map<txio_key_t, stxo_t> utxo_removed;		// [utxo key => [txi key, utxo]]
		std::unordered_map<addr_t, std::shared_ptr<const Contract>> deployed;
	};

	void update();

	void check_vdfs();

	void print_stats();

	void add_fork(std::shared_ptr<fork_t> fork);

	bool add_dummy_block(std::shared_ptr<const BlockHeader> prev);

	bool include_transaction(std::shared_ptr<const Transaction> tx);

	bool make_block(std::shared_ptr<const BlockHeader> prev, std::shared_ptr<const ProofResponse> response);

	void sync_more();

	void sync_result(const uint32_t& height, const std::vector<std::shared_ptr<const Block>>& blocks);

	std::shared_ptr<const BlockHeader> fork_to(const hash_t& state);

	std::shared_ptr<const BlockHeader> fork_to(std::shared_ptr<fork_t> fork_head);

	std::shared_ptr<fork_t> find_best_fork(std::shared_ptr<const BlockHeader> root = nullptr, const uint32_t* at_height = nullptr) const;

	std::vector<std::shared_ptr<fork_t>> get_fork_line(std::shared_ptr<fork_t> fork_head = nullptr) const;

	void validate(std::shared_ptr<const Block> block) const;

	void validate(std::shared_ptr<const Transaction> tx) const;

	std::shared_ptr<const Context> create_context(
			const addr_t& address, std::shared_ptr<const Contract> contract,
			std::shared_ptr<const Context> base, std::shared_ptr<const Transaction> tx) const;

	std::shared_ptr<const Transaction> validate(std::shared_ptr<const Transaction> tx, std::shared_ptr<const Context> context,
												std::shared_ptr<const Block> base, uint64_t& fee_amount) const;

	void validate_diff_adjust(const uint64_t& block, const uint64_t& prev) const;

	void commit(std::shared_ptr<const Block> block) noexcept;

	void purge_tree();

	void verify_proof(std::shared_ptr<fork_t> fork, const hash_t& vdf_output) const;

	uint32_t verify_proof(std::shared_ptr<const ProofOfSpace> proof, const hash_t& challenge, const uint64_t space_diff) const;

	void verify_vdf(std::shared_ptr<const ProofOfTime> proof) const;

	void verify_vdf(std::shared_ptr<const ProofOfTime> proof, const uint32_t chain) const;

	void verify_vdf_success(std::shared_ptr<const ProofOfTime> proof, std::shared_ptr<vdf_point_t> point);

	void verify_vdf_failed(std::shared_ptr<const ProofOfTime> proof);

	void verify_vdf_task(std::shared_ptr<const ProofOfTime> proof) const noexcept;

	void check_vdf_task(std::shared_ptr<fork_t> fork, std::shared_ptr<const BlockHeader> prev, std::shared_ptr<const BlockHeader> infuse) const noexcept;

	void apply(std::shared_ptr<const Block> block) noexcept;

	void apply(std::shared_ptr<const Block> block, std::shared_ptr<const Transaction> tx, change_log_t& log) noexcept;

	void apply_output(	std::shared_ptr<const Block> block, std::shared_ptr<const Transaction> tx,
						const tx_out_t& output, const size_t index, change_log_t& log) noexcept;

	bool revert() noexcept;

	std::shared_ptr<const BlockHeader> get_root() const;

	std::shared_ptr<const BlockHeader> get_peak() const;

	std::shared_ptr<fork_t> find_fork(const hash_t& hash) const;

	std::shared_ptr<const Block> find_block(const hash_t& hash) const;

	std::shared_ptr<const BlockHeader> find_header(const hash_t& hash) const;

	std::shared_ptr<fork_t> find_prev_fork(std::shared_ptr<fork_t> fork, const size_t distance = 1) const;

	std::shared_ptr<const BlockHeader> find_prev_header(
			std::shared_ptr<const BlockHeader> block, const size_t distance = 1, bool clamped = false) const;

	std::shared_ptr<const BlockHeader> find_diff_header(std::shared_ptr<const BlockHeader> block, uint32_t offset = 0) const;

	hash_t get_challenge(std::shared_ptr<const BlockHeader> block, const hash_t& vdf_challenge, uint32_t offset = 0) const;

	bool find_vdf_challenge(std::shared_ptr<const BlockHeader> block, hash_t& vdf_challenge, uint32_t offset = 0) const;

	std::shared_ptr<Node::vdf_point_t> find_vdf_point(const uint32_t height, const uint64_t vdf_start, const uint64_t vdf_iters,
			const std::array<hash_t, 2>& input, const std::array<hash_t, 2>& output) const;

	std::shared_ptr<Node::vdf_point_t> find_next_vdf_point(std::shared_ptr<const BlockHeader> block);

	uint64_t calc_block_reward(std::shared_ptr<const BlockHeader> block) const;

	std::shared_ptr<const Block> read_block(bool is_replay = false, int64_t* file_offset = nullptr);

	void write_block(std::shared_ptr<const Block> block);

private:
	hash_t state_hash;
	std::list<std::shared_ptr<const change_log_t>> change_log;

	std::unordered_map<hash_t, uint32_t> hash_index;								// [block hash => height] (finalized only)
	vnx::rocksdb::table<txio_key_t, stxo_t> stxo_index;								// [stxo key => [txi key, stxo]] (finalized + spent only)
	vnx::rocksdb::multi_table<addr_t, txio_key_t> saddr_map;						// [addr => stxo key] (finalized + spent only)
	vnx::rocksdb::multi_table<uint32_t, txio_key_t> stxo_log;						// [height => stxo key] (finalized + spent only)

	vnx::rocksdb::table<hash_t, std::pair<int64_t, uint32_t>> tx_index;				// [txid => [file offset, height]] (finalized only)
	vnx::rocksdb::multi_table<addr_t, addr_t> owner_map;							// [owner => contract]
	vnx::rocksdb::multi_table<uint32_t, hash_t> tx_log;								// [height => txid] (finalized only)

	std::unordered_map<hash_t, uint32_t> tx_map;									// [txid => height] (pending only)
	std::unordered_map<txio_key_t, utxo_t> utxo_map;								// [utxo key => utxo]
	std::set<std::pair<addr_t, txio_key_t>> addr_map;								// [addr => utxo keys] (finalized + unspent only)
	std::unordered_map<addr_t, std::unordered_set<txio_key_t>> taddr_map;			// [addr => utxo keys] (pending + unspent only)

	std::multimap<uint32_t, std::shared_ptr<fork_t>> fork_index;					// [height => fork]
	std::unordered_map<hash_t, std::shared_ptr<fork_t>> fork_tree;					// [block hash => fork] (pending only)
	std::map<uint32_t, std::shared_ptr<const BlockHeader>> history;					// [height => block header] (finalized only)
	std::unordered_map<hash_t, std::shared_ptr<const Transaction>> tx_pool;			// [txid => transaction] (pending only)

	std::multimap<uint32_t, std::shared_ptr<vdf_point_t>> verified_vdfs;			// [height => output]
	std::multimap<uint32_t, std::shared_ptr<const ProofOfTime>> pending_vdfs;		// [height => proof]

	std::unordered_multimap<uint32_t, hash_t> challenge_map;						// [height => challenge]
	std::unordered_map<hash_t, std::shared_ptr<const ProofResponse>> proof_map;		// [challenge => proof]

	std::unordered_set<addr_t> light_address_set;									// addresses for light mode

	bool is_replay = true;
	bool is_synced = false;
	std::shared_ptr<vnx::File> block_chain;
	std::unordered_map<uint32_t, std::pair<int64_t, hash_t>> block_index;			// [height => [file offset, block hash]]

	mutable std::shared_mutex cache_mutex;
	mutable std::queue<addr_t> contract_cache_queue;
	mutable std::unordered_map<addr_t, std::shared_ptr<const Contract>> contract_map;		// [addr => contract] (cached only)

	uint32_t sync_pos = 0;									// current sync height
	uint32_t sync_retry = 0;
	std::set<uint32_t> sync_pending;						// set of heights
	vnx::optional<uint32_t> sync_peak;						// max height we can sync

	std::shared_ptr<vnx::Timer> stuck_timer;
	std::shared_ptr<vnx::Timer> update_timer;

	std::shared_ptr<const ChainParams> params;
	mutable std::shared_ptr<const NetworkInfo> network;
	std::shared_ptr<RouterAsyncClient> router;
	std::shared_ptr<vnx::addons::HttpInterface<Node>> http;

	mutable std::mutex vdf_mutex;
	std::unordered_set<uint32_t> vdf_verify_pending;		// height
	std::shared_ptr<OCL_VDF> opencl_vdf[2];
	std::shared_ptr<vnx::ThreadPool> vdf_threads;

	friend class vnx::addons::HttpInterface<Node>;

};


} // mmx

#endif /* INCLUDE_MMX_NODE_H_ */
