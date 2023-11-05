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

std::vector<std::pair<uint32_t, bytes_t<48>>>
compute(const std::vector<uint32_t>& X_values, std::vector<uint32_t>* X_out,
		const uint8_t* id, const int ksize, const int xbits);

hash_t verify(const std::vector<uint32_t>& X_values, const hash_t& challenge, const uint8_t* id, const int ksize);


} // pos
} // mmx

#endif /* INCLUDE_MMX_POS_VERIFY_H_ */
