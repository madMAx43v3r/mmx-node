/*
 * Contract.cpp
 *
 *  Created on: Nov 27, 2021
 *      Author: mad
 */

#include <mmx/Contract.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

vnx::bool_t Contract::is_valid() const
{
	return version == 0;
}

hash_t Contract::calc_hash(const vnx::bool_t& full_hash) const
{
	throw std::logic_error("not implemented");
}

uint64_t Contract::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	throw std::logic_error("not implemented");
}

std::vector<addr_t> Contract::get_dependency() const {
	return {};
}

vnx::optional<addr_t> Contract::get_owner() const {
	return nullptr;
}

vnx::bool_t Contract::is_locked(std::shared_ptr<const Context> context) const {
	return !get_owner();
}

std::vector<txout_t> Contract::validate(std::shared_ptr<const Operation> operation, std::shared_ptr<const Context> context) const
{
	throw std::logic_error("invalid operation");
}

void Contract::transfer(const vnx::optional<addr_t>& new_owner)
{
	throw std::logic_error("invalid operation");
}


} // mmx
