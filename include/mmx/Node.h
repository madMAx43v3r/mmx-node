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
#include <mmx/utxo_key_t.hpp>


namespace mmx {

class Node : public NodeBase {
public:
	Node(const std::string& _vnx_value);

protected:
	void init() override;

	void main() override;

	uint32_t get_height() const override;

	std::shared_ptr<const Block> get_block(const hash_t& hash) const override;

	std::shared_ptr<const Block> get_block_at(const uint32_t& height) const override;

	vnx::optional<hash_t> get_block_hash(const uint32_t& height) const override;

	vnx::optional<tx_info_t> get_tx_info(const hash_t& id) const override;

	utxo_info_t get_utxo_info(const utxo_key_t& key) const override;

	std::shared_ptr<const Transaction> get_transaction(const hash_t& id) const override;

	void add_block(std::shared_ptr<const Block> block) override;

	void add_transaction(std::shared_ptr<const Transaction> tx) override;

	uint64_t get_balance(const addr_t& address, const addr_t& contract) const override;

	uint64_t get_total_balance(const std::vector<addr_t>& addresses, const addr_t& contract) const override;

	std::vector<std::pair<utxo_key_t, utxo_t>> get_utxo_list(const std::vector<addr_t>& addresses) const override;

	void start_sync() override;

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
		hash_t output;
		int64_t recv_time = 0;
	};

	struct change_log_t {
		hash_t prev_state;
		std::vector<hash_t> tx_added;
		std::unordered_map<utxo_key_t, std::pair<hash_t, utxo_t>> utxo_added;
		std::unordered_map<utxo_key_t, std::pair<hash_t, utxo_t>> utxo_removed;
	};

	void update();

	bool make_block(std::shared_ptr<const BlockHeader> prev, std::shared_ptr<const ProofResponse> response);

	void sync_more();

	void sync_result(uint32_t height, const std::vector<std::shared_ptr<const Block>>& blocks);

	std::shared_ptr<Node::fork_t> find_best_fork(std::shared_ptr<const BlockHeader> root, const uint32_t* fork_height = nullptr) const;

	std::vector<std::shared_ptr<Node::fork_t>> get_fork_line(std::shared_ptr<fork_t> fork_head = nullptr) const;

	bool calc_fork_weight(std::shared_ptr<const BlockHeader> root, std::shared_ptr<fork_t> fork, uint128_t& total_weight) const;

	void validate(std::shared_ptr<const Block> block) const;

	uint64_t validate(std::shared_ptr<const Transaction> tx, std::shared_ptr<const Block> block = nullptr) const;

	void validate_diff_adjust(const uint64_t& block, const uint64_t& prev) const;

	void commit(std::shared_ptr<const Block> block) noexcept;

	size_t purge_tree();

	uint32_t verify_proof(std::shared_ptr<const Block> block, const hash_t& vdf_output) const;

	uint32_t verify_proof(std::shared_ptr<const ProofOfSpace> proof, const hash_t& challenge, const uint64_t space_diff) const;

	void verify_vdf(std::shared_ptr<const ProofOfTime> proof, const hash_t& begin) const;

	void apply(std::shared_ptr<const Block> block) noexcept;

	void apply(std::shared_ptr<const Block> block, std::shared_ptr<const Transaction> tx, size_t index, change_log_t& log) noexcept;

	bool revert() noexcept;

	std::shared_ptr<const BlockHeader> get_root() const;

	std::shared_ptr<fork_t> find_fork(const hash_t& hash) const;

	std::shared_ptr<const Block> find_block(const hash_t& hash) const;

	std::shared_ptr<const BlockHeader> find_header(const hash_t& hash) const;

	std::shared_ptr<const BlockHeader> find_prev_header(
			std::shared_ptr<const BlockHeader> block, const size_t distance = 1, bool clamp_to_genesis = false) const;

	std::shared_ptr<const BlockHeader> get_diff_header(std::shared_ptr<const BlockHeader> block, bool for_next = false) const;

	hash_t get_challenge(std::shared_ptr<const BlockHeader> block, const hash_t& vdf_challenge, uint32_t offset = 0) const;

	bool find_vdf_challenge(std::shared_ptr<const BlockHeader> block, hash_t& vdf_challenge, uint32_t offset = 0) const;

	bool find_vdf_output(const uint64_t vdf_iters, hash_t& vdf_output) const;

	uint64_t calc_block_reward(std::shared_ptr<const BlockHeader> block) const;

private:
	hash_t state_hash;
	std::list<std::shared_ptr<const change_log_t>> change_log;

	std::unordered_map<hash_t, uint32_t> hash_index;							// [block hash => height] (finalized only)
	std::unordered_map<hash_t, tx_info_t> tx_index;								// [txid => [height, index]] (finalized only)
	std::unordered_map<utxo_key_t, hash_t> utxo_index;							// [utxo key => spent txid] (finalized + spent only)
	std::unordered_multimap<addr_t, utxo_key_t> addr_map;						// [addr => utxo keys] (finalized + unspent only)
	std::map<uint32_t, std::shared_ptr<const BlockHeader>> history;				// [height => block header] (finalized only)

	std::unordered_map<hash_t, tx_info_t> tx_map;								// [txid => [height, index]] (only pending)
	std::unordered_map<utxo_key_t, utxo_t> utxo_map;							// [utxo key => utxo]

	std::unordered_map<hash_t, std::shared_ptr<fork_t>> fork_tree;
	std::unordered_map<hash_t, std::shared_ptr<const Contract>> contracts;
	std::unordered_map<hash_t, std::shared_ptr<const Transaction>> tx_pool;

	std::map<uint64_t, vdf_point_t> verified_vdfs;									// [iters => output]
	std::unordered_multimap<uint32_t, hash_t> challenge_map;						// [height => challenge]
	std::unordered_map<hash_t, std::shared_ptr<const ProofResponse>> proof_map;		// [challenge => proof]

	bool is_replay = true;
	bool is_synced = false;
	std::shared_ptr<vnx::File> block_chain;
	std::unordered_map<uint32_t, std::pair<int64_t, hash_t>> block_index;		// [height => [file offset, block hash]]

	uint32_t sync_pos = 0;
	uint32_t sync_peak = -1;
	std::set<uint32_t> pending_syncs;

	std::shared_ptr<RouterAsyncClient> router;
	std::shared_ptr<const ChainParams> params;

};


} // mmx

#endif /* INCLUDE_MMX_NODE_H_ */
