/*
 * BLS_PubKey.cpp
 *
 *  Created on: Mar 23, 2022
 *      Author: mad
 */

#include <mmx/solution/BLS_PubKey.hxx>


namespace mmx {
namespace solution {

vnx::bool_t BLS_PubKey::is_valid() const
{
	return true;
}

uint64_t BLS_PubKey::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	return params->min_txfee_sign;
}


} // solution
} // mmx

