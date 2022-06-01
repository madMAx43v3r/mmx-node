/*
 * Execute.cpp
 *
 *  Created on: May 9, 2022
 *      Author: mad
 */

#include <mmx/operation/Execute.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace operation {

vnx::bool_t Execute::is_valid() const
{
	return Super::is_valid() && !method.empty();
}

hash_t Execute::calc_hash(const vnx::bool_t& full_hash) const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", version);
	write_field(out, "address", address);
	write_field(out, "method", 	method);
	write_field(out, "args", 	args);
	write_field(out, "user", 	user);

	if(full_hash) {
		write_field(out, "solution", solution ? solution->calc_hash() : hash_t());
	}
	out.flush();

	return hash_t(buffer);
}

uint64_t Execute::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	uint64_t payload = 0;
	for(const auto& arg : args) {
		payload += arg.size();
	}
	return Super::calc_cost(params) + payload * params->min_txfee_byte;
}


} // operation
} // mmx
