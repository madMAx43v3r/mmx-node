/*
 * mem_hash.h
 *
 *  Created on: Oct 29, 2023
 *      Author: mad
 */

#ifndef INCLUDE_MMX_POS_MEM_HASH_H_
#define INCLUDE_MMX_POS_MEM_HASH_H_

#include <mmx/pos/util.h>


namespace mmx {
namespace pos {

/*
 * mem = array of size mem_size
 * key = array of size 32
 */
void gen_mem_array(uint32_t* mem, const uint8_t* key, const uint64_t mem_size);

/*
 * M = log2 number of iterations
 * mem = array of size (16 << B)
 * hash = array of size 64
 */
void calc_mem_hash(uint32_t* mem, uint8_t* hash, const int M, const int B);


} // pos
} // mmx

#endif /* INCLUDE_MMX_POS_MEM_HASH_H_ */
