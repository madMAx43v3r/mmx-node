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
#include <mmx/operation/Execute.hxx>
#include <mmx/operation/Revoke.hxx>
#include <mmx/utils.h>

#include <vnx/vnx.h>


namespace mmx {

void Node::execution_context_t::wait(const hash_t& txid) const
{
	auto iter = wait_map.find(txid);
	if(iter != wait_map.end()) {
		for(const auto& prev : iter->second) {
			auto iter = signal_map.find(prev);
			if(iter != signal_map.end()) {
				auto& entry = iter->second;
				{
					std::unique_lock<std::mutex> lock(entry->mutex);
					while(entry->do_wait) {
						entry->signal.wait(lock);
					}
				}
			}
		}
	}
}

void Node::execution_context_t::signal(const hash_t& txid) const
{
	auto iter = signal_map.find(txid);
	if(iter != signal_map.end()) {
		auto& entry = iter->second;
		{
			std::lock_guard lock(entry->mutex);
			entry->do_wait = false;
		}
		entry->signal.notify_all();
	}
}

std::shared_ptr<Node::execution_context_t> Node::validate(std::shared_ptr<const Block> block) const
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

	auto context = std::make_shared<execution_context_t>();
	{
		auto base = Context::create();
		base->height = block->height;
		context->block = base;
	}
	{
		std::unordered_set<addr_t> tx_set;
		std::unordered_map<addr_t, std::vector<hash_t>> mutate_map;
		std::multiset<std::pair<hash_t, addr_t>> revoked;
		balance_cache_t balance_cache(&balance_map);

		if(auto tx = block->tx_base) {
			tx_set.insert(tx->id);
		}
		for(auto tx : block->tx_list) {
			if(!tx) {
				throw std::logic_error("missing transaction");
			}
			if(uint32_t(tx->id.to_uint256() & 0x1) != (context->block->height & 0x1)) {
				throw std::logic_error("invalid inclusion");
			}
			std::unordered_set<addr_t> mutate_set;
			while(tx) {
				if(!tx_set.insert(tx->id).second) {
					throw std::logic_error("duplicate transaction");
				}
				for(const auto& in : tx->inputs) {
					if(revoked.count(std::make_pair(tx->id, in.address))) {
						throw std::logic_error("tx has been revoked");
					}
					if(in.flags & txin_t::IS_EXEC) {
						const auto& list = mutate_map[in.address];
						if(!list.empty()) {
							context->wait_map[tx->id].insert(list.back());
						}
					}
					const auto balance = balance_cache.get(in.address, in.contract);
					if(!balance || in.amount > *balance) {
						throw std::logic_error("insufficient funds");
					}
					*balance -= in.amount;
				}
				for(const auto& op : tx->execute) {
					if(std::dynamic_pointer_cast<const operation::Mutate>(op)
						|| std::dynamic_pointer_cast<const operation::Execute>(op))
					{
						mutate_set.insert(op->address);
					}
					if(auto revoke = std::dynamic_pointer_cast<const operation::Revoke>(op)) {
						if(tx_set.count(revoke->txid)) {
							throw std::logic_error("tx cannot be revoked anymore");
						}
						revoked.emplace(revoke->txid, revoke->address);
					}
				}
				tx = tx->parent;
			}
			for(const auto& address : mutate_set) {
				{
					auto& list = mutate_map[address];
					if(!list.empty()) {
						context->wait_map[tx->id].insert(list.back());
					}
					list.push_back(tx->id);
				}
				context->contract_map.emplace(address, std::make_shared<contract_state_t>());
			}
			if(!mutate_set.empty()) {
				context->signal_map.emplace(tx->id, std::make_shared<waitcond_t>());
			}
		}
	}
	std::exception_ptr failed_ex;
	std::atomic<uint64_t> total_fees {0};
	std::atomic<uint64_t> total_cost {0};

#pragma omp parallel for
	for(int i = 0; i < int(block->tx_list.size()); ++i)
	{
		const auto& tx = block->tx_list[i];
		context->wait(tx->id);
		try {
			uint64_t fees = 0;
			if(validate(tx, context, nullptr, fees)) {
				throw std::logic_error("missing exec_inputs or exec_outputs");
			}
			total_fees += fees;
			total_cost += tx->calc_cost(params);
		} catch(...) {
#pragma omp critical
			failed_ex = std::current_exception();
		}
		context->signal(tx->id);
	}
	if(failed_ex) {
		std::rethrow_exception(failed_ex);
	}
	if(total_cost > params->max_block_cost) {
		throw std::logic_error("block cost too high: " + std::to_string(uint64_t(total_cost)));
	}
	uint64_t base_spent = 0;
	if(auto tx = block->tx_base) {
		if(validate(tx, context, block, base_spent)) {
			throw std::logic_error("invalid tx_base");
		}
	}
	const auto base_reward = calc_block_reward(block);
	const auto base_allowed = calc_final_block_reward(params, base_reward, total_fees);
	if(base_spent > base_allowed) {
		throw std::logic_error("coin base over-spend");
	}
	return context;
}

