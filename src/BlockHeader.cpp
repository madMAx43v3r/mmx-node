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
	if(!farmer_sig && height) {
		if(nonce || proof || tx_count) {
			return false;
		}
	}
	const auto all_hash = calc_hash();
	return version == 0
			&& (!proof || proof->is_valid())
			&& (!reward_amount || reward_addr)
			&& hash == all_hash.first
			&& content_hash == all_hash.second;
}

std::pair<hash_t, hash_t> BlockHeader::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	buffer.reserve(64 * 1024);

	write_bytes(out, get_type_hash());
	write_field(out, "version", 	version);
	write_field(out, "prev",		prev);
	write_field(out, "height", 		height);
	write_field(out, "nonce", 		nonce);
	write_field(out, "time_stamp", 	time_stamp);
	write_field(out, "time_diff", 	time_diff);
	write_field(out, "space_diff", 	space_diff);
	write_field(out, "weight", 		weight);
	write_field(out, "total_weight",	total_weight);
	write_field(out, "netspace_ratio", 	netspace_ratio);
	write_field(out, "txfee_buffer", 	txfee_buffer);
	write_field(out, "vdf_iters", 	vdf_iters);
	write_field(out, "vdf_output", 	vdf_output);
	write_field(out, "vdf_reward_addr", vdf_reward_addr);
	write_field(out, "vdf_reward_vote", vdf_reward_vote);
	write_field(out, "proof", 		proof ? proof->calc_hash() : hash_t());
	write_field(out, "proof_score_sum", proof_score_sum);
	write_field(out, "reward_amount", reward_amount);
	write_field(out, "reward_addr", reward_addr);
	write_field(out, "reward_contract", reward_contract);
	write_field(out, "reward_account", reward_account);
	write_field(out, "reward_vote", reward_vote);
	write_field(out, "reward_vote_sum", reward_vote_sum);
	write_field(out, "base_reward", base_reward);
	write_field(out, "static_cost", static_cost);
	write_field(out, "total_cost", 	total_cost);
	write_field(out, "tx_count", 	tx_count);
	write_field(out, "tx_fees", 	tx_fees);
	write_field(out, "tx_hash", 	tx_hash);
	out.flush();

	std::pair<hash_t, hash_t> res;
	res.first = hash_t(buffer);

	write_field(out, "farmer_sig", farmer_sig);
	out.flush();
	res.second = hash_t(buffer);
	return res;
}

void BlockHeader::validate() const
{
	if(farmer_sig) {
		if(!proof) {
			throw std::logic_error("missing proof");
		}
		if(!farmer_sig->verify(proof->farmer_key, hash)) {
			throw std::logic_error("invalid farmer signature");
		}
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
	const uint32_t proof_score = proof ? proof->score : params->score_threshold;

	if(height % params->challenge_interval == 0) {
		space_diff = calc_new_space_diff(params, prev);
		proof_score_sum = proof_score;
	} else {
		space_diff = prev->space_diff;
		proof_score_sum = prev->proof_score_sum + proof_score;
	}
}

void BlockHeader::set_base_reward(std::shared_ptr<const ChainParams> params, std::shared_ptr<const BlockHeader> prev)
{
	if(height % params->reward_adjust_interval == 0) {
		base_reward = calc_new_base_reward(params, prev);
		reward_vote_sum = reward_vote;
	} else {
		base_reward = prev->base_reward;
		reward_vote_sum = prev->reward_vote_sum + reward_vote;
	}
}


} // mmx
