/*
 * Node_validate.cpp
 *
 *  Created on: Jan 20, 2022
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/contract/NFT.hxx>
#include <mmx/contract/PubKey.hxx>
#include <mmx/contract/Binary.hxx>
#include <mmx/contract/Executable.hxx>
#include <mmx/operation/Spend.hxx>
#include <mmx/operation/Mint.hxx>
#include <mmx/operation/Mutate.hxx>
#include <mmx/operation/Execute.hxx>
#include <mmx/operation/Deposit.hxx>
#include <mmx/utils.h>
#include <mmx/vm_interface.h>
#include <mmx/exception.h>

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
Node::execution_context_t::get_context_for(std::shared_ptr<const Transaction> tx, std::shared_ptr<const Contract> contract) const
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
		if(std::dynamic_pointer_cast<const contract::Executable>(state->data)) {
			state->balance = get_balances(address);
		}
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

void Node::prepare_context(std::shared_ptr<execution_context_t> context, std::shared_ptr<const Transaction> tx) const
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
	// TODO: maximum VP limit in recent blocks ?

	auto context = new_exec_context();
	{
		auto base = Context::create();
		base->height = block->height;
		context->block = base;
	}
	{
		std::unordered_set<addr_t> tx_set;
		balance_cache_t balance_cache(&balance_map);

		if(auto tx = block->tx_base) {
			tx_set.insert(tx->id);
		}
		for(const auto& tx : block->tx_list) {
			if(!tx) {
				throw std::logic_error("null tx");
			}
			if(!check_tx_inclusion(tx->id, context->block->height)) {
				throw std::logic_error("invalid tx inclusion");
			}
			if(!tx->sender) {
				throw std::logic_error("tx missing sender");
			}
			if(!tx->exec_result) {
				throw std::logic_error("tx missing exec_result");
			}
			if(!tx_set.insert(tx->id).second) {
				throw std::logic_error("duplicate tx in same block");
			}
			{
				// subtract tx fee
				const auto balance = balance_cache.find(*tx->sender, addr_t());
				const auto total_fee = tx->exec_result->total_fee;
				if(!balance || total_fee > *balance) {
					throw std::logic_error("insufficient funds to cover tx fee");
				}
				*balance -= total_fee;
			}
			if(!tx->exec_result->did_fail) {
				auto txi = tx;
				while(txi) {
					for(const auto& in : txi->get_inputs()) {
						const auto balance = balance_cache.find(in.address, in.contract);
						if(!balance || in.amount > *balance) {
							throw std::logic_error("insufficient funds to cover input");
						}
						*balance -= in.amount;
					}
					txi = txi->parent;
				}
			}
			prepare_context(context, tx);
		}
	}
	hash_t failed_tx;
	std::exception_ptr failed_ex;
	std::atomic<uint64_t> total_fees {0};
	std::atomic<uint64_t> total_cost {0};

	const auto tx_count = block->tx_list.size();
#pragma omp parallel for if(is_synced || tx_count >= 16)
	for(int i = 0; i < int(tx_count); ++i)
	{
		const auto& tx = block->tx_list[i];
		context->wait(tx->id);
		try {
			if(validate(tx, context)) {
				throw std::logic_error("missing exec_result");
			}
			total_fees += tx->exec_result->total_fee;
			total_cost += tx->exec_result->total_cost;
		} catch(...) {
#pragma omp critical
			{
				failed_tx = tx->id;
				failed_ex = std::current_exception();
			}
		}
		context->signal(tx->id);
	}
	if(failed_ex) {
		try {
			std::rethrow_exception(failed_ex);
		} catch(const std::exception& ex) {
			throw std::logic_error(std::string(ex.what()) + " (" + failed_tx.to_string() + ")");
		}
	}
	if(total_cost > params->max_block_cost) {
		throw std::logic_error("block cost too high: " + std::to_string(uint64_t(total_cost)));
	}
	if(auto tx = block->tx_base) {
		if(validate(tx, context, block)) {
			throw std::logic_error("invalid tx_base");
		}
		const auto base_reward = calc_block_reward(block);
		const auto base_allowed = calc_final_block_reward(params, base_reward, total_fees);
		if(tx->exec_result->total_fee > base_allowed) {
			throw std::logic_error("coin base over-spend");
		}
	}
	return context;
}

std::shared_ptr<const exec_result_t> Node::validate(std::shared_ptr<const Transaction> tx) const
{
	auto context = new_exec_context();
	{
		auto base = Context::create();
		base->height = get_height() + 1;
		context->block = base;
	}
	prepare_context(context, tx);

	return validate(tx, context);
}

void Node::execute(	std::shared_ptr<const Transaction> tx,
					std::shared_ptr<const execution_context_t> context,
					std::shared_ptr<contract_state_t> state,
					std::shared_ptr<const operation::Execute> exec,
					std::vector<txin_t>& exec_inputs,
					std::vector<txout_t>& exec_outputs,
					std::unordered_map<addr_t, uint128>& amounts,
					std::shared_ptr<vm::StorageCache> storage_cache,
					uint64_t& tx_cost, const bool is_public) const
{
	auto engine = std::make_shared<vm::Engine>(exec->address, storage_cache, false);
	{
		const auto avail_gas = (uint64_t(tx->max_fee_amount) * 1024) / tx->fee_ratio;
		engine->total_gas = std::min(avail_gas - std::min(tx_cost, avail_gas), params->max_block_cost);
	}
	if(exec->user) {
		if(auto contract = context->find_contract(*exec->user)) {
			contract->validate(exec, context->get_context_for(tx, contract));
		} else {
			throw std::logic_error("no such user");
		}
		engine->write(vm::MEM_EXTERN + vm::EXTERN_USER, vm::uint_t(exec->user->to_uint256()));
	}
	engine->write(vm::MEM_EXTERN + vm::EXTERN_ADDRESS, vm::uint_t(exec->address.to_uint256()));

	if(auto deposit = std::dynamic_pointer_cast<const operation::Deposit>(exec)) {
		txout_t out;
		out.address = exec->address;
		out.contract = deposit->currency;
		out.amount = deposit->amount;

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
	const auto binary = std::dynamic_pointer_cast<const contract::Binary>(get_contract(executable->binary));
	if(!executable) {
		throw std::logic_error("no such binary: " + executable->binary.to_string());
	}
	auto method = mmx::find_method(binary, method_name);
	if(!method) {
		throw std::logic_error("no such method: " + method_name);
	}
	if(is_public && !method->is_public) {
		throw std::logic_error("method is not public: " + method_name);
	}
	if(!is_public && method->is_public) {
		throw std::logic_error("method is public: " + method_name);
	}
	mmx::load(engine, binary);

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
		child->write(vm::MEM_EXTERN + vm::EXTERN_USER, vm::uint_t(engine->contract.to_uint256()));
		child->write(vm::MEM_EXTERN + vm::EXTERN_ADDRESS, vm::uint_t(address.to_uint256()));

		execute(tx, context, state, exec_inputs, exec_outputs, storage_cache, child, method, tx_cost, true);

		mmx::copy(engine, child, vm::MEM_STACK + stack_ptr, vm::MEM_STACK);

		const auto cost = child->total_cost + params->min_txfee_exec;
		engine->total_gas -= std::min(cost, engine->total_gas);
		engine->check_gas();
		tx_cost += cost;
	};

	engine->write(vm::MEM_EXTERN + vm::EXTERN_HEIGHT, vm::uint_t(context->block->height));
	engine->write(vm::MEM_EXTERN + vm::EXTERN_TXID, vm::uint_t(tx->id.to_uint256()));
	mmx::set_balance(engine, state->balance);

	mmx::execute(engine, *method);

	for(const auto& out : engine->outputs)
	{
		auto& amount = state->balance[out.contract];
		if(out.amount > amount) {
			throw std::logic_error("contract over-spend");
		}
		amount -= out.amount;

		// TODO: combine multiple inputs when possible
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
					std::unordered_map<addr_t, uint128>& amounts) const
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
	if(tx->is_extendable) {
		if(tx->deploy) {
			throw std::logic_error("extendable tx cannot deploy");
		}
		if(tx->exec_result) {
			throw std::logic_error("extendable tx cannot have exec_result");
		}
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
		if(!tx->salt || *tx->salt != base->prev) {
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
		if(tx->max_fee_amount) {
			throw std::logic_error("coin base invalid max_fee_amount");
		}
	} else {
		if(tx->note == tx_note_e::REWARD) {
			throw std::logic_error("invalid note: " + vnx::to_string(tx->note));
		}
	}
	if(tx_index.find(tx->id)) {
		throw std::logic_error("duplicate parent tx");
	}

	for(const auto& in : tx->inputs)
	{
		const auto balance = balance_cache.find(in.address, in.contract);
		if(!balance || in.amount > *balance) {
			throw std::logic_error("insufficient funds for " + in.address.to_string());
		}
		const auto solution = tx->get_solution(in.solution);
		if(!solution) {
			throw mmx::invalid_solution("missing solution");
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
			throw std::logic_error("no such contract: " + in.address.to_string());
		}
		auto spend = operation::Spend::create();
		spend->address = in.address;
		spend->solution = solution;
		spend->currency = in.contract;
		spend->amount = in.amount;

		const auto outputs = contract->validate(spend, context->get_context_for(tx, contract));
		exec_outputs.insert(exec_outputs.end(), outputs.begin(), outputs.end());

		*balance -= in.amount;
		amounts[in.contract] += in.amount;
	}
	outputs.insert(outputs.end(), tx->outputs.begin(), tx->outputs.end());
}

std::shared_ptr<const exec_result_t>
Node::validate(	std::shared_ptr<const Transaction> tx,
				std::shared_ptr<const execution_context_t> context,
				std::shared_ptr<const Block> base) const
{
	if(!tx->is_valid(params)) {
		throw mmx::static_failure("invalid tx");
	}
	if(tx->is_extendable) {
		throw mmx::static_failure("incomplete tx");
	}
	if(tx->static_cost > params->max_block_cost) {
		throw mmx::static_failure("static_cost > max_block_cost");
	}
	if(tx_index.find(tx->id)) {
		throw mmx::static_failure("duplicate tx");
	}
	uint64_t tx_fee = 0;
	uint64_t tx_cost = tx->static_cost;
	uint128_t base_amount = 0;
	std::vector<txin_t> exec_inputs;
	std::vector<txout_t> outputs;
	std::vector<txout_t> exec_outputs;
	balance_cache_t balance_cache(&balance_map);
	std::unordered_map<addr_t, uint128> amounts;
	std::exception_ptr failed_ex;

	const auto static_fee = (uint64_t(tx->static_cost) * tx->fee_ratio) / 1024;

	if(!base) {
		if(static_fee > tx->max_fee_amount) {
			throw mmx::static_failure("static tx fee > max_fee_amount: " + std::to_string(static_fee) + " > " + std::to_string(tx->max_fee_amount));
		}
		if(!tx->sender) {
			throw mmx::static_failure("missing tx sender");
		}
		if(tx->solutions.empty()) {
			throw mmx::static_failure("missing sender signature");
		}
		auto pubkey = contract::PubKey::create();
		pubkey->address = *tx->sender;

		auto spend = operation::Spend::create();
		spend->address = *tx->sender;
		spend->solution = tx->solutions[0];
		spend->amount = tx->max_fee_amount;

		pubkey->validate(spend, context->get_context_for(tx, pubkey));

		const auto balance = balance_cache.find(*tx->sender, addr_t());
		if(!balance || static_fee > *balance) {
			throw mmx::static_failure("insufficient funds for static tx fee");
		}
		*balance -= static_fee;
	}
	auto storage_cache = std::make_shared<vm::StorageCache>(context->storage);

	try {
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

		std::shared_ptr<contract_state_t> self_state;
		if(tx->deploy) {
			if(!tx->deploy->is_valid()) {
				throw std::logic_error("invalid contract");
			}
			if(auto nft = std::dynamic_pointer_cast<const contract::NFT>(tx->deploy))
			{
				const auto creator = context->find_contract(nft->creator);
				if(!creator) {
					throw std::logic_error("no such creator: " + nft->creator.to_string());
				}
				{
					auto op = operation::Mint::create();
					op->address = tx->id;
					op->solution = nft->solution;
					op->target = nft->creator;
					op->amount = 1;
					creator->validate(op, context->get_context_for(tx, creator));
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
				self_state = state;

				auto exec = operation::Execute::create();
				exec->address = tx->id;
				exec->method = executable->init_method;
				exec->args = executable->init_args;
				execute(tx, context, state, exec, exec_inputs, exec_outputs, amounts, storage_cache, tx_cost, false);
			}
		}

		for(const auto& op : tx->get_all_operations())
		{
			if(!op || !op->is_valid()) {
				throw std::logic_error("invalid operation");
			}
			const auto state = op->address == addr_t() ? self_state : context->find_state(op->address);
			if(!state) {
				throw std::logic_error("missing contract state");
			}
			const auto contract = state->data;
			if(!contract) {
				throw std::logic_error("no such contract: " + op->address.to_string());
			}
			{
				const auto outputs = contract->validate(op, context->get_context_for(tx, contract));
				exec_outputs.insert(exec_outputs.end(), outputs.begin(), outputs.end());
			}
			if(auto mutate = std::dynamic_pointer_cast<const operation::Mutate>(op))
			{
				auto copy = vnx::clone(contract);
				try {
					if(!copy->vnx_call(vnx::clone(mutate->method))) {
						throw std::logic_error("no such method: " + mutate->method["__type"].to_string());
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

		if(base) {
			if(tx_cost > params->max_txbase_cost) {
				throw mmx::static_failure("coin base tx cost > max_txbase_cost: " + std::to_string(tx_cost));
			}
			if(base_amount >> 32) {
				throw mmx::static_failure("coin base output overflow");
			}
			tx_fee = base_amount;
		}
		else {
			for(const auto& entry : amounts) {
				if(entry.second) {
					throw std::logic_error("left-over amount: " + entry.second.str(10) + " [" + entry.first.to_string() + "]");
				}
			}
			if(tx_cost > params->max_block_cost) {
				throw mmx::static_failure("tx cost > max_block_cost");
			}
			{
				const auto amount = (uint128_t(tx_cost) * tx->fee_ratio) / 1024;
				if(amount >> 32) {
					throw mmx::static_failure("tx fee amount overflow: " + amount.str(10));
				}
				tx_fee = amount;
			}
			if(tx_fee > tx->max_fee_amount) {
				const auto prev = tx_fee;
				tx_fee = tx->max_fee_amount;
				throw std::logic_error("tx fee > max_fee_amount: " + std::to_string(prev) + " > " + std::to_string(tx->max_fee_amount));
			}
			const auto dynamic_fee = tx_fee - static_fee;
			const auto balance = balance_cache.find(*tx->sender, addr_t());
			if(!balance || dynamic_fee > *balance) {
				throw mmx::static_failure("insufficient funds for tx fee");
			}
			*balance -= dynamic_fee;
		}
	} catch(const mmx::static_failure& ex) {
		throw;
	} catch(...) {
		failed_ex = std::current_exception();
	}
	std::shared_ptr<exec_result_t> out;

	if(!tx->exec_result) {
		out = std::make_shared<exec_result_t>();
		out->total_cost = tx_cost;
		out->total_fee = tx_fee;
		out->inputs = exec_inputs;
		out->outputs = exec_outputs;

		if(failed_ex) {
			try {
				std::rethrow_exception(failed_ex);
			} catch(const std::exception& ex) {
				out->message = ex.what();
			}
			out->did_fail = true;
		}
	} else {
		const auto result = tx->exec_result;
		if(!result->did_fail && failed_ex) {
			throw std::logic_error("unexpected execution failure");
		}
		if(result->did_fail && !failed_ex) {
			throw std::logic_error("expected failure but did not fail");
		}
		if(result->total_cost != tx_cost) {
			throw std::logic_error("tx cost mismatch: "
					+ std::to_string(result->total_cost) + " != " + std::to_string(tx_cost));
		}
		if(result->total_fee != tx_fee) {
			throw std::logic_error("tx fee mismatch: "
					+ std::to_string(result->total_fee) + " != " + std::to_string(tx_fee));
		}
		if(result->inputs.size() != exec_inputs.size()) {
			throw std::logic_error("execution input count mismatch: "
					+ std::to_string(result->inputs.size()) + " != " + std::to_string(exec_inputs.size()));
		}
		if(result->outputs.size() != exec_outputs.size()) {
			throw std::logic_error("execution output count mismatch: "
					+ std::to_string(result->outputs.size()) + " != " + std::to_string(exec_outputs.size()));
		}
		for(size_t i = 0; i < exec_inputs.size(); ++i) {
			const auto& lhs = exec_inputs[i];
			const auto& rhs = result->inputs[i];
			if(lhs.contract != rhs.contract || lhs.address != rhs.address || lhs.amount != rhs.amount || lhs.flags != rhs.flags) {
				throw std::logic_error("execution input mismatch at index " + std::to_string(i));
			}
		}
		for(size_t i = 0; i < exec_outputs.size(); ++i) {
			const auto& lhs = exec_outputs[i];
			const auto& rhs = result->outputs[i];
			if(lhs.contract != rhs.contract || lhs.address != rhs.address || lhs.amount != rhs.amount) {
				throw std::logic_error("execution output mismatch at index " + std::to_string(i));
			}
		}
	}

	if(!failed_ex) {
		storage_cache->commit();
	}
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
