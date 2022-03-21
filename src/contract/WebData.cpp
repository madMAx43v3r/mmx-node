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

vnx::bool_t WebData::is_valid() const {
	return Contract::is_valid() && !mime_type.empty();
}

hash_t WebData::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_bytes(out, version);
	write_bytes(out, mime_type);
	write_bytes(out, payload);
	out.flush();

	return hash_t(buffer);
}

uint64_t WebData::calc_cost(std::shared_ptr<const ChainParams> params) const {
	return (8 + 4 + mime_type.size() + payload.size()) * params->min_txfee_byte;
}


} // contract
} // mmx
