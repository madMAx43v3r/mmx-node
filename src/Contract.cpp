/*
 * Contract.cpp
 *
 *  Created on: Nov 27, 2021
 *      Author: mad
 */

#include <mmx/Contract.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

vnx::bool_t Contract::is_valid() const {
	return version == 0;
}

hash_t Contract::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_bytes(out, version);
	out.flush();

	return hash_t(buffer);
}

uint64_t Contract::calc_min_fee(std::shared_ptr<const ChainParams> params) const {
	return (8 + 4) * params->min_txfee_byte;
}

std::vector<addr_t> Contract::get_dependency() const {
	return {};
}

vnx::optional<addr_t> Contract::get_owner() const {
	return nullptr;
}

std::vector<tx_out_t> Contract::validate(std::shared_ptr<const Operation> operation, std::shared_ptr<const Context> context) const
{
	throw std::logic_error("invalid operation");
}


} // mmx
