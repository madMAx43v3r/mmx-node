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
txio_entry_t txio_entry_t::create_ex(const hash_t& txid, const uint32_t& height, const tx_type_e& type, const txio_t& txio)
{
	txio_entry_t entry;
	entry.txid = txid;
	entry.height = height;
	entry.type = type;
	entry.txio_t::operator=(txio);
	return entry;
}


} // mmx

#endif /* INCLUDE_MMX_TXIO_ENTRY_T_HPP_ */
