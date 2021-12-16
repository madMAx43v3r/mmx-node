/*
 * utxo_t.hpp
 *
 *  Created on: Dec 16, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_UTXO_T_HPP_
#define INCLUDE_MMX_UTXO_T_HPP_

#include <mmx/utxo_t.hxx>


namespace mmx {

inline
utxo_t utxo_t::create_ex(const tx_out_t& out, const uint32_t& height)
{
	utxo_t utxo;
	utxo.tx_out_t::operator=(out);
	utxo.height = height;
	return utxo;
}


} // mmx

#endif /* INCLUDE_MMX_UTXO_T_HPP_ */
