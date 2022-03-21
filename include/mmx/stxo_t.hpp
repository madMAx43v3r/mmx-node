/*
 * stxo_t.h
 *
 *  Created on: Jan 13, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_STXO_T_HPP_
#define INCLUDE_MMX_STXO_T_HPP_

#include <mmx/stxo_t.hxx>


namespace mmx {

inline
stxo_t stxo_t::create_ex(const utxo_t& utxo, const uint32_t& height, const txio_key_t& spent)
{
	stxo_t stxo;
	stxo.utxo_t::operator=(utxo);
	stxo.spent_height = height;
	stxo.spent_key = spent;
	return stxo;
}


} // mmx

#endif /* INCLUDE_MMX_STXO_T_HPP_ */
