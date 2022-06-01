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
	return Super::is_valid() && target != hash_t() && amount > 0 && amount <= MAX_AMOUNT;
}

hash_t Mint::calc_hash(const vnx::bool_t& full_hash) const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", version);
	write_field(out, "address", address);
	write_field(out, "target", 	target);
	write_field(out, "amount", 	amount);

	if(full_hash) {
		write_field(out, "solution", solution ? solution->calc_hash() : hash_t());
	}
	out.flush();

	return hash_t(buffer);
}


} // operation
} // mmx
