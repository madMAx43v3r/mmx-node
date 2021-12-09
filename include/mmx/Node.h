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
#include <mmx/utxo_key_t.hpp>


namespace mmx {

class Node : public NodeBase {
public:
	Node(const std::string& _vnx_value);

protected:
	void init() override;

	void main() override;

	void handle(std::shared_ptr<const Block> block);

	void handle(std::shared_ptr<const Transaction> tx);

	void handle(std::shared_ptr<const ProofOfTime> proof);

private:
	struct fork_t {
		bool is_verified = false;
		bool is_proof_verified = false;
		uint32_t proof_score = -1;
		std::weak_ptr<fork_t> prev;
		std::shared_ptr<const Block> block;
	};

	struct change_log_t {
		hash_t prev_state;
	};

	void update();

	bool calc_fork_weight(std::shared_ptr<const BlockHeader> root, std::shared_ptr<const fork_t> fork, uint64_t& total_weight);

	void verify(std::shared_ptr<const Block> block) const;

	void commit(std::shared_ptr<const Block> block) noexcept;

	size_t purge_tree();

	uint32_t verify_proof(std::shared_ptr<const Block> block, const hash_t& vdf_output) const;

	bool verify(std::shared_ptr<const ProofOfTime> proof, const hash_t& begin) const;

	void apply(std::shared_ptr<const Block> block) noexcept;

	void apply(change_log_t& log, std::shared_ptr<const Transaction> tx) noexcept;

	bool revert() noexcept;

	std::shared_ptr<const BlockHeader> get_root() const;

	std::shared_ptr<fork_t> find_fork(const hash_t& hash) const;

	std::shared_ptr<const Block> find_block(const hash_t& hash) const;

	std::shared_ptr<const BlockHeader> find_header(const hash_t& hash) const;

	std::shared_ptr<const Block> find_prev_block(std::shared_ptr<const Block> block, const size_t distance = 1) const;

	std::shared_ptr<const BlockHeader> find_prev_header(
			std::shared_ptr<const BlockHeader> block, const size_t distance = 1, bool clamp_to_genesis = false) const;

	hash_t get_challenge(std::shared_ptr<const BlockHeader> block) const;

	uint128_t calc_proof_score(const uint8_t ksize, const hash_t& quality, const uint64_t difficulty) const;

private:
	hash_t state_hash;
	std::list<std::shared_ptr<const change_log_t>> change_log;

	std::unordered_map<hash_t, size_t> finalized;								// [block hash => height]
	std::map<size_t, std::shared_ptr<const BlockHeader>> history;				// [height => block]

	std::unordered_map<hash_t, size_t> tx_map;									// [txid => block height]
	std::unordered_map<utxo_key_t, tx_out_t> utxo_map;							// [utxo key => utxo]
	std::unordered_map<hash_t, std::unordered_set<utxo_key_t>> addr_map;		// [addr => utxo keys]

	std::unordered_map<hash_t, std::shared_ptr<fork_t>> fork_tree;
	std::unordered_map<hash_t, std::shared_ptr<const Contract>> contracts;
	std::unordered_map<hash_t, std::shared_ptr<const Transaction>> tx_pool;

	std::unordered_map<uint64_t, hash_t> verified_vdfs;							// [iters => output]

	std::shared_ptr<const ChainParams> params;

};


} // mmx

#endif /* INCLUDE_MMX_NODE_H_ */
