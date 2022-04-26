/*
 * Deposit.cpp
 *
 *  Created on: Apr 26, 2022
 *      Author: mad
 */

#include <mmx/operation/Deposit.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace operation {

vnx::bool_t Deposit::is_valid() const
{
	return Super::is_valid() && input.amount > 0;
}

hash_t Deposit::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", version);
	write_field(out, "address", address);
	write_field(out, "input", 	input);
	out.flush();

	return hash_t(buffer);
}


} // operation
} // mmx
