/*
 * TokenBase.cpp
 *
 *  Created on: Apr 25, 2022
 *      Author: mad
 */

#include <mmx/contract/TokenBase.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace contract {

vnx::bool_t TokenBase::is_valid() const
{
	return Super::is_valid() && name.size() <= 64 && symbol.size() <= 6
			&& symbol != "MMX" && symbol.find_first_of(" \n\t\r\b\f") == std::string::npos
			&& decimals >= 0 && decimals <= 12;
}

hash_t TokenBase::calc_hash(const vnx::bool_t& full_hash) const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", 	version);
	write_field(out, "name", 		name);
	write_field(out, "symbol", 		symbol);
	write_field(out, "decimals", 	decimals);
	write_field(out, "meta_data", 	meta_data);
	out.flush();

	return hash_t(buffer);
}

uint64_t TokenBase::calc_cost(std::shared_ptr<const ChainParams> params, const vnx::bool_t& is_read) const
{
	const auto num_bytes = 4 + 8 + name.size() + symbol.size() + (meta_data ? 32 : 0);

	return num_bytes * (is_read ? params->min_txfee_read_byte : params->min_txfee_byte);
}


} // contract
} // mmx
