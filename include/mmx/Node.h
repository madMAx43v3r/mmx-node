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
	void check_forks();

	void verify(std::shared_ptr<const Block> block) const;

	void verify(std::shared_ptr<const ProofOfSpace> proof,
				const hash_t& challenge, const uint64_t difficulty, uint64_t& quality) const;

	void verify(std::shared_ptr<const ProofOfTime> proof, const hash_t& begin) const;

	void apply(std::shared_ptr<const Block> block) noexcept;

	void apply(std::shared_ptr<const Transaction> tx) noexcept;

	void revert(std::shared_ptr<const Transaction> tx) noexcept;

	std::shared_ptr<const Block> revert() noexcept;

	std::shared_ptr<const Block> get_root() const;

	std::shared_ptr<const Block> find_block(const hash_t& hash) const;

	std::shared_ptr<const BlockHeader> find_header(const hash_t& hash) const;

	std::shared_ptr<const Block> find_prev_block(std::shared_ptr<const Block> block, const size_t distance) const;

	std::shared_ptr<const BlockHeader> find_prev_header(std::shared_ptr<const BlockHeader> block, const size_t distance) const;

private:
	ChainParams params;

	std::shared_ptr<const Block> curr_peak;

	std::map<hash_t, size_t> finalized;								// [block hash => height]
	std::map<size_t, std::shared_ptr<const BlockHeader>> history;	// [height => block]

	std::unordered_map<hash_t, hash_t> tx_block_map;				// [txid => block hash]
	std::unordered_map<utxo_key_t, tx_out_t> utxo_map;
	std::unordered_multimap<hash_t, utxo_key_t> addr_map;

	std::unordered_map<hash_t, std::shared_ptr<const Block>> fork_tree;
	std::unordered_map<hash_t, std::shared_ptr<const Contract>> contracts;
	std::unordered_map<hash_t, std::shared_ptr<const Transaction>> tx_pool;

	std::unordered_map<uint64_t, hash_t> verified_vdfs;				// [iters => output]
	std::unordered_map<hash_t, uint64_t> verified_blocks;			// [hash => quality]

};


} // mmx

#endif /* INCLUDE_MMX_NODE_H_ */
