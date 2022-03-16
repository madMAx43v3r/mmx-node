/*
 * Data.cpp
 *
 *  Created on: Jan 19, 2022
 *      Author: mad
 */

#include <mmx/contract/Data.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace contract {

hash_t Data::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_bytes(out, version);
	write_bytes(out, owner);
	write_bytes(out, value);
	out.flush();

	return hash_t(buffer);
}

uint64_t Data::calc_cost(std::shared_ptr<const ChainParams> params) const {
	return (8 + 4 + (owner ? 32 : 0) + value.size()) * params->min_txfee_byte;
}

std::vector<addr_t> Data::get_dependency() const {
	if(owner) {
		return {*owner};
	}
	return {};
}

std::vector<addr_t> Data::get_parties() const {
	return get_dependency();
}

vnx::optional<addr_t> Data::get_owner() const {
	return owner;
}

std::vector<tx_out_t> Data::validate(std::shared_ptr<const Operation> operation, std::shared_ptr<const Context> context) const
{
	if(!owner) {
		throw std::logic_error("!owner");
	}
	{
		auto contract = context->get_contract(*owner);
		if(!contract) {
			throw std::logic_error("missing dependency");
		}
		contract->validate(operation, context);
	}
	return {};
}

void Data::transfer(const vnx::optional<addr_t>& new_owner)
{
	owner = new_owner;
}

void Data::set(const vnx::Variant& value_)
{
	value = value_;
}


} // contract
} // mmx
