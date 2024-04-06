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
 * key = 64 bytes
 */
void gen_mem_array(uint32_t* mem, const uint8_t* key, const uint32_t mem_size);

/*
 * mem = array of size 1024
 * hash = array of size 128
 */
void calc_mem_hash(uint32_t* mem, uint8_t* hash, const int num_iter);


} // pos
} // mmx

#endif /* INCLUDE_MMX_POS_MEM_HASH_H_ */
