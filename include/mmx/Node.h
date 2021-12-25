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
#include <mmx/txio_key_t.hpp>
#include <mmx/OCL_VDF.h>

#include <vnx/ThreadPool.h>
#include <vnx/addons/HttpInterface.h>


namespace mmx {

class Node : public NodeBase {
public:
	Node(const std::string& _vnx_value);

protected:
	void init() override;

	void main() override;

	uint32_t get_height() const override;

	vnx::optional<uint32_t> get_synced_height() const override;

	std::shared_ptr<const Block> get_block(const hash_t& hash) const override;

	std::shared_ptr<const Block> get_block_at(const uint32_t& height) const override;

	vnx::optional<hash_t> get_block_hash(const uint32_t& height) const override;

	vnx::optional<tx_key_t> get_tx_key(const hash_t& id) const override;

	txo_info_t get_txo_info(const txio_key_t& key) const override;

	std::shared_ptr<const Transaction> get_transaction(const hash_t& id) const override;

	void add_block(std::shared_ptr<const Block> block) override;

	void add_transaction(std::shared_ptr<const Transaction> tx) override;

	uint64_t get_balance(const addr_t& address, const addr_t& contract) const override;

	uint64_t get_total_balance(const std::vector<addr_t>& addresses, const addr_t& contract) const override;

	std::vector<std::pair<txio_key_t, utxo_t>> get_utxo_list(const std::vector<addr_t>& addresses) const override;

	void start_sync() override;

	void http_request_async(std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
							const vnx::request_id_t& request_id) const override;

	void http_request_chunk_async(	std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
									const int64_t& offset, const int64_t& max_bytes, const vnx::request_id_t& request_id) const override;

	void handle(std::shared_ptr<const Block> block);

	void handle(std::shared_ptr<const Transaction> tx);

	void handle(std::shared_ptr<const ProofOfTime> proof);

	void handle(std::shared_ptr<const ProofResponse> value);

private:
	struct fork_t {
		bool is_verified = false;
		bool is_proof_verified = false;
		uint32_t proof_score = -1;
		int64_t recv_time = 0;
		std::weak_ptr<fork_t> prev;
		std::shared_ptr<const Block> block;
		std::shared_ptr<const BlockHeader> diff_block;
	};

	struct vdf_point_t {
		std::array<hash_t, 2> output;
		int64_t recv_time = 0;
		uint32_t height = -1;
	};

	struct change_log_t {
		hash_t prev_state;
		std::vector<hash_t> tx_added;
		std::unordered_map<txio_key_t, utxo_t> utxo_added;								// [utxo key => utxo]
		std::unordered_map<txio_key_t, std::pair<txio_key_t, utxo_t>> utxo_removed;		// [utxo key => [spent txio key, utxo]]
	};

	void update();

	bool make_block(std::shared_ptr<const BlockHeader> prev, std::shared_ptr<const ProofResponse> response);

	void sync_more();

	void sync_result(uint32_t height, const std::vector<std::shared_ptr<const Block>>& blocks);

	std::shared_ptr<const BlockHeader> fork_to(std::shared_ptr<fork_t> fork_head);

	std::shared_ptr<fork_t> find_best_fork(std::shared_ptr<const BlockHeader> root = nullptr, const uint32_t* at_height = nullptr) const;

	std::vector<std::shared_ptr<fork_t>> get_fork_line(std::shared_ptr<fork_t> fork_head = nullptr) const;

	bool calc_fork_weight(std::shared_ptr<const BlockHeader> root, std::shared_ptr<fork_t> fork, uint64_t& total_weight) const;

	void validate(std::shared_ptr<const Block> block) const;

	uint64_t validate(std::shared_ptr<const Transaction> tx, std::shared_ptr<const Block> block = nullptr) const;

	void validate_diff_adjust(const uint64_t& block, const uint64_t& prev) const;

	void commit(std::shared_ptr<const Block> block) noexcept;

	void purge_tree();

	uint32_t verify_proof(std::shared_ptr<const Block> block, const hash_t& vdf_output) const;

	uint32_t verify_proof(std::shared_ptr<const ProofOfSpace> proof, const hash_t& challenge, const uint64_t space_diff) const;

	void verify_vdf(std::shared_ptr<const ProofOfTime> proof, const vdf_point_t& prev) const;

	void verify_vdf(std::shared_ptr<const ProofOfTime> proof, const uint32_t chain, const hash_t& begin) const;

