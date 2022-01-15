/*
 * MultiSig.cpp
 *
 *  Created on: Jan 15, 2022
 *      Author: mad
 */

#include <mmx/solution/MultiSig.hxx>


namespace mmx {
namespace solution {

uint64_t MultiSig::calc_min_fee(std::shared_ptr<const ChainParams> params) const
{
	uint64_t sum = 0;
	for(const auto& sol : solutions) {
		sum += sol->calc_min_fee(params);
	}
	return sum;
}


} // solution
} // mmx
