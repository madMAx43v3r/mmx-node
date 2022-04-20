/*
 * Node_validate.cpp
 *
 *  Created on: Jan 20, 2022
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/contract/NFT.hxx>
#include <mmx/contract/PubKey.hxx>
#include <mmx/operation/Spend.hxx>
#include <mmx/operation/Mutate.hxx>
#include <mmx/utils.h>

#include <vnx/vnx.h>


namespace mmx {

void Node::validate(std::shared_ptr<const Block> block) const
{
	// Note: block hash, tx_hash, tx_count already verified together with proof
	const auto prev = find_prev_header(block);
	if(!prev) {
		throw std::logic_error("missing prev");
	}
	if(prev->hash != state_hash) {
		throw std::logic_error("state mismatch");
	}
	if(block->version != 0) {
		throw std::logic_error("invalid version");
	}
	if(block->height != prev->height + 1) {
		throw std::logic_error("invalid height");
	}
	if(block->time_diff == 0 || block->space_diff == 0) {
		throw std::logic_error("invalid difficulty");
	}
	if(block->time_diff < params->min_time_diff) {
		throw std::logic_error("time_diff < min_time_diff");
	}
	if(block->farmer_sig) {
		// Note: farmer_sig already verified together with proof
		validate_diff_adjust(block->time_diff, prev->time_diff);
	} else {
		if(block->time_diff != prev->time_diff) {
			throw std::logic_error("invalid time_diff adjust");
		}
	}
	const auto proof_score = block->proof ? block->proof->score : params->score_threshold;
	if(block->space_diff != calc_new_space_diff(params, prev->space_diff, proof_score)) {
		throw std::logic_error("invalid space_diff adjust");
	}
	const auto weight = calc_block_weight(params, prev, block->proof, block->farmer_sig);
	if(block->weight != weight) {
		throw std::logic_error("invalid block weight: " + block->weight.str(10) + " != " + weight.str(10));
	}
	if(block->total_weight != prev->total_weight + block->weight) {
		throw std::logic_error("invalid block total_weight");
	}
	auto context = Context::create();
	context->height = block->height;

	uint64_t base_spent = 0;
	if(auto tx = std::dynamic_pointer_cast<const Transaction>(block->tx_base)) {
		if(validate(tx, context, block, base_spent)) {
			throw std::logic_error("invalid tx_base");
		}
	} else if(block->tx_base) {
		throw std::logic_error("invalid transaction type");
	}
	{
		std::unordered_set<txio_key_t> spent;
		std::unordered_set<addr_t> mutated;

		for(const auto& base : block->tx_list) {
			if(auto tx = std::dynamic_pointer_cast<const Transaction>(base)) {
				for(const auto& in : tx->inputs) {
					if(!spent.insert(in.prev).second) {
						throw std::logic_error("double spend");
					}
				}
				{
					std::unordered_set<addr_t> addr_set;
					for(const auto& op : tx->execute) {
						if(std::dynamic_pointer_cast<const operation::Mutate>(op)) {
							addr_set.insert(op->address);
						}
					}
					for(const auto& addr : addr_set) {
						if(!mutated.insert(addr).second) {
							throw std::logic_error("concurrent mutation");
						}
					}
				}
			} else {
				throw std::logic_error("missing transaction");
			}
		}
	}
	std::exception_ptr failed_ex;
	std::atomic<uint64_t> total_fees {0};
	std::atomic<uint64_t> total_cost {0};

#pragma omp parallel for
	for(int i = 0; i < int(block->tx_list.size()); ++i)
	{
		const auto& base = block->tx_list[i];
		try {
			if(auto tx = std::dynamic_pointer_cast<const Transaction>(base))
			{
				if(uint32_t(tx->id.to_uint256() & 0x1) != (context->height & 0x1)) {
					throw std::logic_error("invalid inclusion");
				}
				uint64_t fees = 0;
				if(validate(tx, context, nullptr, fees)) {
					throw std::logic_error("missing exec_outputs");
				}
				total_fees += fees;
				total_cost += tx->calc_cost(params);
			}
			else {
				throw std::logic_error("invalid transaction type");
			}
		} catch(...) {
#pragma omp critical
			failed_ex = std::current_exception();
		}
	}
	if(failed_ex) {
		std::rethrow_exception(failed_ex);
	}
	if(total_cost > params->max_block_cost) {
		throw std::logic_error("block cost too high: " + std::to_string(uint64_t(total_cost)));
	}
	const auto base_reward = calc_block_reward(block);
	const auto base_allowed = calc_final_block_reward(params, base_reward, total_fees);
	if(base_spent > base_allowed) {
		throw std::logic_error("coin base over-spend");
	}
}

void Node::validate(std::shared_ptr<const Transaction> tx) const
{
	auto context = Context::create();
	context->height = get_height() + 1;

	uint64_t fee = 0;
	validate(tx, context, nullptr, fee);
}

std::shared_ptr<const Context> Node::create_exec_context(
		std::shared_ptr<const Context> base, std::shared_ptr<const Contract> contract, std::shared_ptr<const Transaction> tx) const
{
	auto context = vnx::clone(base);
	context->txid = tx->id;
	for(const auto& addr : contract->get_dependency()) {
		context->depends[addr] = get_contract_for(addr);
	}
	return context;
}

std::shared_ptr<const Transaction> Node::validate(	std::shared_ptr<const Transaction> tx, std::shared_ptr<const Context> context,
													std::shared_ptr<const Block> base, uint64_t& fee_amount) const
{
	if(!tx->is_valid()) {
		throw std::logic_error("invalid tx");
	}
	const auto tx_cost = tx->calc_cost(params);

	if(base) {
		if(tx->deploy) {
			throw std::logic_error("coin base cannot deploy");
		}
		if(!tx->inputs.empty()) {
			throw std::logic_error("coin base cannot have inputs");
		}
		if(!tx->execute.empty()) {
			throw std::logic_error("coin base cannot have operations");
		}
		if(!tx->exec_outputs.empty()) {
			throw std::logic_error("coin base cannot have execution outputs");
		}
		if(tx->outputs.size() > params->max_tx_base_out) {
			throw std::logic_error("coin base has too many outputs");
		}
		if(!tx->salt || *tx->salt != base->vdf_output[0]) {
			throw std::logic_error("invalid coin base salt");
		}
	} else {
		if(tx->inputs.empty()) {
			throw std::logic_error("tx without input");
		}
	}
	if(tx_cost > params->max_block_cost) {
		throw std::logic_error("tx cost > max_block_cost");
	}
	{
		std::unordered_set<txio_key_t> spent;
		for(const auto& in : tx->inputs) {
			if(!spent.insert(in.prev).second) {
				throw std::logic_error("double spend");
			}
		}
	}
	{
		std::unordered_set<addr_t> contracts;
		for(const auto& in : tx->inputs) {
			if(in.flags & tx_in_t::IS_EXEC) {
				auto iter = utxo_map.find(in.prev);
				if(iter != utxo_map.end()) {
					const auto& utxo = iter->second;
					contracts.insert(utxo.address);
				}
			}
		}
		for(const auto& op : tx->execute) {
			if(op) {
				contracts.insert(op->address);
			}
		}
		if(contracts.size() > params->max_tx_operations) {
			throw std::logic_error("number of contracts > max_tx_operations");
		}
	}
	uint64_t base_amount = 0;
	std::vector<tx_out_t> exec_outputs;
	std::unordered_map<hash_t, uint64_t> amounts;
	std::unordered_map<addr_t, std::shared_ptr<Contract>> contract_state;

	for(const auto& in : tx->inputs)
	{
		auto iter = utxo_map.find(in.prev);
		if(iter == utxo_map.end()) {
			throw std::logic_error("utxo not found");
		}
		const auto& utxo = iter->second;

		const auto solution = tx->get_solution(in.solution);
		if(!solution) {
			throw std::logic_error("missing solution");
		}
		std::shared_ptr<const Contract> contract;

		if(in.flags & tx_in_t::IS_EXEC) {
			contract = get_contract(utxo.address);
		} else {
			auto pubkey = contract::PubKey::create();
			pubkey->address = utxo.address;
			contract = pubkey;
		}
		if(!contract) {
			throw std::logic_error("no such contract");
		}
		auto spend = operation::Spend::create();
		spend->address = utxo.address;
		spend->solution = solution;
		spend->key = in.prev;
		spend->utxo = utxo;

		const auto outputs = contract->validate(spend, create_exec_context(context, contract, tx));
		exec_outputs.insert(exec_outputs.end(), outputs.begin(), outputs.end());

		amounts[utxo.contract] += utxo.amount;
	}

	for(const auto& op : tx->execute)
	{
		if(!op || !op->is_valid()) {
			throw std::logic_error("invalid operation");
		}
		std::shared_ptr<const Contract> contract;
		{
			auto iter = contract_state.find(op->address);
			if(iter != contract_state.end()) {
				contract = iter->second;
			}
		}
		if(!contract) {
			contract = get_contract(op->address);
		}
		if(!contract) {
			throw std::logic_error("no such contract");
		}
		if(auto mutate = std::dynamic_pointer_cast<const operation::Mutate>(op))
		{
			auto copy = vnx::clone(contract);
			try {
				if(!copy->vnx_call(vnx::clone(mutate->method))) {
					throw std::logic_error("no such method");
				}
				if(!copy->is_valid()) {
					throw std::logic_error("invalid mutation");
				}
			} catch(const std::exception& ex) {
				throw std::logic_error("mutate failed with: " + std::string(ex.what()));
			}
			contract_state[op->address] = copy;
		}
		const auto outputs = contract->validate(op, create_exec_context(context, contract, tx));
		exec_outputs.insert(exec_outputs.end(), outputs.begin(), outputs.end());
	}

	for(const auto& out : tx->outputs)
	{
		if(out.amount == 0) {
			throw std::logic_error("zero amount output");
		}
		if(base) {
			if(out.contract != addr_t()) {
				throw std::logic_error("invalid coin base output");
			}
			base_amount += out.amount;
		}
		else {
			auto& value = amounts[out.contract];
			if(out.amount > value) {
				throw std::logic_error("tx over-spend");
			}
			value -= out.amount;
		}
	}

	if(tx->deploy) {
		if(!tx->deploy->is_valid()) {
			throw std::logic_error("invalid contract");
		}
		if(auto nft = std::dynamic_pointer_cast<const contract::NFT>(tx->deploy)) {
			tx_out_t out;
			out.contract = tx->id;
			out.address = nft->creator;
			out.amount = 1;
			exec_outputs.push_back(out);
		}
	}

	if(base) {
		fee_amount = base_amount;
		return nullptr;
	}
	fee_amount = amounts[hash_t()];

	if(fee_amount < tx_cost) {
		throw std::logic_error("insufficient fee: " + std::to_string(fee_amount) + " < " + std::to_string(tx_cost));
	}
	if(tx->exec_outputs.empty()) {
		if(!exec_outputs.empty()) {
			auto copy = vnx::clone(tx);
			copy->exec_outputs = exec_outputs;
			return copy;
		}
	} else {
		if(tx->exec_outputs.size() != exec_outputs.size()) {
			throw std::logic_error("exec_outputs size mismatch");
		}
		for(size_t i = 0; i < exec_outputs.size(); ++i) {
			const auto& lhs = exec_outputs[i];
			const auto& rhs = tx->exec_outputs[i];
			if(lhs.contract != rhs.contract || lhs.address != rhs.address || lhs.amount != rhs.amount) {
				throw std::logic_error("exec_output mismatch at index " + std::to_string(i));
			}
		}
	}
	return nullptr;
}

void Node::validate_diff_adjust(const uint64_t& block, const uint64_t& prev) const
{
	const auto max_update = std::max<uint64_t>(prev >> params->max_diff_adjust, 1);
	if(block > prev && block - prev > max_update) {
		throw std::logic_error("invalid difficulty adjustment upwards");
	}
	if(block < prev && prev - block > max_update) {
		throw std::logic_error("invalid difficulty adjustment downwards");
	}
}


} // mmx
