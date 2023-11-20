/*
 * encoding.h
 *
 *  Created on: Nov 20, 2023
 *      Author: mad
 */

#ifndef INCLUDE_MMX_POS_ENCODING_H_
#define INCLUDE_MMX_POS_ENCODING_H_

#include <vector>
#include <cstdint>
#include <utility>
#include <stdexcept>


namespace mmx {
namespace pos {

std::vector<uint64_t> encode(const std::vector<uint8_t>& symbols, uint64_t& total_bits);

std::vector<uint8_t> decode(const std::vector<uint64_t>& bit_stream, const uint64_t num_symbols);



} // pos
} // mmx

#endif /* INCLUDE_MMX_POS_ENCODING_H_ */
