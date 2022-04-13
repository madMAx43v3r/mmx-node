/*
 * Mint.cpp
 *
 *  Created on: Jan 18, 2022
 *      Author: mad
 */

#include <mmx/operation/Mint.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace operation {

vnx::bool_t Mint::is_valid() const
{
	return Super::is_valid() && target != hash_t() && amount > 0 && amount <= max_amount;
}

hash_t Mint::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", version);
	write_field(out, "address", address);
	write_field(out, "target", 	target);
	write_field(out, "amount", 	amount);
	out.flush();

	return hash_t(buffer);
}

uint64_t Mint::calc_cost(std::shared_ptr<const ChainParams> params) const {
	return (8 + 4 + 32 * 2 + 8) * params->min_txfee_byte;
}


} // operation
} // mmx
