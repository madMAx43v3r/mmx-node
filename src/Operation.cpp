/*
 * Operation.cpp
 *
 *  Created on: Nov 27, 2021
 *      Author: mad
 */

#include <mmx/Operation.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

vnx::bool_t Operation::is_valid() const
{
	return version == 0;
}

hash_t Operation::calc_hash(const vnx::bool_t& full_hash) const
{
	throw std::logic_error("not implemented");
}

uint64_t Operation::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	return (solution ? solution->calc_cost(params) : 0);
}

} // mmx
