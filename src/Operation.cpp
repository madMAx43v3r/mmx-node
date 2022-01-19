/*
 * Operation.cpp
 *
 *  Created on: Nov 27, 2021
 *      Author: mad
 */

#include <mmx/Operation.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

vnx::bool_t Operation::is_valid() const {
	return version == 0;
}

hash_t Operation::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_bytes(out, version);
	write_bytes(out, address);
	out.flush();

	return hash_t(buffer);
}

uint64_t Operation::calc_min_fee(std::shared_ptr<const ChainParams> params) const {
	return (8 + 4 + 32) * params->min_txfee_byte;
}

} // mmx
