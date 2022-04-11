/*
 * PlotNFT.cpp
 *
 *  Created on: Mar 14, 2022
 *      Author: mad
 */

#include <mmx/solution/PlotNFT.hxx>


namespace mmx {
namespace solution {

vnx::bool_t PlotNFT::is_valid() const
{
	return target && target->is_valid();
}

uint64_t PlotNFT::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	return target ? target->calc_cost(params) : 0;
}


} // solution
} // mmx
