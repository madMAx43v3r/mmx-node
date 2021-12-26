/*
 * utxo_entry_t.hpp
 *
 *  Created on: Dec 26, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_UTXO_ENTRY_T_HPP_
#define INCLUDE_MMX_UTXO_ENTRY_T_HPP_

#include <mmx/utxo_entry_t.hxx>


namespace mmx {

inline
utxo_entry_t utxo_entry_t::create_ex(const txio_key_t& key, const utxo_t& output)
{
	utxo_entry_t res;
	res.key = key;
	res.output = output;
	return res;
}


} // mmx

#endif /* INCLUDE_MMX_UTXO_ENTRY_T_HPP_ */
