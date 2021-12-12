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

uint64_t Transaction::calc_min_fee(std::shared_ptr<const ChainParams> params) const
{
	if(!params) {
		throw std::logic_error("params == nullptr");
	}
	return (inputs.size() + outputs.size()) * params->min_txfee_inout + execute.size() * params->min_txfee_exec;
}

std::map<addr_t, uint64_t> Transaction::get_output_amounts() const
{
	std::map<addr_t, uint64_t> res;
	for(const auto& out : outputs) {
		res[out.contract] += out.amount;
	}
	return res;
}


} // mmx
