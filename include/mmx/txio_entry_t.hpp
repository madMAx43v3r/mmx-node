/*
 * txio_entry_t.hpp
 *
 *  Created on: Apr 26, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_TXIO_ENTRY_T_HPP_
#define INCLUDE_MMX_TXIO_ENTRY_T_HPP_

#include <mmx/txio_entry_t.hxx>


namespace mmx {

inline
txio_entry_t txio_entry_t::create_ex(const hash_t& txid, const uint32_t& height, const int64_t& time_stamp, const tx_type_e& type, const txio_t& txio)
{
	txio_entry_t out;
	out.txid = txid;
	out.height = height;
	out.time_stamp = time_stamp;
	out.type = type;
	out.txio_t::operator=(txio);
	return out;
}


} // mmx

#endif /* INCLUDE_MMX_TXIO_ENTRY_T_HPP_ */
