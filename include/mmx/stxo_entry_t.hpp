/*
 * stxo_entry_t.hpp
 *
 *  Created on: Dec 26, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_STXO_ENTRY_T_HPP_
#define INCLUDE_MMX_STXO_ENTRY_T_HPP_

#include <mmx/stxo_entry_t.hxx>


namespace mmx {

inline
stxo_entry_t stxo_entry_t::create_ex(const txio_key_t& key, const utxo_t& output, const txio_key_t& spent)
{
	stxo_entry_t res;
	res.key = key;
	res.output = output;
	res.spent = spent;
	return res;
}


} // mmx

#endif /* INCLUDE_MMX_STXO_ENTRY_T_HPP_ */
