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

vnx::bool_t NFT::is_valid() const {
	return Contract::is_valid() && creator != hash_t();
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

	return hash_t(hash_t(buffer).bytes);
}

uint64_t NFT::calc_cost(std::shared_ptr<const ChainParams> params) const {
	uint32_t payload = 0;
	for(const auto& entry : data.field) {
		payload += entry.first.size();
		payload += entry.second.size();
	}
	return (8 + 4 + 32 + (parent ? 32 : 0) + payload) * params->min_txfee_byte;
}


} // contract
} // mmx