void Node::validate(std::shared_ptr<const Transaction> tx) const
{
	auto context = std::make_shared<execution_context_t>();
	{
		auto base = Context::create();
		base->height = get_height() + 1;
		context->block = base;
	}
	uint64_t fee = 0;
	validate(tx, context, nullptr, fee);
}

std::shared_ptr<const Context> Node::create_context(
		std::shared_ptr<const Context> base, std::shared_ptr<const Contract> contract, std::shared_ptr<const Transaction> tx) const
{
	auto context = vnx::clone(base);
	context->txid = tx->id;
	for(const auto& addr : contract->get_dependency()) {
		context->depends[addr] = get_contract_for(addr);
	}
	return context;
}

void Node::validate(std::shared_ptr<const Transaction> tx,
					std::shared_ptr<const execution_context_t> context,
					std::vector<txin_t>& exec_inputs,
					std::vector<txout_t>& outputs,
					std::vector<txout_t>& exec_outputs,
					balance_cache_t& balance_cache,
					std::unordered_map<addr_t, uint128_t>& amounts) const
{
	if(tx->expires < context->block->height) {
		throw std::logic_error("expired tx");
	}
	if(tx->is_extendable && tx->deploy) {
		throw std::logic_error("extendable cannot deploy");
	}
	if(tx_index.find(tx->id)) {
		throw std::logic_error("duplicate tx");
	}
	if(auto parent = tx->parent) {
		if(!parent->is_extendable) {
			throw std::logic_error("not extendable");
		}
		validate(parent, context, exec_inputs, outputs, exec_outputs, balance_cache, amounts);
	}
	const auto revoked = get_revokations(tx->id);

	for(const auto& in : tx->inputs)
	{
		if(revoked.count(in.address)) {
			throw std::logic_error("tx has been revoked");
		}
		const auto balance = balance_cache.get(in.address, in.contract);
		if(!balance || in.amount > *balance) {
			throw std::logic_error("insufficient funds");
		}
		const auto solution = tx->get_solution(in.solution);
		if(!solution) {
			throw std::logic_error("missing solution");
		}
		std::shared_ptr<const Contract> contract;

		if(in.flags & txin_t::IS_EXEC) {
			auto iter = context->contract_map.find(in.address);
			if(iter != context->contract_map.end()) {
				if(auto state = iter->second) {
					contract = state->data;
				}
			}
			if(!contract) {
				contract = get_contract(in.address);
			}
		} else {
			auto pubkey = contract::PubKey::create();
			pubkey->address = in.address;
			contract = pubkey;
		}
		if(!contract) {
			throw std::logic_error("no such contract");
		}
		auto spend = operation::Spend::create();
		spend->address = in.address;
		spend->solution = solution;
		spend->balance = *balance;
		spend->amount = in.amount;

		const auto outputs = contract->validate(spend, create_context(context->block, contract, tx));
		exec_outputs.insert(exec_outputs.end(), outputs.begin(), outputs.end());

		*balance -= in.amount;
		amounts[in.contract] += in.amount;
	}
	for(const auto& op : tx->execute)
	{
		if(!op || !op->is_valid()) {
			throw std::logic_error("invalid operation");
		}
		std::shared_ptr<contract_state_t> state;
		{
			auto iter = context->contract_map.find(op->address);
			if(iter != context->contract_map.end()) {
				state = iter->second;
			}
		}
		if(!state) {
			throw std::logic_error("missing contract state");
		}
		std::shared_ptr<const Contract> contract = state->data;
		if(!contract) {
			contract = get_contract(op->address);
		}
		if(!contract) {
			throw std::logic_error("no such contract");
		}
		if(auto revoke = std::dynamic_pointer_cast<const operation::Revoke>(op))
		{
			if(tx_index.find(revoke->txid)) {
				throw std::logic_error("tx cannot be revoked anymore");
			}
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
			state->data = copy;
		}
		if(auto execute = std::dynamic_pointer_cast<const operation::Execute>(op))
		{
			// TODO
		}
		const auto outputs = contract->validate(op, create_context(context->block, contract, tx));
		exec_outputs.insert(exec_outputs.end(), outputs.begin(), outputs.end());
	}
	outputs.insert(outputs.end(), tx->outputs.begin(), tx->outputs.end());
}

