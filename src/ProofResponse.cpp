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
	if(proof) {
		if(!farmer_sig.verify(proof->farmer_key, proof->calc_hash())) {
			throw std::logic_error("invalid farmer signature");
		}
	}
}


} // mmx
