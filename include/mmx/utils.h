/*
 * utils.h
 *
 *  Created on: Dec 10, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_UTILS_H_
#define INCLUDE_MMX_UTILS_H_

#include <mmx/hash_t.hpp>
#include <mmx/uint128.hpp>
#include <mmx/fixed128.hpp>
#include <mmx/BlockHeader.hxx>
#include <mmx/ChainParams.hxx>

#include <vnx/Util.hpp>
#include <vnx/TimeUtil.h>
#include <vnx/Config.hpp>

#include <uint128_t.h>
#include <uint256_t.h>
#include <cmath>


namespace mmx {

bool is_json(const vnx::Variant& var);

uint64_t get_num_bytes(const vnx::Variant& var);

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
	if(params->time_diff_constant % (2 * params->vdf_segment_size)) {
		throw std::logic_error("time_diff_constant not multiple of 2x vdf_segment_size");
	}
	if(params->max_tx_cost > params->max_block_size / 5) {
		throw std::logic_error("max_tx_cost > band size");
	}
	return params;
}

inline
double to_value(const uint128_t& amount, const int decimals) {
	return (double(amount.upper()) * pow(2, 64) + double(amount.lower())) * pow(10, -decimals);
}

inline
double to_value(const uint128_t& amount, std::shared_ptr<const ChainParams> params) {
	return to_value(amount, params->decimals);
}

inline
uint128 to_amount(const double value, const int decimals)
{
	if(decimals < 0 || decimals > 18) {
		throw std::runtime_error("invalid decimals: " + std::to_string(decimals));
	}
	if(value == std::numeric_limits<double>::quiet_NaN()) {
		throw std::runtime_error("invalid value: NaN");
	}
	if(value < 0) {
		throw std::runtime_error("negative value: " + std::to_string(value));
	}
	auto shift = uint128_1;
	for(int i = 0; i < decimals; ++i) {
		shift *= 10;
	}
	const double div_64 = pow(2, 64);
	const uint64_t value_128 = value / div_64;
	const uint64_t value_64  = fmod(value, div_64);

	const uint256_t amount =
			uint256_t((uint128_t(value_128) << 64) + value_64) * shift
			+ uint64_t(fmod(value, 1) * pow(10, decimals) + 0.5);
	if(amount >> 128) {
		throw std::runtime_error("amount overflow: " + amount.str(10));
	}
	return amount;
}

inline
uint128 to_amount(const fixed128& value, const int decimals)
{
	return value.to_amount(decimals);
}

inline
uint128 to_amount(const double value, std::shared_ptr<const ChainParams> params) {
	return to_amount(value, params->decimals);
}

inline
bool check_plot_filter(
		std::shared_ptr<const ChainParams> params, const hash_t& challenge, const hash_t& plot_id)
{
	const hash_t hash(std::string("plot_filter") + plot_id + challenge);
	return (hash.to_uint256() >> (256 - params->plot_filter)) == 0;
}

inline
bool check_space_fork(std::shared_ptr<const ChainParams> params, const hash_t& challenge, const hash_t& proof_hash)
{
	const hash_t infuse_hash(std::string("proof_infusion_check") + challenge + proof_hash);

	return infuse_hash.to_uint256() % params->challenge_interval == 0;
}

inline
hash_t calc_next_challenge(
		std::shared_ptr<const ChainParams> params, const hash_t& challenge,
		const uint32_t vdf_count, const hash_t& proof_hash, bool& is_space_fork)
{
	hash_t out = challenge;
	for(uint32_t i = 0; i < vdf_count; ++i) {
		out = hash_t(std::string("next_challenge") + out);
	}
	is_space_fork = check_space_fork(params, out, proof_hash);

	if(is_space_fork) {
		out = hash_t(std::string("challenge_infusion") + out + proof_hash);
	}
	return out;
}

inline
hash_t get_plot_challenge(const hash_t& challenge, const hash_t& plot_id)
{
	return hash_t(std::string("plot_challenge") + plot_id + challenge);
}

inline
uint128_t to_effective_space(const uint128_t num_bytes)
{
	return (2464 * num_bytes) / 1000;
}

inline
uint128_t calc_total_netspace(std::shared_ptr<const ChainParams> params, const uint64_t space_diff)
{
	// the win chance of a k32 at diff 1 is: 0.6979321856
	// don't ask why times two, it works
	const auto ideal = uint128_t(space_diff) * params->space_diff_constant * params->avg_proof_count * 2;
	return to_effective_space(ideal);
}

inline
bool check_proof_threshold(std::shared_ptr<const ChainParams> params,
							const uint8_t ksize, const hash_t& quality, const uint64_t space_diff, const bool hard_fork)
{
	if(space_diff <= 0) {
		return false;
	}
	const auto threshold =
			((uint256_1 << 255) / (uint128_t(space_diff) * params->space_diff_constant)) * ((2 * ksize) + 1);

	auto value = quality.to_uint256();
	if(hard_fork) {
		value >>= params->post_filter;		// compensate for post filter
	}
	return (value >> (ksize - 1)) < threshold;
}

inline
uint16_t get_proof_score(const hash_t& proof_hash)
{
	return proof_hash.bytes[0];
}

inline
uint64_t get_block_iters(std::shared_ptr<const ChainParams> params, const uint64_t time_diff)
{
	return (time_diff / params->time_diff_divider) * params->time_diff_constant;
}

inline
hash_t calc_proof_hash(const hash_t& challenge, const std::vector<uint32_t>& proof_xs)
{
	auto tmp = proof_xs;
	for(auto& x : tmp) {
		x = vnx::to_little_endian(x);
	}
	// proof needs to be hashed after challenge, otherwise compression to 256-bit is possible
	return hash_t(challenge + hash_t(tmp.data(), tmp.size() * 4));
}

inline
uint64_t calc_project_reward(std::shared_ptr<const ChainParams> params, const uint64_t tx_fees)
{
	const auto dynamic = (params->project_ratio.value * uint64_t(tx_fees)) / params->project_ratio.inverse;
	return std::min(params->fixed_project_reward + dynamic, tx_fees / 2);
}

inline
uint64_t calc_min_reward_deduction(std::shared_ptr<const ChainParams> params, const uint64_t txfee_buffer)
{
	const uint64_t divider = 8640;
	const uint64_t min_deduction = 1000;
	return std::min(std::max(txfee_buffer / divider, min_deduction), txfee_buffer);
}

inline
uint64_t calc_final_block_reward(std::shared_ptr<const ChainParams> params, const uint64_t reward, const uint64_t tx_fees)
{
	const auto fee_burn = tx_fees / 2;
	const auto fee_deduction = calc_project_reward(params, tx_fees);
	return reward + (tx_fees - std::max(fee_burn, fee_deduction));
}

inline
uint64_t calc_new_base_reward(std::shared_ptr<const ChainParams> params, std::shared_ptr<const BlockHeader> prev)
{
	const auto& base_reward = prev->base_reward;
	const auto& vote_sum = prev->reward_vote_sum;

	if(prev->reward_vote_count < params->reward_adjust_interval / 2) {
		return base_reward;
	}
	const auto step_size = std::max<int64_t>(base_reward / params->reward_adjust_div, params->reward_adjust_tick);

	int64_t reward = base_reward;
	if(vote_sum > 0) {
		reward += step_size;
	} else if(vote_sum < 0) {
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
uint64_t calc_new_space_diff(std::shared_ptr<const ChainParams> params, std::shared_ptr<const BlockHeader> prev)
{
	const uint64_t diff = prev->space_diff;
	if(diff >> 48) {
		return diff - (diff / 1024);		// clamp to 48 bits (should never happen)
	}
	if(prev->space_fork_len == 0) {
		return prev->space_diff;			// should only happen at genesis
	}
	const uint32_t expected_count = prev->space_fork_len * params->avg_proof_count;

	const uint64_t new_diff = (uint128_t(diff) * prev->space_fork_proofs) / expected_count;

	int64_t delta = new_diff - diff;
	delta /= std::max((16 * params->challenge_interval) / prev->space_fork_len, 1u);

	if(delta == 0) {
		delta = (prev->space_fork_proofs > expected_count ? 1 : -1);
	}
	return std::max<int64_t>(diff + delta, 1);
}

inline
uint64_t calc_new_netspace_ratio(std::shared_ptr<const ChainParams> params, const uint64_t prev_ratio, const bool is_og_proof)
{
	const uint64_t value = is_og_proof ? uint64_t(1) << (2 * params->max_diff_adjust) : 0;
	return (prev_ratio * ((uint64_t(1) << params->max_diff_adjust) - 1) + value) >> params->max_diff_adjust;
}

inline
uint64_t calc_new_txfee_buffer(std::shared_ptr<const ChainParams> params, std::shared_ptr<const BlockHeader> prev)
{
	return (prev->txfee_buffer - calc_min_reward_deduction(params, prev->txfee_buffer)) + prev->tx_fees;
}

inline
uint128_t calc_block_weight(std::shared_ptr<const ChainParams> params,
							std::shared_ptr<const BlockHeader> block, std::shared_ptr<const BlockHeader> prev)
{
	if(block->proof.empty()) {
		return 0;
	}
	const auto num_iters = (block->vdf_iters - prev->vdf_iters) / block->vdf_count;
	const auto time_diff = num_iters / params->time_diff_constant;
	return uint128_t(time_diff) * block->proof[0]->difficulty * params->avg_proof_count;
}

inline
uint64_t get_vdf_speed(std::shared_ptr<const ChainParams> params, const uint64_t time_diff)
{
	return (uint128_t(time_diff) * params->time_diff_constant * 1000) / params->time_diff_divider / params->block_interval_ms;
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

inline int64_t get_time_sec() {
	return vnx::get_wall_time_seconds();
}

inline int64_t get_time_ms() {
	return vnx::get_wall_time_millis();
}

inline int64_t get_time_us() {
	return vnx::get_wall_time_micros();
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
