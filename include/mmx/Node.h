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
#include <mmx/contract/Binary.hxx>
#include <mmx/operation/Execute.hxx>
#include <mmx/txio_entry_t.hpp>
#include <mmx/OCL_VDF.h>
#include <mmx/utils.h>
#include <mmx/uint128.hpp>
#include <mmx/balance_cache_t.h>
#include <mmx/table.h>
#include <mmx/multi_table.h>

#include <mmx/vm/Engine.h>
#include <mmx/vm/StorageDB.h>
#include <mmx/vm/StorageCache.h>

#include <vnx/ThreadPool.h>
#include <vnx/addons/HttpInterface.h>

#include <unordered_set>


namespace mmx {

class Node : public NodeBase {
public:
	Node(const std::string& _vnx_value);

	bool do_restart = false;

protected:
	void init() override;

	void main() override;

	std::shared_ptr<const ChainParams> get_params() const override;

	std::shared_ptr<const NetworkInfo> get_network_info() const override;

	hash_t get_genesis_hash() const override;

	uint32_t get_height() const override;

	vnx::optional<uint32_t> get_synced_height() const override;

	std::shared_ptr<const Block> get_block(const hash_t& hash) const override;

	std::shared_ptr<const Block> get_block_at(const uint32_t& height) const override;

	std::shared_ptr<const BlockHeader> get_header(const hash_t& hash) const override;

	std::shared_ptr<const BlockHeader> get_header_at(const uint32_t& height) const override;

	vnx::optional<hash_t> get_block_hash(const uint32_t& height) const override;

	vnx::optional<uint32_t> get_tx_height(const hash_t& id) const override;

	std::vector<hash_t> get_tx_ids_at(const uint32_t& height) const override;

	std::vector<hash_t> get_tx_ids_since(const uint32_t& height) const override;

	vnx::optional<tx_info_t> get_tx_info(const hash_t& id) const override;

	vnx::optional<tx_info_t> get_tx_info_for(std::shared_ptr<const Transaction> tx) const;

	std::shared_ptr<const Transaction> get_transaction(const hash_t& id, const bool& include_pending = false) const override;

	std::vector<std::shared_ptr<const Transaction>> get_transactions(const std::vector<hash_t>& ids) const override;

	std::vector<tx_entry_t> get_history(const std::vector<addr_t>& addresses, const int32_t& since) const override;

	std::shared_ptr<const Contract> get_contract(const addr_t& address) const override;

	std::shared_ptr<const Contract> get_contract_for(const addr_t& address) const override;

	std::vector<std::shared_ptr<const Contract>> get_contracts(const std::vector<addr_t>& addresses) const override;

	std::map<addr_t, std::shared_ptr<const Contract>> get_contracts_by(const std::vector<addr_t>& addresses) const override;

	std::shared_ptr<const Contract> get_contract_at(const addr_t& address, const hash_t& block_hash) const override;

	void add_block(std::shared_ptr<const Block> block) override;

	void add_transaction(std::shared_ptr<const Transaction> tx, const vnx::bool_t& pre_validate = false) override;

	uint128 get_balance(const addr_t& address, const addr_t& currency, const uint32_t& min_confirm = 1) const override;

	uint128 get_total_balance(const std::vector<addr_t>& addresses, const addr_t& currency, const uint32_t& min_confirm = 1) const override;

	std::map<addr_t, uint128> get_balances(const addr_t& address, const uint32_t& min_confirm = 1) const override;

	std::map<addr_t, balance_t> get_contract_balances(const addr_t& address, const uint32_t& min_confirm) const;

	std::map<addr_t, uint128> get_total_balances(const std::vector<addr_t>& addresses, const uint32_t& min_confirm = 1) const override;

	std::map<std::pair<addr_t, addr_t>, uint128> get_all_balances(const std::vector<addr_t>& addresses, const uint32_t& min_confirm = 1) const override;

	std::vector<exec_entry_t> get_exec_history(const addr_t& address, const int32_t& since) const override;

	std::map<std::string, vm::varptr_t> read_storage(const addr_t& contract, const uint32_t& height = -1) const override;

