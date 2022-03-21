/*
 * PuzzleLock.cpp
 *
 *  Created on: Mar 13, 2022
 *      Author: mad
 */

#include <mmx/solution/PuzzleLock.hxx>


namespace mmx {
namespace solution {

vnx::bool_t PuzzleLock::is_valid() const
{
	return (!puzzle || puzzle->is_valid()) && (!target || target->is_valid());
}


} // solution
} // mmx
