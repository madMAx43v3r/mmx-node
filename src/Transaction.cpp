/*
 * Transaction.cpp
 *
 *  Created on: Nov 27, 2021
 *      Author: mad
 */

#include <mmx/Transaction.hxx>
#include <mmx/solution/PubKey.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

hash_t TransactionBase::calc_hash() const {
	return id;
}

uint64_t TransactionBase::calc_cost(std::shared_ptr<const ChainParams> params) const {
	return 0;
}

void Transaction::finalize() {
	id = calc_hash();
}

vnx::bool_t Transaction::is_valid() const {
	return calc_hash() == id;
}

hash_t Transaction::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	buffer.reserve(4 * 1024);

	// TODO: write_bytes(out, get_type_hash());
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
	write_bytes(out, deploy ? deploy->calc_hash() : hash_t());

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

tx_out_t Transaction::get_output(const uint32_t& index) const
{
	if(index < outputs.size()) {
		return outputs[index];
	}
	if(index >= outputs.size()) {
		const auto offset = index - outputs.size();
		if(offset < exec_outputs.size()) {
			return exec_outputs[offset];
		}
	}
	throw std::logic_error("no such output");
}

std::vector<tx_out_t> Transaction::get_all_outputs() const
{
	auto res = outputs;
	res.insert(res.end(), exec_outputs.begin(), exec_outputs.end());
	return res;
}

uint64_t Transaction::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	if(!params) {
		throw std::logic_error("!params");
	}
	uint64_t fee = (inputs.size() + outputs.size()) * params->min_txfee_io;

	std::unordered_map<uint32_t, uint32_t> sol_count;
	for(const auto& in : inputs) {
		sol_count[in.solution]++;
	}
	for(const auto& entry : sol_count) {
		if(auto sol = get_solution(entry.first)) {
			if(sol->is_contract) {
				// TODO: fee += entry.second * params->min_txfee_exec;
			}
		}
	}
	for(const auto& op : execute) {
		if(op) {
			// TODO: fee += params->min_txfee_exec;
			fee += op->calc_min_fee(params);
		}
	}
	for(const auto& sol : solutions) {
		if(sol) {
			fee += sol->calc_min_fee(params);
		}
	}
	if(deploy) {
		fee += deploy->calc_min_fee(params);
	}
	return fee;
}

void Transaction::merge_sign(std::shared_ptr<const Transaction> tx)
{
	std::unordered_map<uint32_t, uint32_t> import_map;
	for(size_t i = 0; i < inputs.size() && i < tx->inputs.size(); ++i) {
		auto& our = inputs[i];
		const auto& other = tx->inputs[i];
		if(other.solution < tx->solutions.size() && our.solution >= solutions.size()) {
			auto iter = import_map.find(other.solution);
			if(iter != import_map.end()) {
				our.solution = iter->second;
			} else {
				our.solution = solutions.size();
				import_map[other.solution] = our.solution;
				solutions.push_back(tx->solutions[other.solution]);
			}
		}
	}
}

vnx::bool_t Transaction::is_signed() const
{
	for(const auto& in : inputs) {
		if(in.solution >= solutions.size()) {
			return false;
		}
	}
	return true;
}


} // mmx
