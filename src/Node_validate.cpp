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

std::shared_ptr<Block> Node::validate(std::shared_ptr<const Block> block) const
{
	const auto prev = find_prev_header(block);
	if(!prev) {
		throw std::logic_error("invalid prev");
	}
	if(prev->hash != state_hash) {
		throw std::logic_error("state mismatch");
	}
	if(block->height != prev->height + 1) {
		throw std::logic_error("invalid height");
	}
	if(block->time_diff == 0 || block->space_diff == 0) {
		throw std::logic_error("invalid difficulty");
	}
	if(auto proof = block->proof) {
		if(!block->farmer_sig || !block->farmer_sig->verify(proof->farmer_key, block->hash)) {
			throw std::logic_error("invalid farmer signature");
		}
		validate_diff_adjust(block->time_diff, prev->time_diff);
		validate_diff_adjust(block->space_diff, prev->space_diff);
	} else {
		if(block->tx_base || !block->tx_list.empty()) {
			throw std::logic_error("transactions not allowed");
		}
		if(block->time_diff != prev->time_diff || block->space_diff != prev->space_diff) {
			throw std::logic_error("invalid difficulty adjustment");
		}
	}
	auto context = Context::create();
	context->height = block->height;

	uint64_t base_spent = 0;
	if(auto tx = std::dynamic_pointer_cast<const Transaction>(block->tx_base)) {
		if(validate(tx, context, block, base_spent)) {
			throw std::logic_error("missing exec_outputs");
		}
	}
	{
		std::unordered_set<txio_key_t> inputs;
		for(const auto& base : block->tx_list) {
			if(auto tx = std::dynamic_pointer_cast<const Transaction>(base)) {
				for(const auto& in : tx->inputs) {
					if(!inputs.insert(in.prev).second) {
						throw std::logic_error("double spend");
					}
				}
			} else {
				throw std::logic_error("transaction missing");
			}
		}
	}
	auto out = vnx::clone(block);
	std::exception_ptr failed_ex;
	std::atomic<uint64_t> total_fees {0};
	std::atomic<uint64_t> total_cost {0};

#pragma omp parallel for
	for(size_t i = 0; i < out->tx_list.size(); ++i)
	{
		auto& base = out->tx_list[i];
		try {
			if(auto tx = std::dynamic_pointer_cast<const Transaction>(base)) {
				uint64_t fees = 0;
				if(auto new_tx = validate(tx, context, nullptr, fees)) {
					base = new_tx;
				}
				total_fees += fees;
				total_cost += tx->calc_cost(params);
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
	return out;
}

void Node::validate(std::shared_ptr<const Transaction> tx) const
{
	auto context = Context::create();
	context->height = get_height();

	uint64_t fee = 0;
	validate(tx, context, nullptr, fee);
}

std::shared_ptr<const Context> Node::create_context(const addr_t& address, std::shared_ptr<const Contract> contract,
													std::shared_ptr<const Context> base, std::shared_ptr<const Transaction> tx) const
{
	auto context = vnx::clone(base);
	context->txid = tx->id;
	for(const auto& addr : contract->get_dependency()) {
		if(auto contract = get_contract(addr)) {
			context->depends[addr] = contract;
		} else {
			auto pubkey = contract::PubKey::create();
			pubkey->address = addr;
			context->depends[addr] = pubkey;
		}
	}
	return context;
}

std::shared_ptr<const Transaction> Node::validate(	std::shared_ptr<const Transaction> tx, std::shared_ptr<const Context> context,
													std::shared_ptr<const Block> base, uint64_t& fee_amount) const
{
	if(tx->id != tx->calc_hash()) {
		throw std::logic_error("invalid tx id");
	}
	if(base) {
		if(tx->deploy) {
			throw std::logic_error("coin base cannot deploy");
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
		if(tx->inputs.size() != 1) {
			throw std::logic_error("coin base must have one input");
		}
		const auto& in = tx->inputs[0];
		if(in.prev.txid != hash_t(base->prev) || in.prev.index != 0) {
			throw std::logic_error("invalid coin base input");
		}
	} else {
		if(tx->inputs.empty()) {
			throw std::logic_error("tx without input");
		}
	}
	uint64_t base_amount = 0;
	std::vector<tx_out_t> exec_outputs;
	std::unordered_map<hash_t, uint64_t> amounts;

	if(!base) {
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
			auto contract = get_contract(utxo.address);
			if(!contract) {
				auto pubkey = contract::PubKey::create();
				pubkey->address = utxo.address;
				contract = pubkey;
			} else if(!solution->is_contract) {
				// TODO: throw std::logic_error("invalid solution: !is_contract");
			}
			auto spend = operation::Spend::create();
			spend->address = utxo.address;
			spend->solution = solution;
			spend->key = in.prev;
			spend->utxo = utxo;

			const auto outputs = contract->validate(spend, create_context(utxo.address, contract, context, tx));
			exec_outputs.insert(exec_outputs.end(), outputs.begin(), outputs.end());

			amounts[utxo.contract] += utxo.amount;
		}
	}
	for(const auto& op : tx->execute)
	{
		if(!op->is_valid()) {
			throw std::logic_error("invalid operation");
		}
		if(auto contract = get_contract(op->address)) {
			const auto outputs = contract->validate(op, create_context(op->address, contract, context, tx));
			exec_outputs.insert(exec_outputs.end(), outputs.begin(), outputs.end());
		} else {
			throw std::logic_error("no such contract");
		}
	}
	for(const auto& out : tx->outputs)
	{
		if(out.amount == 0) {
			throw std::logic_error("zero tx output");
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

	const auto fee_needed = tx->calc_cost(params);
	if(fee_amount < fee_needed) {
		throw std::logic_error("insufficient fee: " + std::to_string(fee_amount) + " < " + std::to_string(fee_needed));
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