	std::map<uint64_t, vm::varptr_t> dump_storage(const addr_t& contract, const uint32_t& height = -1) const override;

	vm::varptr_t read_storage_var(const addr_t& contract, const uint64_t& address, const uint32_t& height = -1) const override;

	std::pair<vm::varptr_t, uint64_t> read_storage_field(const addr_t& contract, const std::string& name, const uint32_t& height = -1) const override;

	std::vector<vm::varptr_t> read_storage_array(const addr_t& contract, const uint64_t& address, const uint32_t& height = -1) const override;

	std::map<vm::varptr_t, vm::varptr_t> read_storage_map(const addr_t& contract, const uint64_t& address, const uint32_t& height = -1) const override;

	vnx::Variant call_contract(const addr_t& address, const std::string& method, const std::vector<vnx::Variant>& args) const override;

	address_info_t get_address_info(const addr_t& address) const override;

	uint128 get_total_supply(const addr_t& currency) const override;

	std::vector<std::pair<addr_t, std::shared_ptr<const Contract>>> get_virtual_plots_for(const bls_pubkey_t& farmer_key) const;

	uint64_t get_virtual_plot_balance(const addr_t& plot_id, const vnx::optional<hash_t>& block_hash) const override;

	offer_data_t get_offer(const addr_t& address) const override;

	std::vector<offer_data_t> get_offers(const uint32_t& since, const vnx::bool_t& is_open) const override;

	std::vector<offer_data_t> get_offers_for(
			const vnx::optional<addr_t>& bid, const vnx::optional<addr_t>& ask, const uint32_t& since, const vnx::bool_t& is_open) const override;

	std::vector<trade_data_t> get_trade_history(const int32_t& since) const override;

	std::vector<trade_data_t> get_trade_history_for(
			const vnx::optional<addr_t>& bid, const vnx::optional<addr_t>& ask, const int32_t& since) const override;

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
	struct waitcond_t {
		std::mutex mutex;
		std::condition_variable signal;
		bool do_wait = true;
	};

	struct contract_state_t {
		bool is_mutated = false;
		std::map<addr_t, uint128> balance;
		std::shared_ptr<const Contract> data;
	};

	struct contract_cache_t {
		contract_cache_t() = default;
		contract_cache_t(const contract_cache_t* parent) : parent(parent) {}
		mutable std::mutex mutex;
		const contract_cache_t* parent = nullptr;
		std::unordered_map<addr_t, std::shared_ptr<contract_state_t>> state_map;
		std::shared_ptr<contract_state_t> get_state(const addr_t& address);
		std::shared_ptr<contract_state_t> find_state(const addr_t& address) const;
		std::shared_ptr<const Contract> find_contract(const addr_t& address) const;
		void commit(const contract_cache_t& cache);
	};

	struct execution_context_t {
		std::shared_ptr<const Context> block;
		std::shared_ptr<vm::StorageCache> storage;
		std::unordered_map<addr_t, std::vector<hash_t>> mutate_map;
		std::unordered_map<hash_t, std::unordered_set<hash_t>> wait_map;
		std::unordered_map<hash_t, std::shared_ptr<waitcond_t>> signal_map;
		mutable contract_cache_t contract_cache;
		void wait(const hash_t& txid) const;
		void signal(const hash_t& txid) const;
		void setup_wait(const hash_t& txid, const addr_t& address);
		std::shared_ptr<const Context> get_context_for(
				std::shared_ptr<const Transaction> tx, std::shared_ptr<const Contract> contract, const contract_cache_t& cache) const;
	};

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
		bool is_connected = false;
		bool is_vdf_verified = false;
		bool is_proof_verified = false;
		bool is_all_proof_verified = false;
		int64_t recv_time = 0;
		std::weak_ptr<fork_t> prev;
		std::shared_ptr<const Block> block;
		std::shared_ptr<const vdf_point_t> vdf_point;
		std::shared_ptr<const BlockHeader> diff_block;
		std::shared_ptr<const execution_context_t> context;
	};

	struct tx_map_t {
		uint32_t height = -1;
		std::shared_ptr<const Transaction> tx;
	};

