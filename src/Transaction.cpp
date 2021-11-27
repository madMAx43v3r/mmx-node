/*
 * Transaction.cpp
 *
 *  Created on: Nov 27, 2021
 *      Author: mad
 */

#include <mmx/Transaction.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

hash_t Transaction::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	buffer.reserve(16 * 1024);

	mmx::write_bytes(out, version);

	for(const auto& tx : inputs) {
		mmx::write_bytes(out, tx);
	}
	for(const auto& tx : outputs) {
		mmx::write_bytes(out, tx);
	}
	for(const auto& op : execute) {
		if(op) {
			mmx::write_bytes(out, op->calc_hash());
		}
	}
	out.flush();

	return hash_t(buffer.data(), buffer.size());
}


} // mmx
