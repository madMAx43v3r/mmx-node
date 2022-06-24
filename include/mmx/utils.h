/*
 * utils.h
 *
 *  Created on: Dec 10, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_UTILS_H_
#define INCLUDE_MMX_UTILS_H_

#include <mmx/hash_t.hpp>
#include <mmx/BlockHeader.hxx>
#include <mmx/ChainParams.hxx>

#include <vnx/Config.hpp>

#include <uint128_t.h>
#include <uint256_t.h>


namespace mmx {

inline
std::shared_ptr<const ChainParams> get_params()
{
	auto params = ChainParams::create();
	vnx::read_config("chain.params", params);
	if(params->challenge_delay <= params->infuse_delay) {
		throw std::logic_error("challenge_delay <= infuse_delay");
	}
	if(params->challenge_interval <= params->challenge_delay) {
		throw std::logic_error("challenge_interval <= challenge_delay");
	}
	if(params->commit_delay >= params->challenge_interval - params->challenge_delay) {
		throw std::logic_error("commit_delay >= challenge_interval - challenge_delay");
	}
	return params;
}

inline
bool check_plot_filter(	std::shared_ptr<const ChainParams> params,
						const hash_t& challenge, const hash_t& plot_id)
{
	return hash_t(challenge + plot_id).to_uint256() >> (256 - params->plot_filter) == 0;
}

inline
uint256_t calc_proof_score(	std::shared_ptr<const ChainParams> params,
							const uint8_t ksize, const hash_t& quality, const uint64_t space_diff)
{
	uint256_t divider = (uint256_1 << (256 - params->score_bits)) / (uint128_t(space_diff) * params->space_diff_constant);
	divider *= (2 * ksize) + 1;
	divider <<= ksize - 1;
	return quality.to_uint256() / divider;
}

inline
uint256_t calc_virtual_score(	std::shared_ptr<const ChainParams> params,
								const hash_t& challenge, const hash_t& plot_id,
								const uint64_t balance, const uint64_t space_diff)
{
	uint256_t divider = (uint256_1 << (256 - params->score_bits)) / (uint128_t(space_diff) * params->virtual_space_constant);
	return hash_t(plot_id + challenge).to_uint256() / (divider * balance);
}

inline
uint64_t calc_block_reward(std::shared_ptr<const ChainParams> params, const uint64_t space_diff)
{
	return ((uint256_t(space_diff) * params->space_diff_constant * params->reward_factor.value) << (params->plot_filter + params->score_bits))
			/ params->score_target / params->reward_factor.inverse;
}

inline
uint64_t calc_final_block_reward(std::shared_ptr<const ChainParams> params, const uint64_t reward, const uint64_t fees)
{
	if(reward == 0) {
		return fees;
	}
	const uint64_t scale = 1 << 16;
	return uint64_t(std::max<int64_t>(int64_t(scale) - ((scale * fees) / (2 * reward)), 0) * reward) / scale + fees;
}

inline
uint64_t calc_total_netspace(std::shared_ptr<const ChainParams> params, const uint64_t space_diff)
{
	return 0.762 * uint64_t(
			((uint256_t(space_diff) * params->space_diff_constant) << (params->plot_filter + params->score_bits)) / params->score_target);
}

inline
uint64_t calc_virtual_plot_size(std::shared_ptr<const ChainParams> params, const uint64_t balance)
{
	return (762 * ((uint128_t(balance) * params->space_diff_constant) / (params->virtual_space_constant))) / 1000;
}

inline
uint64_t calc_new_space_diff(std::shared_ptr<const ChainParams> params, const uint64_t prev_diff, const uint32_t proof_score)
{
	int64_t delta = prev_diff * (int64_t(params->score_target) - proof_score);
	delta /= params->score_target;
	delta >>= params->max_diff_adjust;
	if(delta == 0) {
		delta = 1;
	}
	return std::max<int64_t>(int64_t(prev_diff) + delta, 1);
}

inline
uint128_t calc_block_weight(std::shared_ptr<const ChainParams> params, std::shared_ptr<const BlockHeader> diff_block,
							std::shared_ptr<const BlockHeader> block, bool have_farmer_sig)
{
	uint256_t weight = 0;
	// TODO: remove height switch
	if(block->height > 200000) {
		if(block->proof) {
			if(have_farmer_sig) {
				weight += params->score_threshold;
			}
			weight += params->score_threshold - block->proof->score;
		} else {
			weight += 1;
		}
	} else {
		 weight = have_farmer_sig ? 2 : 1;
		 if(block->proof) {
			 weight += params->score_threshold - block->proof->score;
		 }
	}
	weight *= diff_block->space_diff;
	weight *= diff_block->time_diff;
	if(weight.upper()) {
		throw std::logic_error("weight overflow");
	}
	return weight;
}

inline
void safe_acc(uint64_t& lhs, const uint64_t& rhs)
{
	const auto tmp = uint128_t(lhs) + rhs;
	if(tmp.upper()) {
		throw std::runtime_error("accumulate overflow");
	}
	lhs = tmp.lower();
}

inline
double amount_value(uint128_t amount, const int decimals)
{
	int i = 0;
	for(; amount.upper() && i < decimals; ++i) {
		amount /= 10;
	}
	if(amount.upper()) {
		return std::numeric_limits<double>::quiet_NaN();
	}
	return double(amount.lower()) * pow(10, i - decimals);
}


} // mmx

#endif /* INCLUDE_MMX_UTILS_H_ */