	struct tx_pool_t {
		bool is_valid = false;
		uint32_t cost = 0;
		uint32_t fee = 0;
		std::shared_ptr<const Transaction> tx;
	};

	struct balance_log_t {
		std::map<std::pair<addr_t, addr_t>, uint128> added;
		std::map<std::pair<addr_t, addr_t>, uint128> removed;
	};

	struct proof_data_t {
		uint32_t height = 0;
		vnx::Hash64 farmer_mac;
		std::shared_ptr<const ProofOfSpace> proof;
	};

	void update();

	void verify_vdfs();

	void print_stats();

	bool recv_height(const uint32_t& height);

	void add_fork(std::shared_ptr<fork_t> fork);

	void add_dummy_blocks(std::shared_ptr<const BlockHeader> prev);

	bool tx_pool_update(const tx_pool_t& entry, const bool force_add = false);

	void tx_pool_erase(const hash_t& txid);

	void purge_tx_pool();

	void validate_new();

	std::vector<tx_pool_t> validate_for_block(const uint64_t verify_limit, const uint64_t select_limit);

	std::shared_ptr<const Block> make_block(std::shared_ptr<const BlockHeader> prev, const proof_data_t& proof_data);

	std::vector<trade_data_t> get_trade_history_for(const std::vector<addr_t>& offers) const;

	std::pair<vm::varptr_t, uint64_t> read_storage_field(const addr_t& binary, const addr_t& contract, const std::string& name, const uint32_t& height = -1) const;

	void sync_more();

	void sync_result(const uint32_t& height, const std::vector<std::shared_ptr<const Block>>& blocks);

	void fetch_block(const hash_t& hash);

	void fetch_result(const hash_t& hash, std::shared_ptr<const Block> block);

	std::shared_ptr<const BlockHeader> fork_to(const hash_t& state);

	std::shared_ptr<const BlockHeader> fork_to(std::shared_ptr<fork_t> fork_head);

	std::shared_ptr<fork_t> find_best_fork(const uint32_t height = -1) const;

	std::vector<std::shared_ptr<fork_t>> get_fork_line(std::shared_ptr<fork_t> fork_head = nullptr) const;

	std::shared_ptr<execution_context_t> validate(std::shared_ptr<const Block> block) const;

	std::shared_ptr<const exec_result_t> validate(std::shared_ptr<const Transaction> tx) const;

	std::shared_ptr<execution_context_t> new_exec_context() const;

	std::shared_ptr<contract_state_t> get_contract_state(contract_cache_t& contract_cache, const addr_t& address) const;

	void setup_context_wait(std::shared_ptr<execution_context_t> context, const hash_t& txid, const addr_t& address) const;

	void prepare_context(std::shared_ptr<execution_context_t> context, std::shared_ptr<const Transaction> tx) const;

	void execute(	std::shared_ptr<const Transaction> tx,
					std::shared_ptr<const execution_context_t> context,
					std::shared_ptr<const operation::Execute> exec,
					std::shared_ptr<contract_state_t> state,
					std::vector<txin_t>& exec_inputs,
					std::vector<txout_t>& exec_outputs,
					std::unordered_map<addr_t, uint128>& amounts,
					contract_cache_t& contract_cache,
					std::shared_ptr<vm::StorageCache> storage_cache,
					uint64_t& tx_cost, const bool is_public) const;

	void execute(	std::shared_ptr<const Transaction> tx,
					std::shared_ptr<const execution_context_t> context,
					std::shared_ptr<contract_state_t> state,
					std::vector<txin_t>& exec_inputs,
					std::vector<txout_t>& exec_outputs,
					contract_cache_t& contract_cache,
					std::shared_ptr<vm::StorageCache> storage_cache,
					std::shared_ptr<vm::Engine> engine,
					const std::string& method_name,
					uint64_t& tx_cost, const bool is_public) const;

	std::shared_ptr<const exec_result_t>
	validate(	std::shared_ptr<const Transaction> tx, std::shared_ptr<const execution_context_t> context,
				std::shared_ptr<const Block> base = nullptr) const;

	void validate_diff_adjust(const uint64_t& block, const uint64_t& prev) const;

