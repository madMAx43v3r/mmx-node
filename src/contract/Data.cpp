/*
 * Data.cpp
 *
 *  Created on: Jan 19, 2022
 *      Author: mad
 */

#include <mmx/contract/Data.hxx>
#include <mmx/write_bytes.h>
#include <mmx/utils.h>


namespace mmx {
namespace contract {

vnx::bool_t Data::is_valid() const
{
	return Super::is_valid() && is_json(value);
}

hash_t Data::calc_hash(const vnx::bool_t& full_hash, const uint32_t& hash_version) const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	WriteBytes out(&stream, hash_version);

	write_bytes(out, get_type_hash());
	write_field(out, "version", version);
	write_field(out, "value", 	value);
	out.flush();

	return hash_t(buffer);
}

uint64_t Data::num_bytes() const
{
	return Super::num_bytes() + get_num_bytes(value);
}


} // contract
} // mmx
