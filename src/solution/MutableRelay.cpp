/*
 * MutableRelay.cpp
 *
 *  Created on: Mar 14, 2022
 *      Author: mad
 */

#include <mmx/solution/MutableRelay.hxx>


namespace mmx {
namespace solution {

vnx::bool_t MutableRelay::is_valid() const
{
	return Super::is_valid() && target && target->is_valid();
}

uint64_t MutableRelay::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	return target ? target->calc_cost(params) : 0;
}


} // solution
} // mmx