	void commit(std::shared_ptr<const Block> block);

	void purge_tree();

	bool add_proof(	const uint32_t height, const hash_t& challenge,
					std::shared_ptr<const ProofOfSpace> proof, const vnx::Hash64 farmer_mac);

	bool verify(std::shared_ptr<const ProofResponse> value);

	void verify_proof(std::shared_ptr<fork_t> fork, const hash_t& challenge) const;

	void verify_proof(std::shared_ptr<const ProofOfSpace> proof, const hash_t& challenge, std::shared_ptr<const BlockHeader> diff_block) const;

	void verify_vdf(std::shared_ptr<const ProofOfTime> proof) const;

	void verify_vdf(std::shared_ptr<const ProofOfTime> proof, const uint32_t chain) const;

	void verify_vdf_success(std::shared_ptr<const ProofOfTime> proof, std::shared_ptr<vdf_point_t> point);

	void verify_vdf_failed(std::shared_ptr<const ProofOfTime> proof);

	void verify_vdf_task(std::shared_ptr<const ProofOfTime> proof) const noexcept;

	void check_vdf_task(std::shared_ptr<fork_t> fork, std::shared_ptr<const BlockHeader> prev, std::shared_ptr<const BlockHeader> infuse) const noexcept;

	void apply(	std::shared_ptr<const Block> block,
				std::shared_ptr<const execution_context_t> context, bool is_replay = false);

	void apply(	std::shared_ptr<const Block> block,
				std::shared_ptr<const execution_context_t> context,
				std::shared_ptr<const Transaction> tx,
				balance_cache_t& balance_cache, uint32_t& counter);

	void revert(const uint32_t height);

	std::shared_ptr<const BlockHeader> get_root() const;

	std::shared_ptr<const BlockHeader> get_peak() const;

	std::shared_ptr<const BlockHeader> get_block_ex(const hash_t& hash, bool full_block) const;

	std::shared_ptr<const BlockHeader> get_block_at_ex(const uint32_t& height, bool full_block) const;

	std::shared_ptr<fork_t> find_fork(const hash_t& hash) const;

	std::shared_ptr<fork_t> find_prev_fork(std::shared_ptr<fork_t> fork, const size_t distance = 1) const;

	std::shared_ptr<const BlockHeader> find_prev_header(
			std::shared_ptr<const BlockHeader> block, const size_t distance = 1, bool clamped = false) const;

	std::shared_ptr<const BlockHeader> find_diff_header(std::shared_ptr<const BlockHeader> block, uint32_t offset = 0) const;

	std::shared_ptr<const BlockHeader> get_diff_header(std::shared_ptr<const BlockHeader> block, uint32_t offset = 0) const;

	hash_t get_challenge(std::shared_ptr<const BlockHeader> block, const hash_t& vdf_challenge, uint32_t offset = 0) const;

	bool find_vdf_challenge(std::shared_ptr<const BlockHeader> block, hash_t& vdf_challenge, uint32_t offset = 0) const;

	std::shared_ptr<vdf_point_t> find_vdf_point(const uint32_t height, const uint64_t vdf_start, const uint64_t vdf_iters,
			const std::array<hash_t, 2>& input, const std::array<hash_t, 2>& output) const;

	std::shared_ptr<vdf_point_t> find_next_vdf_point(std::shared_ptr<const BlockHeader> block) const;

	std::vector<std::pair<hash_t, proof_data_t>> find_proof(const hash_t& challenge) const;

	uint64_t calc_block_reward(std::shared_ptr<const BlockHeader> block) const;

	std::shared_ptr<const BlockHeader> read_block(vnx::File& file, bool full_block = true,
			int64_t* block_offset = nullptr, std::vector<std::pair<hash_t, int64_t>>* tx_offsets = nullptr) const;

	void write_block(std::shared_ptr<const Block> block, bool is_replay = false);

private:
	DataBase db;
	hash_t state_hash;

	hash_uint_uint_table<addr_t, uint32_t, uint32_t, txio_entry_t> recv_log;	// [[address, height, counter] => entry]
	hash_uint_uint_table<addr_t, uint32_t, uint32_t, txio_entry_t> spend_log;	// [[address, height, counter] => entry]
	hash_uint_uint_table<addr_t, uint32_t, uint32_t, exec_entry_t> exec_log;	// [[address, height, counter] => entry]

