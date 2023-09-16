/*
 * WebData.cpp
 *
 *  Created on: Jan 19, 2022
 *      Author: mad
 */

#include <mmx/contract/WebData.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace contract {

vnx::bool_t WebData::is_valid() const
{
	return Super::is_valid() && mime_type.size() <= 256;
}

hash_t WebData::calc_hash(const vnx::bool_t& full_hash) const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", 	version);
	write_field(out, "mime_type", 	mime_type);
	write_field(out, "payload", 	payload);
	out.flush();

	return hash_t(buffer);
}

uint64_t WebData::num_bytes() const
{
	return 8 + mime_type.size() + payload.size();
}

uint64_t WebData::calc_cost(std::shared_ptr<const ChainParams> params, const vnx::bool_t& is_read) const
{
	return calc_byte_cost(params, num_bytes(), is_read);
}

vnx::Variant WebData::read_field(const std::string& name) const
{
	if(name == "payload") {
		return vnx::Variant(vnx::to_hex_string(payload.data(), payload.size(), false, false));
	}
	return Super::read_field(name);
}


} // contract
} // mmx
