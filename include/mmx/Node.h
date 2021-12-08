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
	struct ChangeLog {
		hash_t prev_state_hash;
	};

	void update();

	bool calc_fork_weight(std::shared_ptr<const Block> block, uint64_t& total_score);

	void verify(std::shared_ptr<const Block> block) const;

	uint64_t verify_proof(std::shared_ptr<const Block> block, const hash_t& vdf_output) const;

	bool verify(std::shared_ptr<const ProofOfTime> proof, const hash_t& begin) const;

	void apply(std::shared_ptr<const Block> block) noexcept;

	void apply(ChangeLog& log, std::shared_ptr<const Transaction> tx) noexcept;

	bool revert() noexcept;

	std::shared_ptr<const BlockHeader> get_root() const;

	std::shared_ptr<const Block> find_block(const hash_t& hash) const;

	std::shared_ptr<const BlockHeader> find_header(const hash_t& hash) const;

	std::shared_ptr<const Block> find_prev_block(std::shared_ptr<const Block> block, const size_t distance = 1) const;

	std::shared_ptr<const BlockHeader> find_prev_header(
			std::shared_ptr<const BlockHeader> block, const size_t distance = 1, bool clamp_to_genesis = false) const;

	hash_t get_challenge(std::shared_ptr<const BlockHeader> block) const;

private:
	hash_t state_hash;
	std::vector<std::shared_ptr<const ChangeLog>> change_log;

	std::unordered_map<hash_t, size_t> finalized;					// [block hash => height]
	std::map<size_t, std::shared_ptr<const BlockHeader>> history;	// [height => block]

	std::unordered_map<hash_t, hash_t> tx_block_map;				// [txid => block hash]
	std::unordered_map<utxo_key_t, tx_out_t> utxo_map;
	std::unordered_multimap<hash_t, utxo_key_t> addr_map;

	std::unordered_map<hash_t, std::shared_ptr<const Block>> fork_tree;
	std::unordered_map<hash_t, std::shared_ptr<const Contract>> contracts;
	std::unordered_map<hash_t, std::shared_ptr<const Transaction>> tx_pool;

	std::unordered_map<uint64_t, hash_t> verified_vdfs;				// [iters => output]
	std::unordered_map<hash_t, uint64_t> verified_proofs;			// [block hash => score]
	std::unordered_set<hash_t> verified_blocks;						// [block hash]

	std::shared_ptr<const ChainParams> params;

};


} // mmx

#endif /* INCLUDE_MMX_NODE_H_ */