	hash_table<addr_t, std::shared_ptr<const Contract>> contract_map;			// [addr, contract]
	hash_multi_table<addr_t, addr_t> deploy_map;								// [sender => contract]
	hash_multi_table<bls_pubkey_t, addr_t> vplot_map;							// [farmer_key => contract]

	uint_uint_table<uint32_t, uint32_t, addr_t> offer_log;						// [[height, counter] => contract]
	uint_uint_table<uint32_t, uint32_t, addr_t> trade_log;						// [[height, counter] => contract]
	hash_uint_uint_table<addr_t, uint32_t, uint32_t, addr_t> offer_map;			// [[currency, height, counter] => contract]
	hash_uint_uint_table<addr_t, uint32_t, uint32_t, addr_t> trade_history;		// [[currency, height, counter] => contract]

	balance_table_t<uint128> balance_table;										// [[addr, currency] => balance]
	std::map<std::pair<addr_t, addr_t>, uint128> balance_map;					// [[addr, currency] => balance]
	std::map<uint32_t, balance_log_t> balance_log;								// [height => log]

	std::unordered_map<hash_t, tx_pool_t> tx_pool;									// [txid => transaction] (non-executed only)
	std::unordered_map<addr_t, uint64_t> tx_pool_fees;								// [address => total pending fees]
	std::unordered_map<hash_t, std::shared_ptr<fork_t>> fork_tree;					// [block hash => fork] (pending only)
	std::multimap<uint32_t, std::shared_ptr<fork_t>> fork_index;					// [height => fork] (pending only)
	std::map<uint32_t, std::shared_ptr<const BlockHeader>> history;					// [height => block header] (finalized only)

	std::multimap<uint32_t, std::shared_ptr<vdf_point_t>> verified_vdfs;			// [height => output]
	std::multimap<uint32_t, std::shared_ptr<const ProofOfTime>> pending_vdfs;		// [height => proof]

	std::unordered_map<hash_t, std::map<hash_t, proof_data_t>> proof_map;			// [challenge => best proofs]
	std::unordered_multimap<uint32_t, hash_t> challenge_map;						// [height => challenge]
	std::map<std::pair<hash_t, hash_t>, hash_t> created_blocks;						// [[prev hash, proof hash] => hash]
	std::unordered_set<hash_t> purged_blocks;

	bool is_synced = false;
	bool is_sync_fail = false;
	uint32_t min_pool_fee_ratio = 0;
	std::shared_ptr<vnx::File> block_chain;
	std::shared_ptr<vm::StorageDB> storage;
	mutable hash_table<hash_t, uint32_t> hash_index;							// [block hash => height]
	mutable hash_table<hash_t, std::pair<int64_t, uint32_t>> tx_index;			// [txid => [file offset, height]]
	mutable uint_table<uint32_t, std::pair<int64_t, hash_t>> block_index;		// [height => [file offset, block hash]]
	mutable uint_table<uint32_t, std::vector<hash_t>> tx_log;					// [height => txids]

	uint32_t sync_pos = 0;									// current sync height
	uint32_t sync_retry = 0;
	double max_sync_pending = 0;
	std::set<uint32_t> sync_pending;						// set of heights
	vnx::optional<uint32_t> sync_peak;						// max height we can sync
	std::unordered_set<hash_t> fetch_pending;				// block hash

	std::vector<std::shared_ptr<fork_t>> pending_forks;
	std::vector<std::shared_ptr<const ProofResponse>> pending_proofs;
	std::unordered_map<hash_t, std::shared_ptr<const Transaction>> pending_transactions;

	std::shared_ptr<vnx::Timer> stuck_timer;
	std::shared_ptr<vnx::Timer> update_timer;

	mutable std::mutex mutex;
	mutable std::shared_ptr<const NetworkInfo> network;
	mutable std::unordered_map<addr_t, std::shared_ptr<const Contract>> contract_cache;

	std::shared_ptr<const ChainParams> params;
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
