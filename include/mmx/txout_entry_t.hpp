/*
 * txout_entry_t.hpp
 *
 *  Created on: Apr 26, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_TXOUT_ENTRY_T_HPP_
#define INCLUDE_MMX_TXOUT_ENTRY_T_HPP_

#include <mmx/txout_entry_t.hxx>


namespace mmx {

inline
txout_entry_t txout_entry_t::create_ex(const txio_key_t& key, const uint32_t& height, const txout_t& out)
{
	txout_entry_t entry;
	entry.key = key;
	entry.height = height;
	entry.txout_t::operator=(out);
	return entry;
}


} // mmx

#endif /* INCLUDE_MMX_TXOUT_ENTRY_T_HPP_ */
