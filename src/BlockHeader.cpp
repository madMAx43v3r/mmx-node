/*
 * BlockHeader.cpp
 *
 *  Created on: Nov 30, 2021
 *      Author: mad
 */

#include <mmx/BlockHeader.hxx>
#include <mmx/write_bytes.h>
#include <mmx/utils.h>


namespace mmx {

vnx::bool_t BlockHeader::is_valid() const
{
	if(height) {
		if(!proof || !farmer_sig || vdf_count < 1) {
			return false;
		}
	}
	return version == 0
			&& (!proof || proof->is_valid())
			&& (!reward_amount || reward_addr)
			&& hash == calc_hash()
			&& content_hash == calc_content_hash();
}

hash_t BlockHeader::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	buffer.reserve(64 * 1024);

	write_bytes(out, get_type_hash());
	write_field(out, "version", 		version);
	write_field(out, "support_flags", 	support_flags);
	write_field(out, "prev",			prev);
	write_field(out, "height", 			height);
	write_field(out, "vdf_height", 		vdf_height);
	write_field(out, "nonce", 			nonce);
	write_field(out, "time_stamp", 		time_stamp);
	write_field(out, "time_diff", 		time_diff);
	write_field(out, "space_diff", 		space_diff);
	write_field(out, "weight", 			weight);
	write_field(out, "total_weight",	total_weight);
	write_field(out, "vdf_count", 		vdf_count);
	write_field(out, "vdf_iters", 		vdf_iters);
	write_field(out, "vdf_output", 		vdf_output);
	write_field(out, "vdf_reward_addr", vdf_reward_addr);
	write_field(out, "vdf_reward_payout", vdf_reward_payout);
	write_field(out, "proof", 			proof ? proof->calc_hash() : hash_t());
	write_field(out, "proof_hash", 		proof_hash);
	write_field(out, "challenge", 		challenge);
	write_field(out, "is_space_fork",	is_space_fork);
	write_field(out, "space_fork_len",	space_fork_len);
	write_field(out, "space_fork_proofs", space_fork_proofs);
	write_field(out, "reward_amount", 	reward_amount);
	write_field(out, "reward_addr", 	reward_addr);
	write_field(out, "reward_contract", reward_contract);
	write_field(out, "reward_account", 	reward_account);
	write_field(out, "reward_vote", 	reward_vote);
	write_field(out, "reward_vote_sum", reward_vote_sum);
	write_field(out, "reward_vote_count", reward_vote_count);
	write_field(out, "base_reward", 	base_reward);
	write_field(out, "static_cost", 	static_cost);
	write_field(out, "total_cost", 		total_cost);
	write_field(out, "tx_count", 		tx_count);
	write_field(out, "tx_fees", 		tx_fees);
	write_field(out, "txfee_buffer", 	txfee_buffer);
	write_field(out, "tx_hash", 		tx_hash);
	out.flush();

	return hash_t(buffer);
}

hash_t BlockHeader::calc_content_hash() const
{
	if(farmer_sig) {
		return hash_t(hash + (*farmer_sig));
	}
	return hash_t(hash.bytes);
}

void BlockHeader::validate() const
{
	if(!proof) {
		throw std::logic_error("missing proof");
	}
	if(!farmer_sig) {
		throw std::logic_error("missing farmer signature");
	}
	if(!farmer_sig->verify(proof->farmer_key, hash)) {
		throw std::logic_error("invalid farmer signature");
	}
}

std::shared_ptr<const BlockHeader> BlockHeader::get_header() const
{
	return vnx::clone(*this);
}

block_index_t BlockHeader::get_block_index(const int64_t& file_offset) const
{
	block_index_t index;
	index.hash = hash;
	index.content_hash = content_hash;
	index.static_cost = static_cost;
	index.total_cost = total_cost;
	index.file_offset = file_offset;
	return index;
}

void BlockHeader::set_space_diff(std::shared_ptr<const ChainParams> params, std::shared_ptr<const BlockHeader> prev)
{
	if(!proof || !vdf_count) {
		throw std::logic_error("invalid block state");
	}
	const auto proof_count = calc_proof_count(params, proof->score);

	if(is_space_fork) {
		space_diff = calc_new_space_diff(params, prev);
		space_fork_len = vdf_count;
		space_fork_proofs = proof_count;
	} else {
		space_diff = prev->space_diff;
		space_fork_len = prev->space_fork_len + vdf_count;
		space_fork_proofs = prev->space_fork_proofs + proof_count;
	}
}

void BlockHeader::set_base_reward(std::shared_ptr<const ChainParams> params, std::shared_ptr<const BlockHeader> prev)
{
	if(height % params->reward_adjust_interval == 0) {
		base_reward = calc_new_base_reward(params, prev);
		reward_vote_sum = reward_vote;
		reward_vote_count = (reward_vote ? 1 : 0);
	} else {
		base_reward = prev->base_reward;
		reward_vote_sum = prev->reward_vote_sum + reward_vote;
		reward_vote_count = prev->reward_vote_count + (reward_vote ? 1 : 0);
	}
}


} // mmx
