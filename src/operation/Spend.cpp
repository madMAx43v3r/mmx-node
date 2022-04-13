/*
 * Spend.cpp
 *
 *  Created on: Jan 18, 2022
 *      Author: mad
 */

#include <mmx/operation/Spend.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace operation {

hash_t Spend::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", version);
	write_field(out, "address", address);
	write_field(out, "key", 	key);
	write_field(out, "utxo", 	utxo);
	out.flush();

	return hash_t(buffer);
}

uint64_t Spend::calc_cost(std::shared_ptr<const ChainParams> params) const {
	return (8 + 4 + 32 + 40 + (32 * 2 + 8 + 4)) * params->min_txfee_byte;
}


} // operation
} // mmx
