/*
 * Data.cpp
 *
 *  Created on: Jan 19, 2022
 *      Author: mad
 */

#include <mmx/contract/Data.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace contract {

hash_t Data::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_bytes(out, version);
	write_bytes(out, payload);
	out.flush();

	return hash_t(buffer);
}

uint64_t Data::calc_min_fee(std::shared_ptr<const ChainParams> params) const {
	return (8 + 4 + payload.data.size()) * params->min_txfee_byte;
}


} // contract
} // mmx
