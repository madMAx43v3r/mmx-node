/*
 * ProofOfStake.cpp
 *
 *  Created on: Apr 13, 2022
 *      Author: mad
 */

#include <mmx/ProofOfStake.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

vnx::bool_t ProofOfStake::is_valid() const
{
	return false;
}

mmx::hash_t ProofOfStake::calc_hash() const
{
	throw std::logic_error("not implemented");
}

void ProofOfStake::validate() const
{
}


} // mmx
