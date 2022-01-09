/*
 * Transaction.cpp
 *
 *  Created on: Nov 27, 2021
 *      Author: mad
 */

#include <mmx/Transaction.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

hash_t TransactionBase::calc_hash() const
{
	return id;
}

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

std::shared_ptr<const Solution> Transaction::get_solution(const uint32_t& index) const
{
	if(index < solutions.size()) {
		return solutions[index];
	}
	return nullptr;
}

uint64_t Transaction::calc_min_fee(std::shared_ptr<const ChainParams> params) const
{
	if(!params) {
		throw std::logic_error("!params");
	}
	return (inputs.size() + outputs.size()) * params->min_txfee_io
			+ solutions.size() * params->min_txfee_sign
			+ execute.size() * params->min_txfee_exec;
}


} // mmx
