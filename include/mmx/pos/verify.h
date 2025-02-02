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

hash_t calc_quality(const hash_t& challenge, const bytes_t<META_BYTES_OUT>& meta);

std::vector<std::pair<uint32_t, bytes_t<META_BYTES_OUT>>>
compute(const std::vector<uint32_t>& X_values, std::vector<uint32_t>* X_out,
		const hash_t& id, const int ksize, const int xbits);

hash_t verify(	const std::vector<uint32_t>& X_values, const hash_t& challenge,
				const hash_t& id, const int plot_filter, const int ksize);


} // pos
} // mmx

#endif /* INCLUDE_MMX_POS_VERIFY_H_ */
