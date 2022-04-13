/*
 * PuzzleTimeLock.cpp
 *
 *  Created on: Mar 13, 2022
 *      Author: mad
 */

#include <mmx/solution/PuzzleTimeLock.hxx>


namespace mmx {
namespace solution {

vnx::bool_t PuzzleTimeLock::is_valid() const
{
	return Super::is_valid() && puzzle && puzzle->is_valid() && target && target->is_valid();
}

uint64_t PuzzleTimeLock::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	return (puzzle ? puzzle->calc_cost(params) : 0) + (target ? target->calc_cost(params) : 0);
}


} // solution
} // mmx
