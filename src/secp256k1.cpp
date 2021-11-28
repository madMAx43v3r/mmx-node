/*
 * secp256k1.cpp
 *
 *  Created on: Nov 28, 2021
 *      Author: mad
 */

#include <mmx/secp256k1.hpp>


namespace mmx {

const secp256k1_context* g_secp256k1 = nullptr;

static secp256k1_context* g_secp256k1_priv = nullptr;


void secp256k1_init()
{
	g_secp256k1_priv = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
	g_secp256k1 = g_secp256k1_priv;
}

void secp256k1_free()
{
	g_secp256k1 = nullptr;
	secp256k1_context_destroy(g_secp256k1_priv);
	g_secp256k1_priv = nullptr;
}


} // mmx
