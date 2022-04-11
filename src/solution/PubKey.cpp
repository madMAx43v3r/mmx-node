/*
 * PubKey.cpp
 *
 *  Created on: Jan 15, 2022
 *      Author: mad
 */

#include <mmx/solution/PubKey.hxx>


namespace mmx {
namespace solution {

vnx::bool_t PubKey::is_valid() const
{
	return true;
}

uint64_t PubKey::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	return params->min_txfee_sign;
}


} // solution
} // mmx
