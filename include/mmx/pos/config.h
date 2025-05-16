/*
 * config.h
 *
 *  Created on: Feb 2, 2025
 *      Author: mad
 */

#ifndef INCLUDE_MMX_POS_CONFIG_H_
#define INCLUDE_MMX_POS_CONFIG_H_

#include <array>
#include <cstdint>

#ifdef MMX_POS_EXPORT_ENABLE
#include <mmx_pos_export.h>
#else
#ifndef MMX_POS_EXPORT
#define MMX_POS_EXPORT
#endif
#endif


namespace mmx {
namespace pos {

static constexpr int N_META = 14;
static constexpr int N_META_OUT = 12;
static constexpr int N_TABLE = 9;

static constexpr int META_BYTES = N_META * 4;
static constexpr int META_BYTES_OUT = N_META_OUT * 4;

static constexpr int MEM_HASH_ITER = 256;


} // pos
} // mmx

#endif /* INCLUDE_MMX_POS_CONFIG_H_ */