	void verify_vdf_success(std::shared_ptr<const ProofOfTime> proof, const vdf_point_t& prev, const vdf_point_t& point);

	void verify_vdf_task(std::shared_ptr<const ProofOfTime> proof, const vdf_point_t& prev) const noexcept;

	void apply(std::shared_ptr<const Block> block) noexcept;

	void apply(std::shared_ptr<const Block> block, std::shared_ptr<const Transaction> tx, size_t index, change_log_t& log) noexcept;

	bool revert() noexcept;

	std::shared_ptr<const BlockHeader> get_root() const;

	std::shared_ptr<fork_t> find_fork(const hash_t& hash) const;

	std::shared_ptr<const Block> find_block(const hash_t& hash) const;

	std::shared_ptr<const BlockHeader> find_header(const hash_t& hash) const;

	std::shared_ptr<const BlockHeader> find_prev_header(
			std::shared_ptr<const BlockHeader> block, const size_t distance = 1, bool clamped = false) const;

	std::shared_ptr<const BlockHeader> find_diff_header(std::shared_ptr<const BlockHeader> block, uint32_t offset = 0) const;

	hash_t get_challenge(std::shared_ptr<const BlockHeader> block, const hash_t& vdf_challenge, uint32_t offset = 0) const;

	bool find_vdf_challenge(std::shared_ptr<const BlockHeader> block, hash_t& vdf_challenge, uint32_t offset = 0) const;

	uint64_t calc_block_reward(std::shared_ptr<const BlockHeader> block) const;

private:
	hash_t state_hash;
	std::list<std::shared_ptr<const change_log_t>> change_log;

	std::unordered_map<hash_t, uint32_t> hash_index;							// [block hash => height] (finalized only)
	std::unordered_map<hash_t, tx_key_t> tx_index;								// [txid => [height, index]] (finalized only)
	std::unordered_map<txio_key_t, txio_key_t> stxo_index;						// [txo key => spent txio key] (finalized + spent only)
	std::map<uint32_t, std::shared_ptr<const BlockHeader>> history;				// [height => block header] (finalized only)

	std::unordered_map<hash_t, tx_key_t> tx_map;								// [txid => [height, index]] (pending only)
	std::unordered_map<txio_key_t, utxo_t> utxo_map;							// [utxo key => utxo]
	std::unordered_map<addr_t, std::unordered_set<txio_key_t>> taddr_map;		// [addr => utxo keys] (pending + unspent only)
	std::set<std::pair<addr_t, txio_key_t>> addr_map;							// [addr => utxo keys] (finalized + unspent only)

	std::unordered_map<hash_t, std::shared_ptr<fork_t>> fork_tree;					// pending blocks
	std::unordered_map<hash_t, std::shared_ptr<const Contract>> contracts;			// current contract state
	std::unordered_map<hash_t, std::shared_ptr<const Transaction>> tx_pool;

	std::map<uint64_t, vdf_point_t> verified_vdfs;									// [end iters => output]
	std::multimap<uint64_t, std::shared_ptr<const ProofOfTime>> pending_vdfs;		// [start iters => proof]

	std::unordered_multimap<uint32_t, hash_t> challenge_map;						// [height => challenge]
	std::unordered_map<hash_t, std::shared_ptr<const ProofResponse>> proof_map;		// [challenge => proof]

	bool is_replay = true;
	bool is_synced = false;
	std::shared_ptr<vnx::File> block_chain;
	std::unordered_map<uint32_t, std::pair<int64_t, hash_t>> block_index;		// [height => [file offset, block hash]]

	uint32_t sync_pos = 0;									// current sync height
	uint32_t sync_peak = -1;								// max height we can sync
	uint32_t sync_update = 0;								// height of last update
	uint32_t sync_retry = 0;
	std::set<uint32_t> sync_pending;						// set of heights
	std::shared_ptr<vnx::Timer> update_timer;

	std::shared_ptr<const ChainParams> params;
	std::shared_ptr<RouterAsyncClient> router;
	std::shared_ptr<vnx::addons::HttpInterface<Node>> http;

	mutable std::mutex vdf_mutex;
	std::shared_ptr<OCL_VDF> opencl_vdf[2];
	std::shared_ptr<vnx::ThreadPool> threads;

	friend class vnx::addons::HttpInterface<Node>;

};


} // mmx

#endif /* INCLUDE_MMX_NODE_H_ */