std::shared_ptr<const Transaction>
Node::validate(	std::shared_ptr<const Transaction> tx, std::shared_ptr<const execution_context_t> context,
				std::shared_ptr<const Block> base, uint64_t& fee_amount) const
{
	if(!tx->is_valid()) {
		throw std::logic_error("invalid tx");
	}
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
		if(tx->note != tx_note_e::REWARD) {
			throw std::logic_error("invalid coin base note");
		}
		if(!tx->salt || *tx->salt != base->vdf_output[0]) {
			throw std::logic_error("invalid coin base salt");
		}
		if(tx->expires != base->height) {
			throw std::logic_error("invalid coin base expires");
		}
		if(tx->fee_ratio != 1024) {
			throw std::logic_error("invalid coin base fee_ratio");
		}
		if(tx->sender) {
			throw std::logic_error("coin base cannot have sender");
		}
		if(tx->parent) {
			throw std::logic_error("coin base cannot have parent");
		}
		if(tx->is_extendable) {
			throw std::logic_error("coin base cannot be extendable");
		}
	} else {
		if(tx->note == tx_note_e::REWARD) {
			throw std::logic_error("invalid note");
		}
	}
	const auto tx_cost = tx->calc_cost(params);
	if(base) {
		if(tx_cost > params->max_txbase_cost) {
			throw std::logic_error("tx cost > max_txbase_cost");
		}
	} else {
		if(tx_cost > params->max_block_cost) {
			throw std::logic_error("tx cost > max_block_cost");
		}
	}
	uint128_t base_amount = 0;
	std::vector<txout_t> outputs;
	std::vector<txin_t> exec_inputs;
	std::vector<txout_t> exec_outputs;
	balance_cache_t balance_cache(&balance_map);
	std::unordered_map<addr_t, uint128_t> amounts;

	validate(tx, context, exec_inputs, outputs, exec_outputs, balance_cache, amounts);

	for(const auto& out : outputs) {
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
		if(auto nft = std::dynamic_pointer_cast<const contract::NFT>(tx->deploy))
		{
			const auto creator = get_contract_for(nft->creator);
			{
				auto op = Operation::create();
				op->address = nft->creator;
				op->solution = nft->solution;
				creator->validate(op, create_context(context->block, creator, tx));
			}
			txout_t out;
			out.contract = tx->id;
			out.address = nft->creator;
			out.amount = 1;
			exec_outputs.push_back(out);
		}
	}
	if(base) {
		if(base_amount.upper()) {
			throw std::logic_error("coin base output overflow");
		}
		fee_amount = base_amount;
		return nullptr;
	}
	fee_amount = (uint128_t(tx_cost) * tx->fee_ratio) / 1024;
	{
		const auto& amount = amounts[addr_t()];
		if(amount.upper()) {
			throw std::logic_error("fee amount overflow");
		}
		if(amount < fee_amount) {
			throw std::logic_error("insufficient fee: " + amount.str(10) + " < " + std::to_string(fee_amount));
		}
		const uint64_t change = amount - fee_amount;
		if(change > params->min_txfee_io && tx->sender) {
			txout_t out;
			out.address = *tx->sender;
			out.amount = change;
			exec_outputs.push_back(out);
		} else {
			fee_amount = amount;
		}
	}
	if(tx->exec_inputs.empty() && tx->exec_outputs.empty()) {
		if(!exec_inputs.empty() || !exec_outputs.empty()) {
			auto copy = vnx::clone(tx);
			copy->exec_inputs = exec_inputs;
			copy->exec_outputs = exec_outputs;
			return copy;
		}
	} else {
		if(tx->exec_inputs.size() != exec_inputs.size()) {
			throw std::logic_error("exec_inputs count mismatch");
		}
		if(tx->exec_outputs.size() != exec_outputs.size()) {
			throw std::logic_error("exec_outputs count mismatch");
		}
		for(const auto& in : tx->exec_inputs) {
			if(in.solution != std::numeric_limits<decltype(in.solution)>::max()) {
				throw std::logic_error("invalid exec_input");
			}
		}
		for(size_t i = 0; i < exec_inputs.size(); ++i) {
			const auto& lhs = exec_inputs[i];
			const auto& rhs = tx->exec_inputs[i];
			if(lhs.contract != rhs.contract || lhs.address != rhs.address || lhs.amount != rhs.amount || lhs.flags != rhs.flags) {
				throw std::logic_error("exec_input mismatch at index " + std::to_string(i));
			}
		}
		for(size_t i = 0; i < exec_outputs.size(); ++i) {
			const auto& lhs = exec_outputs[i];
			const auto& rhs = tx->exec_outputs[i];
			if(lhs.contract != rhs.contract || lhs.address != rhs.address || lhs.amount != rhs.amount
				|| bool(lhs.sender) != bool(rhs.sender) || (lhs.sender && rhs.sender && *lhs.sender != *rhs.sender))
			{
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
