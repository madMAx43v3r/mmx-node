/*
 * TokenBase.cpp
 *
 *  Created on: Apr 25, 2022
 *      Author: mad
 */

#include <mmx/contract/TokenBase.hxx>
#include <mmx/write_bytes.h>
#include <mmx/utils.h>


namespace mmx {
namespace contract {

vnx::bool_t TokenBase::is_valid() const
{
	return Super::is_valid() && name.size() <= 64 && symbol.size() <= 6
			&& symbol != "MMX" && symbol.find_first_of(" \n\t\r\b\f") == std::string::npos
			&& decimals >= 0 && decimals <= 18
			&& is_json(meta_data);
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

uint64_t TokenBase::num_bytes() const
{
	return Super::num_bytes() + name.size() + symbol.size() + get_num_bytes(meta_data);
}


} // contract
} // mmx
