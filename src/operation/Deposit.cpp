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
	return Super::is_valid() && amount > 0;
}

hash_t Deposit::calc_hash(const vnx::bool_t& full_hash) const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", 	version);
	write_field(out, "address", 	address);
	write_field(out, "user", 		user);
	write_field(out, "method", 		method);
	write_field(out, "args", 		args);
	write_field(out, "currency", 	currency);
	write_field(out, "amount", 		amount);
	write_field(out, "sender", 		sender);

	if(full_hash) {
		write_field(out, "solution", solution ? solution->calc_hash() : hash_t());
	}
	out.flush();

	return hash_t(buffer);
}


} // operation
} // mmx
