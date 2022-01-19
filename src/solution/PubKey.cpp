/*
 * PubKey.cpp
 *
 *  Created on: Jan 15, 2022
 *      Author: mad
 */

#include <mmx/solution/PubKey.hxx>


namespace mmx {
namespace solution {

uint64_t PubKey::calc_min_fee(std::shared_ptr<const ChainParams> params) const {
	return params->min_txfee_sign;
}


} // solution
} // mmx
