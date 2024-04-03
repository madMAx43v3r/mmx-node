/*
 * verify.h
 *
 *  Created on: Nov 5, 2023
 *      Author: mad
 */

#ifndef INCLUDE_MMX_POS_VERIFY_H_
#define INCLUDE_MMX_POS_VERIFY_H_

#include <mmx/hash_t.hpp>
#include <vector>


namespace mmx {
namespace pos {

static constexpr int N_META = 14;
static constexpr int N_META_OUT = 12;
static constexpr int N_TABLE = 9;

static constexpr int META_BYTES = N_META * 4;
static constexpr int META_BYTES_OUT = N_META_OUT * 4;


std::vector<std::pair<uint32_t, bytes_t<META_BYTES_OUT>>>
compute(const std::vector<uint32_t>& X_values, std::vector<uint32_t>* X_out,
		const hash_t& id, const int ksize, const int xbits);

hash_t verify(	const std::vector<uint32_t>& X_values, const hash_t& challenge,
				const hash_t& id, const int plot_filter, const int ksize);


} // pos
} // mmx

#endif /* INCLUDE_MMX_POS_VERIFY_H_ */
