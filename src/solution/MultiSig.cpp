/*
 * MultiSig.cpp
 *
 *  Created on: Jan 15, 2022
 *      Author: mad
 */

#include <mmx/solution/MultiSig.hxx>


namespace mmx {
namespace solution {

vnx::bool_t MultiSig::is_valid() const {
	return true;
}

uint64_t MultiSig::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	uint64_t cost = 0;
	for(const auto& sol : solutions) {
		if(sol) {
			cost += sol->calc_cost(params);
		}
	}
	return cost;
}


} // solution
} // mmx
