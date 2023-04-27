/*
 * Solution.cpp
 *
 *  Created on: Jan 15, 2022
 *      Author: mad
 */

#include <mmx/Solution.hxx>


namespace mmx {

vnx::bool_t Solution::is_valid() const
{
	return version == 0;
}

hash_t Solution::calc_hash() const
{
	throw std::logic_error("not implemented");
}

uint64_t Solution::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	throw std::logic_error("not implemented");
}


} // mmx
