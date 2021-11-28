/*
 * secp256k1.hpp
 *
 *  Created on: Nov 28, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_SECP256K1_HPP_
#define INCLUDE_MMX_SECP256K1_HPP_

#include <secp256k1.h>


namespace mmx {

extern const secp256k1_context* g_secp256k1;

void secp256k1_init();

void secp256k1_free();


} // mmx

#endif /* INCLUDE_MMX_SECP256K1_HPP_ */
