/*
 * tx_entry_t.hpp
 *
 *  Created on: Oct 19, 2024
 *      Author: mad
 */

#ifndef INCLUDE_MMX_TX_ENTRY_T_HPP_
#define INCLUDE_MMX_TX_ENTRY_T_HPP_

#include <mmx/tx_entry_t.hxx>


namespace mmx {

inline
tx_entry_t tx_entry_t::create_ex(const txio_entry_t& entry, const bool& valid)
{
	tx_entry_t out;
	out.txio_entry_t::operator=(entry);
	out.is_validated = valid;
	return out;
}


} // mmx

#endif /* INCLUDE_MMX_TX_ENTRY_T_HPP_ */
