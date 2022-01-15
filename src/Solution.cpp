/*
 * Solution.cpp
 *
 *  Created on: Jan 15, 2022
 *      Author: mad
 */

#include <mmx/Solution.hxx>


namespace mmx {

uint64_t Solution::calc_min_fee(std::shared_ptr<const ChainParams> params) const {
	return 8 * params->min_txfee_byte;
}


} // mmx
