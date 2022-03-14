/*
 * DataObject.cpp
 *
 *  Created on: Mar 13, 2022
 *      Author: mad
 */

#include <mmx/contract/DataObject.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace contract {

vnx::bool_t DataObject::is_valid() const
{
	return Contract::is_valid() && num_bytes() <= MAX_BYTES;
}

hash_t DataObject::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_bytes(out, version);
	write_bytes(out, owner);
	for(const auto& entry : data.field) {
		write_bytes(out, entry.first);
		write_bytes(out, entry.second);
	}
	out.flush();

	return hash_t(buffer);
}

uint64_t DataObject::num_bytes() const
{
	uint64_t num_bytes = 0;
	for(const auto& entry : data.field) {
		num_bytes += entry.first.size();
		num_bytes += entry.second.size();
	}
	return num_bytes;
}

uint64_t DataObject::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	return (8 + 4 + (owner ? 32 : 0) + num_bytes()) * params->min_txfee_byte;
}

std::vector<addr_t> DataObject::get_dependency() const {
	if(owner) {
		return {*owner};
	}
	return {};
}

std::vector<addr_t> DataObject::get_parties() const {
	return get_dependency();
}

vnx::optional<addr_t> DataObject::get_owner() const {
	return owner;
}

std::vector<tx_out_t> DataObject::validate(std::shared_ptr<const Operation> operation, std::shared_ptr<const Context> context) const
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

void DataObject::transfer(const vnx::optional<addr_t>& new_owner)
{
	owner = new_owner;
}

void DataObject::set(const std::string& key, const vnx::Variant& value)
{
	data[key] = value;
}

void DataObject::erase(const std::string& key)
{
	data.erase(key);
}

void DataObject::clear()
{
	data.clear();
}


} // contract
} // mmx
