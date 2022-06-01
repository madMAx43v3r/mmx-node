/*
 * Node_validate.cpp
 *
 *  Created on: Jan 20, 2022
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/contract/NFT.hxx>
#include <mmx/contract/PubKey.hxx>
#include <mmx/contract/Executable.hxx>
#include <mmx/operation/Spend.hxx>
#include <mmx/operation/Mint.hxx>
#include <mmx/operation/Mutate.hxx>
#include <mmx/operation/Execute.hxx>
#include <mmx/operation/Deposit.hxx>
#include <mmx/operation/Revoke.hxx>
#include <mmx/utils.h>
#include <mmx/vm_interface.h>

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

void Node::execution_context_t::setup_wait(const hash_t& txid, const addr_t& address)
{
	const auto& list = mutate_map[address];
	if(!list.empty()) {
		wait_map[txid].insert(list.back());
	}
}

std::shared_ptr<Node::contract_state_t> Node::execution_context_t::find_state(const addr_t& address) const
{
	auto iter = contract_map.find(address);
	if(iter != contract_map.end()) {
		return iter->second;
	}
	return nullptr;
}

std::shared_ptr<const Contract> Node::execution_context_t::find_contract(const addr_t& address) const
{
	if(auto state = find_state(address)) {
		return state->data;
	}
	return nullptr;
}

std::shared_ptr<const Context>
Node::execution_context_t::get_context(std::shared_ptr<const Transaction> tx, std::shared_ptr<const Contract> contract) const
{
	auto out = vnx::clone(block);
	out->txid = tx->id;
	if(contract) {
		for(const auto& address : contract->get_dependency()) {
			if(auto contract = find_contract(address)) {
				out->depends[address] = contract;
			}
		}
	}
	return out;
}

std::shared_ptr<Node::execution_context_t> Node::new_exec_context() const
{
	auto context = std::make_shared<execution_context_t>();
	context->storage = std::make_shared<vm::StorageCache>(storage);
	return context;
}

std::shared_ptr<Node::contract_state_t>
Node::get_context_state(std::shared_ptr<execution_context_t> context, const addr_t& address) const
{
	auto& state = context->contract_map[address];
	if(!state) {
		state = std::make_shared<contract_state_t>();
		state->data = get_contract_for(address);
		state->balance = get_balances(address);
	}
	return state;
}

void Node::setup_context_wait(std::shared_ptr<execution_context_t> context, const hash_t& txid, const addr_t& address) const
{
	auto state = get_context_state(context, address);
	if(auto contract = state->data) {
		for(const auto& address : contract->get_dependency()) {
			get_context_state(context, address);
			context->setup_wait(txid, address);
		}
	}
	context->setup_wait(txid, address);
}

void Node::setup_context(	std::shared_ptr<execution_context_t> context,
							std::shared_ptr<const Transaction> tx) const
{
	for(const auto& in : tx->get_all_inputs()) {
		if(in.flags & txin_t::IS_EXEC) {
			setup_context_wait(context, tx->id, in.address);
		}
	}
	std::unordered_set<addr_t> mutate_set;
	for(const auto& op : tx->get_all_operations()) {
		if(std::dynamic_pointer_cast<const operation::Mutate>(op)) {
			mutate_set.insert(op->address);
		} else if(auto exec = std::dynamic_pointer_cast<const operation::Execute>(op)) {
			mutate_set.insert(op->address);
			if(exec->user) {
				setup_context_wait(context, tx->id, *exec->user);
			}
		} else if(op) {
			setup_context_wait(context, tx->id, op->address);
		}
	}
	if(auto nft = std::dynamic_pointer_cast<const contract::NFT>(tx->deploy)) {
		setup_context_wait(context, tx->id, nft->creator);
	}
	auto list = std::vector<addr_t>(mutate_set.begin(), mutate_set.end());
	while(!list.empty()) {
		std::vector<addr_t> more;
		for(const auto& address : list) {
			auto state = get_context_state(context, address);
			if(auto exec = std::dynamic_pointer_cast<const contract::Executable>(state->data)) {
				for(const auto& entry : exec->depends) {
					if(mutate_set.insert(entry.second).second) {
						more.push_back(entry.second);
					}
				}
			}
		}
		list = std::move(more);
	}
	for(const auto& address : mutate_set) {
		setup_context_wait(context, tx->id, address);
		context->mutate_map[address].push_back(tx->id);
	}
	if(!mutate_set.empty()) {
		context->signal_map.emplace(tx->id, std::make_shared<waitcond_t>());
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
	const auto diff_block = get_diff_header(block);
	const auto weight = calc_block_weight(params, diff_block, block, block->farmer_sig);
	if(block->weight != weight) {
		throw std::logic_error("invalid block weight: " + block->weight.str(10) + " != " + weight.str(10));
	}
	if(block->total_weight != prev->total_weight + block->weight) {
		throw std::logic_error("invalid block total_weight");
	}

	auto context = new_exec_context();
	{
		auto base = Context::create();
		base->height = block->height;
		context->block = base;
	}
	{
		std::unordered_set<addr_t> tx_set;
		std::multiset<std::pair<hash_t, addr_t>> revoked;
		balance_cache_t balance_cache(&balance_map);

		if(auto tx = block->tx_base) {
			tx_set.insert(tx->id);
		}
		for(const auto& tx : block->tx_list) {
			if(!tx) {
				throw std::logic_error("missing transaction");
			}
			if(uint32_t(tx->id.to_uint256() & 0x1) != (context->block->height & 0x1)) {
				throw std::logic_error("invalid inclusion");
			}
			{
				auto txi = tx;
				while(txi) {
					if(!tx_set.insert(txi->id).second) {
						throw std::logic_error("duplicate transaction");
					}
					if(txi->sender) {
						if(revoked.count(std::make_pair(txi->id, *txi->sender))) {
							throw std::logic_error("tx has been revoked");
						}
					}
					for(const auto& in : txi->inputs) {
						const auto balance = balance_cache.get(in.address, in.contract);
						if(!balance || in.amount > *balance) {
							throw std::logic_error("insufficient funds");
						}
						*balance -= in.amount;
					}
					for(const auto& op : txi->execute) {
						if(auto revoke = std::dynamic_pointer_cast<const operation::Revoke>(op)) {
							if(tx_set.count(revoke->txid)) {
								throw std::logic_error("tx cannot be revoked anymore");
							}
							revoked.emplace(revoke->txid, revoke->address);
						}
					}
					txi = txi->parent;
				}
			}
			setup_context(context, tx);
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
			uint64_t fee = 0;
			uint64_t cost = 0;
			if(validate(tx, context, nullptr, fee, cost)) {
				throw std::logic_error("missing exec_inputs or exec_outputs");
			}
			total_fees += fee;
			total_cost += cost;
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
	uint64_t base_cost = 0;
	if(auto tx = block->tx_base) {
		if(validate(tx, context, block, base_spent, base_cost)) {
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
	auto context = new_exec_context();
	{
		auto base = Context::create();
		base->height = get_height() + 1;
		context->block = base;
	}
	setup_context(context, tx);

	uint64_t fee = 0;
	uint64_t cost = 0;
	validate(tx, context, nullptr, fee, cost);
}

void Node::execute(	std::shared_ptr<const Transaction> tx,
					std::shared_ptr<const execution_context_t> context,
					std::shared_ptr<contract_state_t> state,
					std::shared_ptr<const operation::Execute> exec,
					std::vector<txin_t>& exec_inputs,
					std::vector<txout_t>& exec_outputs,
					std::unordered_map<addr_t, uint128_t>& amounts,
					std::shared_ptr<vm::StorageCache> storage_cache,
					uint64_t& tx_cost, const bool is_public) const
{
	auto engine = std::make_shared<vm::Engine>(exec->address, storage_cache, false);

	auto total_gas = std::min<uint64_t>(amounts[addr_t()], params->max_block_cost);
	total_gas -= std::min(tx_cost, total_gas);
	engine->total_gas = total_gas;

	if(exec->user) {
		if(auto contract = context->find_contract(*exec->user)) {
			contract->validate(exec, context->get_context(tx, contract));
		} else {
			throw std::logic_error("no such user");
		}
		engine->write(vm::MEM_EXTERN + vm::EXTERN_USER, vm::uint_t(*exec->user));
	}
	engine->write(vm::MEM_EXTERN + vm::EXTERN_ADDRESS, vm::uint_t(exec->address));

	if(auto deposit = std::dynamic_pointer_cast<const operation::Deposit>(exec)) {
		txout_t out;
		out.address = exec->address;
		out.contract = deposit->currency;
		out.amount = deposit->amount;
		out.sender = deposit->sender;

		auto& value = amounts[out.contract];
		if(out.amount > value) {
			throw std::logic_error("deposit over-spend");
		}
		value -= out.amount;

		mmx::set_deposit(engine, out);
		exec_outputs.push_back(out);
	}
	mmx::set_args(engine, exec->args);
	execute(tx, context, state, exec_inputs, exec_outputs, storage_cache, engine, exec->method, tx_cost, is_public);
	tx_cost += engine->total_cost;
}

void Node::execute(	std::shared_ptr<const Transaction> tx,
					std::shared_ptr<const execution_context_t> context,
					std::shared_ptr<contract_state_t> state,
					std::vector<txin_t>& exec_inputs,
					std::vector<txout_t>& exec_outputs,
					std::shared_ptr<vm::StorageCache> storage_cache,
					std::shared_ptr<vm::Engine> engine,
					const std::string& method_name,
					uint64_t& tx_cost, const bool is_public) const
{
	if(!state || !state->data) {
		throw std::logic_error("no such contract");
	}
	const auto executable = std::dynamic_pointer_cast<const contract::Executable>(state->data);
	if(!executable) {
		throw std::logic_error("not an executable");
	}
	auto method = mmx::find_method(executable, method_name);
	if(!method) {
		throw std::logic_error("no such method");
	}
	if(is_public && !method->is_public) {
		throw std::logic_error("method is not public");
	}
	if(!is_public && method->is_public) {
		throw std::logic_error("method is public");
	}
	mmx::load(engine, executable);

	std::weak_ptr<vm::Engine> parent = engine;
	engine->remote = [this, tx, context, executable, storage_cache, parent, &exec_inputs, &exec_outputs, &tx_cost]
		(const std::string& name, const std::string& method, const uint32_t nargs)
	{
		auto engine = parent.lock();
		if(!engine) {
			throw std::logic_error("!engine");
		}
		auto iter = executable->depends.find(name);
		if(iter == executable->depends.end()) {
			throw std::runtime_error("no such external contract: " + name);
		}
		const auto& address = iter->second;

		auto state = context->find_state(address);
		auto child = std::make_shared<vm::Engine>(address, storage_cache, false);
		child->total_gas = engine->total_gas - std::min(engine->total_cost, engine->total_gas);

		const auto stack_ptr = engine->get_frame().stack_ptr;
		for(uint32_t i = 0; i < nargs; ++i) {
			mmx::copy(child, engine, vm::MEM_STACK + 1 + i, vm::MEM_STACK + stack_ptr + 1 + i);
		}
		child->write(vm::MEM_EXTERN + vm::EXTERN_USER, vm::uint_t(engine->contract));
		child->write(vm::MEM_EXTERN + vm::EXTERN_ADDRESS, vm::uint_t(address));

		execute(tx, context, state, exec_inputs, exec_outputs, storage_cache, child, method, tx_cost, true);

		mmx::copy(engine, child, vm::MEM_STACK + stack_ptr, vm::MEM_STACK);

		const auto cost = child->total_cost + params->min_txfee_exec;
		engine->total_gas -= std::min(cost, engine->total_gas);
		engine->check_gas();
		tx_cost += cost;
	};

	engine->write(vm::MEM_EXTERN + vm::EXTERN_HEIGHT, vm::uint_t(context->block->height));
	engine->write(vm::MEM_EXTERN + vm::EXTERN_TXID, vm::uint_t(tx->id));
	mmx::set_balance(engine, state->balance);

	mmx::execute(engine, *method);

	for(const auto& out : engine->outputs)
	{
		auto& amount = state->balance[out.contract];
		if(out.amount > amount) {
			throw std::logic_error("contract over-spend");
		}
		amount -= out.amount;

		txin_t in;
		in.address = engine->contract;
		in.contract = out.contract;
		in.amount = out.amount;
		exec_inputs.push_back(in);
		exec_outputs.push_back(out);
	}
	exec_outputs.insert(exec_outputs.end(), engine->mint_outputs.begin(), engine->mint_outputs.end());
}

void Node::validate(std::shared_ptr<const Transaction> tx,
					std::shared_ptr<const execution_context_t> context,
					std::shared_ptr<const Block> base,
					std::vector<txout_t>& outputs,
					std::vector<txout_t>& exec_outputs,
					balance_cache_t& balance_cache,
					std::unordered_map<addr_t, uint128_t>& amounts) const
{
	if(auto parent = tx->parent) {
		if(!parent->is_extendable) {
			throw std::logic_error("parent not extendable");
		}
		validate(parent, context, base, outputs, exec_outputs, balance_cache, amounts);
	}
	if(tx->expires < context->block->height) {
		throw std::logic_error("tx expired");
	}
	if(tx->is_extendable && tx->deploy) {
		throw std::logic_error("extendable tx cannot deploy");
	}
	if(base) {
		if(tx->deploy) {
			throw std::logic_error("coin base cannot deploy");
		}
		if(tx->inputs.size()) {
			throw std::logic_error("coin base cannot have inputs");
		}
		if(tx->execute.size()) {
			throw std::logic_error("coin base cannot have operations");
		}
		if(tx->note != tx_note_e::REWARD) {
			throw std::logic_error("invalid coin base note");
		}
		if(tx->salt != base->prev) {
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
		if(tx->solutions.size()) {
			throw std::logic_error("coin base cannot have solutions");
		}
	} else {
		if(tx->note == tx_note_e::REWARD) {
			throw std::logic_error("invalid note");
		}
		if(tx->salt != get_genesis_hash()) {
			throw std::logic_error("invalid salt");
		}
	}
	if(tx_index.find(tx->id)) {
		throw std::logic_error("duplicate tx");
	}
	if(tx->sender) {
		if(is_revoked(tx->id, *tx->sender)) {
			throw std::logic_error("tx has been revoked");
		}
	}

	for(const auto& in : tx->inputs)
	{
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
			contract = context->find_contract(in.address);
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
		spend->currency = in.contract;
		spend->balance = *balance;
		spend->amount = in.amount;

		const auto outputs = contract->validate(spend, context->get_context(tx, contract));
		exec_outputs.insert(exec_outputs.end(), outputs.begin(), outputs.end());

		*balance -= in.amount;
		amounts[in.contract] += in.amount;
	}
	outputs.insert(outputs.end(), tx->outputs.begin(), tx->outputs.end());
}

std::shared_ptr<const Transaction>
Node::validate(	std::shared_ptr<const Transaction> tx, std::shared_ptr<const execution_context_t> context,
				std::shared_ptr<const Block> base, uint64_t& tx_fee, uint64_t& tx_cost) const
{
	if(!tx->is_valid()) {
		throw std::logic_error("invalid tx");
	}
	if(tx->is_extendable) {
		throw std::logic_error("incomplete tx");
	}
	tx_cost = tx->calc_cost(params);

	uint128_t base_amount = 0;
	std::vector<txin_t> exec_inputs;
	std::vector<txout_t> outputs;
	std::vector<txout_t> exec_outputs;
	balance_cache_t balance_cache(&balance_map);
	std::unordered_map<addr_t, uint128_t> amounts;

	validate(tx, context, base, outputs, exec_outputs, balance_cache, amounts);

	for(const auto& out : outputs)
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
	if(amounts[addr_t()].upper()) {
		throw std::logic_error("fee amount overflow");
	}
	auto storage_cache = std::make_shared<vm::StorageCache>(context->storage);

	for(const auto& op : tx->get_all_operations())
	{
		if(!op || !op->is_valid()) {
			throw std::logic_error("invalid operation");
		}
		const auto state = context->find_state(op->address);
		if(!state) {
			throw std::logic_error("missing contract state");
		}
		const auto contract = state->data;
		if(!contract) {
			throw std::logic_error("no such contract");
		}
		{
			const auto outputs = contract->validate(op, context->get_context(tx, contract));
			exec_outputs.insert(exec_outputs.end(), outputs.begin(), outputs.end());
		}
		if(auto revoke = std::dynamic_pointer_cast<const operation::Revoke>(op))
		{
			if(tx_index.find(revoke->txid)) {
				throw std::logic_error("tx cannot be revoked anymore");
			}
		}
		else if(auto mutate = std::dynamic_pointer_cast<const operation::Mutate>(op))
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
			state->is_mutated = true;
		}
		else if(auto exec = std::dynamic_pointer_cast<const operation::Execute>(op))
		{
			execute(tx, context, state, exec, exec_inputs, exec_outputs, amounts, storage_cache, tx_cost, true);
		}
	}
	if(tx->deploy) {
		if(!tx->deploy->is_valid()) {
			throw std::logic_error("invalid contract");
		}
		if(auto nft = std::dynamic_pointer_cast<const contract::NFT>(tx->deploy))
		{
			const auto creator = context->find_contract(nft->creator);
			if(!creator) {
				throw std::logic_error("no such creator");
			}
			{
				auto op = operation::Mint::create();
				op->address = tx->id;
				op->solution = nft->solution;
				op->target = nft->creator;
				op->amount = 1;
				creator->validate(op, context->get_context(tx, creator));
			}
			txout_t out;
			out.contract = tx->id;
			out.address = nft->creator;
			out.amount = 1;
			exec_outputs.push_back(out);
		}
		else if(auto executable = std::dynamic_pointer_cast<const contract::Executable>(tx->deploy))
		{
			auto state = std::make_shared<contract_state_t>();
			state->data = executable;

			auto exec = operation::Execute::create();
			exec->address = tx->id;
			exec->method = executable->init_method;
			exec->args = executable->init_args;
			execute(tx, context, state, exec, exec_inputs, exec_outputs, amounts, storage_cache, tx_cost, false);
		}
	}

	if(base) {
		if(tx_cost > params->max_txbase_cost) {
			throw std::logic_error("tx cost > max_txbase_cost");
		}
		if(base_amount.upper()) {
			throw std::logic_error("coin base output overflow");
		}
		tx_fee = base_amount;
	}
	else {
		if(tx_cost > params->max_block_cost) {
			throw std::logic_error("tx cost > max_block_cost");
		}
		{
			const auto amount = (uint128_t(tx_cost) * tx->fee_ratio) / 1024;
			if(amount.upper()) {
				throw std::logic_error("fee amount overflow");
			}
			tx_fee = amount;
		}
		{
			const uint64_t amount = amounts[addr_t()];
			if(amount < tx_fee) {
				throw std::logic_error("insufficient fee: " + std::to_string(amount) + " < " + std::to_string(tx_fee));
			}
			const uint64_t change = amount - tx_fee;
			if(change > params->min_txfee_io && tx->sender) {
				txout_t out;
				out.address = *tx->sender;
				out.amount = change;
				exec_outputs.push_back(out);
			} else {
				tx_fee = amount;
			}
		}
	}
	std::shared_ptr<Transaction> out = nullptr;

	if(tx->exec_inputs.empty() && tx->exec_outputs.empty())
	{
		if(!exec_inputs.empty() || !exec_outputs.empty()) {
			auto copy = vnx::clone(tx);
			copy->exec_inputs = exec_inputs;
			copy->exec_outputs = exec_outputs;
			out = copy;
		}
	}
	else {
		if(tx->exec_inputs.size() != exec_inputs.size()) {
			throw std::logic_error("exec_inputs count mismatch"
					+ std::to_string(tx->exec_inputs.size()) + " != " + std::to_string(exec_inputs.size()));
		}
		if(tx->exec_outputs.size() != exec_outputs.size()) {
			throw std::logic_error("exec_outputs count mismatch: "
					+ std::to_string(tx->exec_outputs.size()) + " != " + std::to_string(exec_outputs.size()));
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
	storage_cache->commit();
	return out;
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
