/*
 * NFT.cpp
 *
 *  Created on: Jan 18, 2022
 *      Author: mad
 */

#include <mmx/contract/NFT.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace contract {

vnx::bool_t NFT::is_valid() const
{
	return Super::is_valid() && creator != hash_t();
}

hash_t NFT::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", version);
	write_field(out, "creator", creator);
	write_field(out, "parent", 	parent);
	write_field(out, "data", 	data);
	out.flush();

	return hash_t(buffer);
}

uint64_t NFT::num_bytes() const
{
	uint64_t num_bytes = 0;
	for(const auto& entry : data.field) {
		num_bytes += entry.first.size();
		num_bytes += entry.second.size();
	}
	return num_bytes;
}

uint64_t NFT::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	return num_bytes() * params->min_txfee_byte;
}


} // contract
} // mmx
