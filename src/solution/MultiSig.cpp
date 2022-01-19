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
	uint64_t sum = (8 + 4) * params->min_txfee_byte;

	for(const auto& sol : solutions) {
		if(sol) {
			sum += sol->calc_min_fee(params);
		} else {
			sum += 2;
		}
	}
	return sum;
}


} // solution
} // mmx
