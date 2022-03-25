/*
 * utils.h
 *
 *  Created on: Dec 10, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_UTILS_H_
#define INCLUDE_MMX_UTILS_H_

#include <mmx/hash_t.hpp>
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
	if(params->finality_delay < 1) {
		throw std::logic_error("finality_delay < 1");
	}
	if(params->challenge_delay < 1) {
		throw std::logic_error("challenge_delay < 1");
	}
	if(params->commit_delay <= params->finality_delay) {
		throw std::logic_error("commit_delay <= finality_delay");
	}
	if(params->challenge_interval <= params->commit_delay) {
		throw std::logic_error("challenge_interval <= commit_delay");
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
uint128_t calc_proof_score(	std::shared_ptr<const ChainParams> params,
							const uint8_t ksize, const hash_t& quality, const uint64_t space_diff)
{
	uint256_t divider = (uint256_1 << (256 - params->score_bits)) / (uint128_t(space_diff) * params->space_diff_constant);
	divider *= (2 * ksize) + 1;
	divider <<= ksize - 1;
	return quality.to_uint256() / divider;
}

inline
uint64_t calc_block_reward(std::shared_ptr<const ChainParams> params, const uint64_t space_diff)
{
	return ((uint128_t(space_diff) * params->space_diff_constant * params->reward_factor.value) << (params->plot_filter + params->score_bits))
			/ params->score_target / params->reward_factor.inverse;
}

inline
uint64_t calc_final_block_reward(std::shared_ptr<const ChainParams> params, const uint64_t reward, const uint64_t fees)
{
	const uint64_t scale = 1 << 16;
	const uint64_t reward_ = std::max(reward, params->min_reward);
	return uint64_t(std::max<int64_t>(int64_t(scale) - ((scale * fees) / (2 * reward_)), 0) * reward_) / scale + fees;
}

inline
uint64_t calc_total_netspace(std::shared_ptr<const ChainParams> params, const uint64_t space_diff)
{
	return 0.762 * uint64_t(
			((uint128_t(space_diff) * params->space_diff_constant) << (params->plot_filter + params->score_bits)) / params->score_target);
}


} // mmx

#endif /* INCLUDE_MMX_UTILS_H_ */
