/*
 * Transaction.cpp
 *
 *  Created on: Nov 27, 2021
 *      Author: mad
 */

#include <mmx/Transaction.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

void Transaction::finalize()
{
	id = calc_hash();
}

vnx::bool_t Transaction::is_valid() const
{
	return calc_hash() == id;
}

hash_t Transaction::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	buffer.reserve(4 * 1024);

	write_bytes(out, version);

	for(const auto& tx : inputs) {
		write_bytes(out, tx);
	}
	for(const auto& tx : outputs) {
		write_bytes(out, tx);
	}
	for(const auto& op : execute) {
		write_bytes(out, op ? op->calc_hash() : hash_t());
	}
	out.flush();

	return hash_t(buffer);
}


} // mmx
