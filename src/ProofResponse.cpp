/*
 * ProofResponse.cpp
 *
 *  Created on: Apr 1, 2022
 *      Author: mad
 */

#include <mmx/ProofResponse.hxx>


namespace mmx {

bool ProofResponse::is_valid() const
{
	return request && proof && proof->is_valid();
}

void ProofResponse::validate() const
{
	// TODO: validate farmer_sig
}


} // mmx
