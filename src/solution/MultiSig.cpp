/*
 * MultiSig.cpp
 *
 *  Created on: Jan 15, 2022
 *      Author: mad
 */

#include <mmx/solution/MultiSig.hxx>


namespace mmx {
namespace solution {

vnx::bool_t MultiSig::is_valid() const
{
	size_t count = 0;
	for(const auto& sol : solutions) {
		if(sol) {
			count++;
		}
	}
	return count <= MAX_SIGNATURES;
}

// TODO: cost per signature


} // solution
} // mmx
