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


} // solution
} // mmx
