/*
 * verify.h
 *
 *  Created on: Nov 5, 2023
 *      Author: mad
 */

#ifndef INCLUDE_MMX_POS_VERIFY_H_
#define INCLUDE_MMX_POS_VERIFY_H_

#include <mmx/hash_t.hpp>
#include <mmx/pos/config.h>
#include <vector>


namespace mmx {
namespace pos {

void set_remote_compute(bool enable);

hash_t calc_quality(const hash_t& challenge, const bytes_t<META_BYTES_OUT>& meta);

bool check_post_filter(const hash_t& challenge, const bytes_t<META_BYTES_OUT>& meta, const int post_filter);

std::vector<std::pair<uint32_t, bytes_t<META_BYTES_OUT>>>
compute(const std::vector<uint32_t>& X_values, std::vector<uint32_t>* X_out,
		const hash_t& id, const int ksize, const int xbits);

std::vector<std::pair<uint32_t, bytes_t<META_BYTES_OUT>>>
compute_full(	const std::vector<uint32_t>& X_in,
				const std::vector<uint32_t>& Y_in,
				std::vector<std::array<uint32_t, N_META>>& M_in,
				std::vector<uint32_t>* X_out,
				const hash_t& id, const int ksize);

hash_t verify(	const std::vector<uint32_t>& X_values, const hash_t& challenge, const hash_t& id,
				const int plot_filter, const int post_filter, const int ksize, const bool hard_fork);


} // pos
} // mmx

#endif /* INCLUDE_MMX_POS_VERIFY_H_ */
