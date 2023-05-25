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

uint64_t Contract::calc_cost(std::shared_ptr<const ChainParams> params, const vnx::bool_t& is_read) const
{
	throw std::logic_error("not implemented");
}

vnx::optional<addr_t> Contract::get_owner() const {
	return nullptr;
}

vnx::bool_t Contract::is_locked(const uint32_t& height) const
{
	return !get_owner();
}

void Contract::validate(std::shared_ptr<const Operation> operation, const hash_t& txid) const
{
	throw std::logic_error("invalid operation");
}

vnx::Variant Contract::read_field(const std::string& name) const
{
	if(name == "__type") {
		return vnx::Variant(get_type_name());
	}
	return get_field(name);
}


} // mmx
