/*
 * utils.h
 *
 *  Created on: Dec 10, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_UTILS_H_
#define INCLUDE_MMX_UTILS_H_

#include <mmx/hash_t.hpp>
#include <mmx/fixed128.hpp>
#include <mmx/BlockHeader.hxx>
#include <mmx/ChainParams.hxx>

#include <vnx/Config.hpp>

#include <uint128_t.h>
#include <uint256_t.h>
#include <cmath>

static constexpr double UINT64_MAX_DOUBLE = 18446744073709549568.0;


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
double to_value(const uint64_t amount, const int decimals) {
	return amount * pow(10, -decimals);
}

inline
double to_value(const uint128_t& amount, const int decimals) {
	return (double(amount.upper()) * pow(2, 64) + double(amount.lower())) * pow(10, -decimals);
}

inline
double to_value(const uint64_t amount, std::shared_ptr<const ChainParams> params) {
	return to_value(amount, params->decimals);
}

inline
double to_value_128(uint128_t amount, const int decimals)
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

inline
uint64_t to_amount(const double value, const int decimals)
{
	const auto amount = value * pow(10, decimals);
	if(amount < 0) {
		throw std::runtime_error("negative amount: " + std::to_string(value));
	}
	if(amount > UINT64_MAX_DOUBLE) {
		throw std::runtime_error("amount overflow: " + std::to_string(value));
	}
	if(amount == std::numeric_limits<double>::quiet_NaN()) {
		throw std::runtime_error("invalid amount: " + std::to_string(value));
	}
	return amount;
}

inline
uint64_t to_amount(const fixed128& value, const int decimals)
{
	const auto amount = value.to_amount(decimals);
	if(amount.upper()) {
		throw std::runtime_error("amount overflow: " + amount.str(10));
	}
	return amount;
}

inline
uint64_t to_amount(const double value, std::shared_ptr<const ChainParams> params) {
	return to_amount(value, params->decimals);
}

inline
bool check_tx_inclusion(const hash_t& txid, const uint32_t height)
{
	return uint32_t(txid.bytes[31] & 0x1) == (height & 0x1);
}

inline
bool check_plot_filter(
		std::shared_ptr<const ChainParams> params, const hash_t& challenge, const hash_t& plot_id)
{
	const hash_t hash(std::string("plot_filter") + plot_id + challenge);
	return (hash.to_uint256() >> (256 - params->plot_filter)) == 0;
}

inline
hash_t get_plot_challenge(const hash_t& challenge, const hash_t& plot_id)
{
	return hash_t(std::string("plot_challenge") + plot_id + challenge);
}

inline
uint64_t to_effective_space(const uint64_t num_bytes)
{
	return (2467 * uint128_t(num_bytes)) / 1000;
}

inline
uint64_t calc_total_netspace_ideal(std::shared_ptr<const ChainParams> params, const uint64_t space_diff)
{
	return ((uint256_t(space_diff) * params->space_diff_constant) << params->score_bits) / params->score_target;
}

inline
uint64_t calc_total_netspace(std::shared_ptr<const ChainParams> params, const uint64_t space_diff)
{
	return to_effective_space(calc_total_netspace_ideal(params, space_diff));
}

inline
uint256_t calc_proof_score(	std::shared_ptr<const ChainParams> params,
							const uint8_t ksize, const hash_t& quality, const uint64_t space_diff)
{
	uint256_t divider = (uint256_1 << (256 - params->score_bits)) / (uint128_t(space_diff) * params->space_diff_constant);
	divider *= (2 * ksize) + 1;
	return (quality.to_uint256() / divider) >> (ksize - 1);
}

inline
uint256_t calc_virtual_score(	std::shared_ptr<const ChainParams> params,
								const hash_t& challenge, const hash_t& plot_id,
								const uint64_t balance, const uint64_t space_diff)
{
	if(balance == 0) {
		throw std::logic_error("zero balance (virtual plot)");
	}
	const hash_t quality(std::string("virtual_score") + plot_id + challenge);
	const uint256_t divider = (uint256_1 << (256 - params->score_bits)) / (uint128_t(space_diff) * params->space_diff_constant);
	return (quality.to_uint256() / divider) / (uint128_t(balance) * params->virtual_space_constant);
}

inline
uint64_t calc_project_reward(std::shared_ptr<const ChainParams> params, const uint64_t tx_fees)
{
	const auto dynamic = (params->project_ratio.value * uint64_t(tx_fees)) / params->project_ratio.inverse;
	return std::min(params->fixed_project_reward + dynamic, tx_fees / 2);
}

inline
uint64_t calc_final_block_reward(std::shared_ptr<const ChainParams> params, const uint64_t base_reward, const uint64_t avg_txfee, const uint64_t tx_fees)
{
	const uint64_t fee_deduction = calc_project_reward(params, tx_fees);

	uint64_t min_reward = 0;
	if(params->min_reward > avg_txfee) {
		min_reward = params->min_reward - avg_txfee;
	}
	return std::max(base_reward, min_reward) + (tx_fees > fee_deduction ? tx_fees - fee_deduction : 0);
}

inline
uint64_t calc_next_base_reward(std::shared_ptr<const ChainParams> params, const uint64_t base_reward, const int8_t vote)
{
	const auto step_size = std::max<int64_t>(base_reward / params->reward_adjust_div, 1);

	int64_t reward = base_reward;
	if(vote > 0) {
		reward += step_size;
	} else if(vote < 0) {
		reward -= step_size;
	}
	return std::max<int64_t>(std::min<int64_t>(reward, 4200000000000ll), 0);
}

inline
uint64_t get_effective_plot_size(const int ksize)
{
	return to_effective_space(uint64_t((2 * ksize) + 1) << (ksize - 1));
}

inline
uint64_t get_virtual_plot_size(std::shared_ptr<const ChainParams> params, const uint64_t balance)
{
	return to_effective_space(uint128_t(balance) * params->virtual_space_constant);
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
uint64_t calc_new_netspace_ratio(std::shared_ptr<const ChainParams> params, const uint64_t prev_ratio, const bool is_og_proof)
{
	const uint64_t value = is_og_proof ? uint64_t(1) << (2 * params->max_diff_adjust) : 0;
	return (prev_ratio * ((uint64_t(1) << params->max_diff_adjust) - 1) + value) >> params->max_diff_adjust;
}

inline
uint64_t calc_new_average_txfee(std::shared_ptr<const ChainParams> params, const uint64_t prev_value, const uint64_t total_fees)
{
	return (prev_value * ((uint64_t(1) << params->avg_txfee_adjust) - 1) + total_fees) >> params->avg_txfee_adjust;
}

inline
uint128_t calc_block_weight(std::shared_ptr<const ChainParams> params, std::shared_ptr<const BlockHeader> diff_block,
							std::shared_ptr<const BlockHeader> block)
{
	uint256_t weight = 0;
	if(block->proof) {
		weight += params->score_threshold + (params->score_threshold - block->proof->score);
		weight *= diff_block->space_diff;
		weight *= diff_block->time_diff;
	}
	if(weight.upper()) {
		throw std::logic_error("block weight overflow");
	}
	return weight;
}

inline
std::string get_finger_print(const hash_t& seed_value, const vnx::optional<std::string>& passphrase)
{
	hash_t pass_hash;
	if(passphrase) {
		pass_hash = hash_t("MMX/fingerprint/" + *passphrase);
	}
	hash_t hash;
	for(int i = 0; i < 16384; ++i) {
		hash = hash_t(hash + seed_value + pass_hash);
	}
	return std::to_string(hash.to_uint<uint32_t>());
}

template<typename error_t>
uint64_t cost_to_fee(const uint64_t cost, const uint32_t fee_ratio)
{
	const auto fee = (uint128_t(cost) * fee_ratio) / 1024;
	if(fee.upper()) {
		throw error_t("fee amount overflow");
	}
	return fee;
}

template<typename error_t>
uint64_t fee_to_cost(const uint64_t fee, const uint32_t fee_ratio)
{
	const auto cost = (uint128_t(fee) * 1024) / fee_ratio;
	if(cost.upper()) {
		throw error_t("cost value overflow");
	}
	return cost;
}

template<typename error_t>
void safe_acc(uint64_t& lhs, const uint64_t& rhs)
{
	const auto tmp = uint128_t(lhs) + rhs;
	if(tmp.upper()) {
		throw error_t("accumulate overflow");
	}
	lhs = tmp.lower();
}

template<typename T, typename S>
T clamped_sub(const T L, const S R)
{
	if(L > R) {
		return L - R;
	}
	return T(0);
}

template<typename T, typename S>
void clamped_sub_assign(T& L, const S R) {
	L = clamped_sub(L, R);
}


} // mmx

#endif /* INCLUDE_MMX_UTILS_H_ */
