/*
 * Node_validate.cpp
 *
 *  Created on: Jan 20, 2022
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/ProofOfSpaceOG.hxx>
#include <mmx/ProofOfSpaceNFT.hxx>
#include <mmx/ProofOfStake.hxx>
#include <mmx/contract/PubKey.hxx>
#include <mmx/contract/MultiSig.hxx>
#include <mmx/contract/Binary.hxx>
#include <mmx/contract/Executable.hxx>
#include <mmx/contract/VirtualPlot.hxx>
#include <mmx/operation/Execute.hxx>
#include <mmx/operation/Deposit.hxx>
#include <mmx/utils.h>
#include <mmx/vm_interface.h>
#include <mmx/exception.h>
#include <mmx/error_code_e.hxx>

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

std::shared_ptr<Node::execution_context_t> Node::new_exec_context(const uint32_t height) const
{
	auto context = std::make_shared<execution_context_t>();
	context->height = height;
	context->storage = std::make_shared<vm::StorageCache>(storage);
	return context;
}

void Node::prepare_context(std::shared_ptr<execution_context_t> context, std::shared_ptr<const Transaction> tx) const
{
	std::unordered_set<addr_t> mutate_set;
	for(const auto& op : tx->get_operations()) {
		mutate_set.insert(op->address == addr_t() ? tx->id : op->address);
	}
	auto list = std::vector<addr_t>(mutate_set.begin(), mutate_set.end());
	while(!list.empty()) {
		std::vector<addr_t> more;
		// TODO: parallelize get_contract() calls
		for(const auto& address : list) {
			auto contract = (address == tx->id ? tx->deploy : get_contract(address));
			if(auto exec = std::dynamic_pointer_cast<const contract::Executable>(contract)) {
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
		context->setup_wait(tx->id, address);
		context->mutate_map[address].push_back(tx->id);

		if(!context->storage->has_balances(address)) {
			context->storage->set_balances(address, get_balances(address));
		}
	}
	if(!mutate_set.empty()) {
		context->signal_map.emplace(tx->id, std::make_shared<waitcond_t>());
	}
}

std::shared_ptr<Node::execution_context_t> Node::validate(std::shared_ptr<const Block> block) const
{
	// Note: hash, tx_hash, tx_count, tx_cost, tx_fees and proof already verified
	// Note: Block::is_valid() and validate() already called during pre_validate_blocks()

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
	if(block->static_cost > params->max_block_size) {
		throw std::logic_error("block size too high: " + std::to_string(block->static_cost));
	}
	if(block->total_cost > params->max_block_cost) {
		throw std::logic_error("block cost too high: " + std::to_string(block->total_cost));
	}
	if(block->farmer_sig) {
		// Note: farmer_sig already verified together with proof
		validate_diff_adjust(block->time_diff, prev->time_diff);

		const auto netspace_ratio = calc_new_netspace_ratio(
				params, prev->netspace_ratio, bool(std::dynamic_pointer_cast<const ProofOfSpaceOG>(block->proof)));
		if(block->netspace_ratio != netspace_ratio) {
			throw std::logic_error("invalid netspace_ratio: " + std::to_string(block->netspace_ratio) + " != " + std::to_string(netspace_ratio));
		}
		const auto average_txfee = calc_new_average_txfee(params, prev->average_txfee, block->tx_fees);
		if(block->average_txfee != average_txfee) {
			throw std::logic_error("invalid average_txfee: " + std::to_string(block->average_txfee) + " != " + std::to_string(average_txfee));
		}
	} else {
		if(block->time_diff != prev->time_diff) {
			throw std::logic_error("invalid time_diff adjust");
		}
		if(block->netspace_ratio != prev->netspace_ratio) {
			throw std::logic_error("invalid netspace_ratio adjust");
		}
		if(block->average_txfee != prev->average_txfee) {
			throw std::logic_error("invalid average_txfee adjust");
		}
	}
	if(block->reward_addr) {
		if(auto proof = std::dynamic_pointer_cast<const ProofOfSpaceNFT>(block->proof)) {
			if(*block->reward_addr != proof->contract) {
				throw std::logic_error("invalid reward_addr for NFT proof");
			}
		}
		if(auto proof = std::dynamic_pointer_cast<const ProofOfStake>(block->proof)) {
			if(auto plot = get_contract_as<contract::VirtualPlot>(proof->contract)) {
				if(plot->reward_address) {
					if(*block->reward_addr != *plot->reward_address) {
						throw std::logic_error("invalid reward_addr for stake proof");
					}
				}
			}
		}
	}
	const auto proof_score = block->proof ? block->proof->score : params->score_threshold;
	if(block->space_diff != calc_new_space_diff(params, prev->space_diff, proof_score)) {
		throw std::logic_error("invalid space_diff adjust");
	}
	const auto diff_block = get_diff_header(block);
	const auto weight = calc_block_weight(params, diff_block, block);
	const auto total_weight = prev->total_weight + block->weight;
	if(block->weight != weight) {
		throw std::logic_error("invalid block weight: " + block->weight.str(10) + " != " + weight.str(10));
	}
	if(block->total_weight != total_weight) {
		throw std::logic_error("invalid total weight: " + block->total_weight.str(10) + " != " + total_weight.str(10));
	}

	auto context = new_exec_context(block->height);
	{
		std::unordered_set<addr_t> tx_set;
		balance_cache_t balance_cache(&balance_map);

		for(const auto& tx : block->tx_list) {
			if(!tx) {
				throw std::logic_error("null tx");
			}
			if(!check_tx_inclusion(tx->id, context->height)) {
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
				// Note: exec_inputs are checked during tx validation
				for(const auto& in : tx->inputs) {
					const auto balance = balance_cache.find(in.address, in.contract);
					if(!balance || in.amount > *balance) {
						throw std::logic_error("insufficient funds to cover input");
					}
					*balance -= in.amount;
				}
			}
			prepare_context(context, tx);
		}
	}
	hash_t failed_tx;
	std::mutex mutex;
	std::exception_ptr failed_ex;

	for(const auto& tx : block->tx_list) {
		threads->add_task([this, tx, context, &mutex, &failed_tx, &failed_ex]() {
			context->wait(tx->id);
			try {
				if(validate(tx, context)) {
					throw std::logic_error("missing exec_result");
				}
			} catch(...) {
				std::lock_guard<std::mutex> lock(mutex);
				failed_tx = tx->id;
				failed_ex = std::current_exception();
			}
			context->signal(tx->id);
		});
	}
	threads->sync();

	if(failed_ex) {
		try {
			std::rethrow_exception(failed_ex);
		} catch(const std::exception& ex) {
			throw std::logic_error(std::string(ex.what()) + " (" + failed_tx.to_string() + ")");
		}
	}
	if(block->reward_addr) {
		const auto base_allowed = calc_block_reward(block, block->tx_fees);
		if(block->reward_amount > base_allowed) {
			throw std::logic_error("invalid reward_amount: "
					+ std::to_string(block->reward_amount) + " > " + std::to_string(base_allowed));
		}
	}
	return context;
}

exec_result_t Node::validate(std::shared_ptr<const Transaction> tx) const
{
	auto context = new_exec_context(get_height() + 1);
	prepare_context(context, tx);

	const auto out = validate(tx, context);
	return out ? *out : *tx->exec_result;
}

void Node::execute(	std::shared_ptr<const Transaction> tx,
					std::shared_ptr<const execution_context_t> context,
					std::shared_ptr<const operation::Execute> op,
					std::shared_ptr<const Contract> contract,
					const addr_t& address,
					std::vector<txout_t>& exec_outputs,
					std::map<std::pair<addr_t, addr_t>, uint128>& exec_spend_map,
					std::shared_ptr<vm::StorageCache> storage_cache,
					uint64_t& tx_cost, exec_error_t& error, const bool is_public) const
{
	auto executable = std::dynamic_pointer_cast<const contract::Executable>(contract);
	if(!executable) {
		throw std::logic_error("not an executable: " + address.to_string());
	}
	auto engine = std::make_shared<vm::Engine>(address, storage_cache, false);
	{
		const auto avail_gas = (uint64_t(tx->max_fee_amount) * 1024) / tx->fee_ratio;
		engine->gas_limit = std::min(avail_gas - std::min(tx_cost, avail_gas), params->max_tx_cost);
	}
	if(op->user) {
		const auto contract = get_contract_for_ex(*op->user, &engine->gas_used, engine->gas_limit);
		contract->validate(tx->get_solution(op->solution), tx->id);

		engine->write(vm::MEM_EXTERN + vm::EXTERN_USER, vm::uint_t(op->user->to_uint256()));
	} else {
		engine->write(vm::MEM_EXTERN + vm::EXTERN_USER, vm::var_t());
	}
	engine->write(vm::MEM_EXTERN + vm::EXTERN_ADDRESS, vm::uint_t(address.to_uint256()));

	if(auto deposit = std::dynamic_pointer_cast<const operation::Deposit>(op)) {
		{
			uint128 amount = deposit->amount;
			if(auto value = storage_cache->get_balance(address, deposit->currency)) {
				amount += *value;
			}
			storage_cache->set_balance(address, deposit->currency, amount);
		}
		vm::set_deposit(engine, deposit->currency, deposit->amount);
	}
	vm::set_args(engine, op->args);

	std::exception_ptr failed_ex;
	try {
		execute(tx, context, executable, exec_outputs, exec_spend_map, storage_cache, engine, op->method, error, is_public);
	} catch(...) {
		failed_ex = std::current_exception();
	}
	// decouple gas checking from consensus by clamping cost to limit
	tx_cost += std::min(engine->gas_used, engine->gas_limit);

	if(failed_ex) {
		std::rethrow_exception(failed_ex);
	}
}

void Node::execute(	std::shared_ptr<const Transaction> tx,
					std::shared_ptr<const execution_context_t> context,
					std::shared_ptr<const contract::Executable> executable,
					std::vector<txout_t>& exec_outputs,
					std::map<std::pair<addr_t, addr_t>, uint128>& exec_spend_map,
					std::shared_ptr<vm::StorageCache> storage_cache,
					std::shared_ptr<vm::Engine> engine,
					const std::string& method_name,
					exec_error_t& error, const bool is_public) const
{
	const auto binary = get_contract_as<contract::Binary>(executable->binary, &engine->gas_used, engine->gas_limit);
	if(!binary) {
		throw std::logic_error("no such binary: " + executable->binary.to_string());
	}
	auto method = vm::find_method(binary, method_name);
	if(!method) {
		throw std::logic_error("no such method: " + method_name);
	}
	if(is_public && !method->is_public) {
		throw std::logic_error("method is not public: " + method_name);
	}
	if(!is_public && method->is_public) {
		throw std::logic_error("method is public: " + method_name);
	}
	vm::load(engine, binary);

	std::weak_ptr<vm::Engine> parent = engine;
	std::map<addr_t, std::shared_ptr<const Contract>> contract_cache;

	contract_cache[tx->id] = tx->deploy;

	engine->remote = [this, tx, context, executable, storage_cache, parent, &contract_cache, &exec_outputs, &exec_spend_map, &error]
		(const std::string& name, const std::string& method, const uint32_t nargs)
	{
		auto engine = parent.lock();
		if(!engine) {
			throw std::logic_error("!engine");
		}
		const auto address = executable->get_external(name);

		auto& fetch = contract_cache[address];
		if(!fetch) {
			fetch = get_contract_ex(address, &engine->gas_used, engine->gas_limit);
		}
		const auto contract = std::dynamic_pointer_cast<const contract::Executable>(fetch);
		if(!contract) {
			throw std::logic_error("not an executable: " + address.to_string());
		}
		engine->gas_used += params->min_txfee_exec;
		engine->check_gas();

		const auto child = std::make_shared<vm::Engine>(address, storage_cache, false);
		child->gas_limit = engine->gas_limit - std::min(engine->gas_used, engine->gas_limit);

		const auto stack_ptr = engine->get_frame().stack_ptr;
		for(uint32_t i = 0; i < nargs; ++i) {
			vm::copy(child, engine, vm::MEM_STACK + 1 + i, vm::MEM_STACK + stack_ptr + 1 + i);
		}
		child->write(vm::MEM_EXTERN + vm::EXTERN_USER, vm::uint_t(engine->contract.to_uint256()));
		child->write(vm::MEM_EXTERN + vm::EXTERN_ADDRESS, vm::uint_t(address.to_uint256()));

		execute(tx, context, contract, exec_outputs, exec_spend_map, storage_cache, child, method, error, true);

		vm::copy(engine, child, vm::MEM_STACK + stack_ptr, vm::MEM_STACK);

		engine->gas_used += child->gas_used;
		engine->check_gas();
	};

	engine->read_contract = [this, tx, executable, parent, &contract_cache]
		(const addr_t& address, const std::string& field, const uint64_t dst)
	{
		auto engine = parent.lock();
		if(!engine) {
			throw std::logic_error("!engine");
		}
		auto& contract = contract_cache[address];
		if(!contract) {
			contract = get_contract_ex(address, &engine->gas_used, engine->gas_limit);
		}
		if(!contract) {
			throw std::logic_error("no such contract: " + address.to_string());
		}
		vm::assign(engine, dst, contract->read_field(field));
		engine->check_gas();
	};

	engine->write(vm::MEM_EXTERN + vm::EXTERN_HEIGHT, vm::uint_t(context->height));
	engine->write(vm::MEM_EXTERN + vm::EXTERN_TXID, vm::uint_t(tx->id.to_uint256()));

	vm::set_balance(engine, storage_cache->get_balances(engine->contract));

	try {
		vm::execute(engine, *method);
	} catch(...) {
		error.code = engine->error_code;
		error.address = engine->error_addr;

		if(error.address < engine->code.size() && !binary->line_info.empty()) {
			auto iter = binary->line_info.upper_bound(error.address);
			if(iter != binary->line_info.begin()) {
				iter--;
				error.line = iter->second;
			}
		}
		throw;
	}

	for(const auto& out : engine->outputs) {
		{
			auto amount = storage_cache->get_balance(engine->contract, out.contract);
			if(!amount || out.amount > *amount) {
				throw std::logic_error("contract over-spend");
			}
			*amount -= out.amount;
			storage_cache->set_balance(engine->contract, out.contract, *amount);
		}
		exec_outputs.push_back(out);
		exec_spend_map[std::make_pair(engine->contract, out.contract)] += out.amount;
	}
	exec_outputs.insert(exec_outputs.end(), engine->mint_outputs.begin(), engine->mint_outputs.end());
}

std::shared_ptr<const exec_result_t>
Node::validate(	std::shared_ptr<const Transaction> tx,
				std::shared_ptr<const execution_context_t> context) const
{
	if(!tx->is_valid(params)) {
		throw mmx::static_failure("invalid tx");
	}
	if(tx->static_cost > params->max_tx_cost) {
		throw mmx::static_failure("static_cost > max_tx_cost");
	}
	if(tx_index.count(tx->id)) {
		throw mmx::static_failure("duplicate tx");
	}
	const uint64_t gas_limit = std::min((uint64_t(tx->max_fee_amount) * 1024) / tx->fee_ratio, params->max_tx_cost);
	const uint64_t static_fee = (uint64_t(tx->static_cost) * tx->fee_ratio) / 1024;

	uint64_t tx_cost = tx->static_cost;
	std::vector<txin_t> exec_inputs;
	std::vector<txout_t> exec_outputs;
	balance_cache_t balance_cache(&balance_map);
	auto storage_cache = std::make_shared<vm::StorageCache>(context->storage);
	std::unordered_map<addr_t, uint128> amounts;
	std::map<std::pair<addr_t, addr_t>, uint128> deposit_map;
	std::map<std::pair<addr_t, addr_t>, uint128> exec_spend_map;
	std::exception_ptr failed_ex;
	exec_error_t error;

	if(static_fee > tx->max_fee_amount) {
		throw mmx::static_failure("static tx fee > max_fee_amount: "
				+ std::to_string(static_fee) + " > " + std::to_string(tx->max_fee_amount));
	}
	if(!tx->sender) {
		throw mmx::static_failure("missing tx sender");
	}
	if(tx->solutions.empty()) {
		throw mmx::static_failure("missing sender signature");
	}
	{
		// validate tx sender
		auto pubkey = contract::PubKey::create();
		pubkey->address = *tx->sender;
		pubkey->validate(tx->solutions[0], tx->id);
	}

	const auto balance = balance_cache.find(*tx->sender, addr_t());
	if(!balance || static_fee > *balance) {
		error.code = error_code_e::INSUFFICIENT_FUNDS_TXFEE;
		throw mmx::static_failure("insufficient funds for tx fee: "
				+ std::to_string(static_fee) + " > " + (balance ? balance->str(10) : "0"));
	}
	*balance -= static_fee;

	try {
		if(tx->expires < context->height) {
			error.code = error_code_e::TX_EXPIRED;
			throw std::logic_error("tx expired at height " + std::to_string(tx->expires));
		}
		error.address = 0;

		for(const auto& in : tx->inputs)
		{
			const auto balance = balance_cache.find(in.address, in.contract);
			if(!balance || in.amount > *balance) {
				error.code = error_code_e::INSUFFICIENT_FUNDS;
				throw std::logic_error("insufficient funds for " + in.address.to_string());
			}
			const auto solution = tx->get_solution(in.solution);
			if(!solution) {
				throw mmx::invalid_solution("missing solution");
			}
			std::shared_ptr<const Contract> contract;

			if(in.flags & txin_t::IS_EXEC) {
				contract = get_contract_ex(in.address, &tx_cost, gas_limit);
			} else {
				auto pubkey = contract::PubKey::create();
				pubkey->address = in.address;
				contract = pubkey;
			}
			if(!contract) {
				throw std::logic_error("no such contract: " + in.address.to_string());
			}
			contract->validate(solution, tx->id);

			*balance -= in.amount;
			amounts[in.contract] += in.amount;
			error.address++;
		}
		error.address = 0;

		for(const auto& out : tx->outputs)
		{
			if(out.amount == 0) {
				throw std::logic_error("zero amount output");
			}
			auto& value = amounts[out.contract];
			if(out.amount > value) {
				throw std::logic_error("tx over-spend");
			}
			value -= out.amount;
			error.address++;
		}
		error.address = -1;

		if(tx->deploy) {
			if(!tx->deploy->is_valid()) {
				error.code = error_code_e::INVALID_CONTRACT;
				throw std::logic_error("invalid contract");
			}

			if(auto executable = std::dynamic_pointer_cast<const contract::Executable>(tx->deploy))
			{
				auto exec = operation::Execute::create();
				exec->method = executable->init_method;
				exec->args = executable->init_args;
				execute(tx, context, exec, executable, tx->id, exec_outputs, exec_spend_map, storage_cache, tx_cost, error, false);
			}
		}
		error.operation = 0;

		for(const auto& op : tx->get_operations())
		{
			if(!op || !op->is_valid()) {
				error.code = error_code_e::INVALID_OPERATION;
				throw std::logic_error("invalid operation");
			}
			const auto address = (op->address == addr_t() ? addr_t(tx->id) : op->address);
			const auto contract = (address == tx->id ? tx->deploy : get_contract_ex(address, &tx_cost, gas_limit));
			if(!contract) {
				throw std::logic_error("no such contract: " + address.to_string());
			}

			if(auto deposit = std::dynamic_pointer_cast<const operation::Deposit>(op))
			{
				auto& value = amounts[deposit->currency];
				if(deposit->amount > value) {
					throw std::logic_error("deposit over-spend");
				}
				value -= deposit->amount;

				deposit_map[std::make_pair(address, deposit->currency)] += deposit->amount;
			}
			if(auto exec = std::dynamic_pointer_cast<const operation::Execute>(op))
			{
				const auto solution = tx->get_solution(op->solution);
				execute(tx, context, exec, contract, address, exec_outputs, exec_spend_map, storage_cache, tx_cost, error, true);
			}
			error.operation++;
		}
		error.operation = -1;

		// create exec inputs
		for(auto& entry : exec_spend_map) {
			auto& amount_left = entry.second;
			{
				// use deposit amounts first
				auto& deposit = deposit_map[entry.first];
				const auto amount = std::min(deposit, amount_left);
				deposit -= amount;
				amount_left -= amount;
			}
			while(amount_left > 0) {
				txin_t in;
				in.address = entry.first.first;
				in.contract = entry.first.second;
				in.amount = amount_left >> 64 ? uint64_t(-1) : amount_left.lower();
				exec_inputs.push_back(in);
				amount_left -= in.amount;
			}
		}

		// create deposit outputs
		for(auto& entry : deposit_map) {
			auto& amount_left = entry.second;
			while(amount_left > 0) {
				txout_t out;
				out.address = entry.first.first;
				out.contract = entry.first.second;
				out.amount = amount_left >> 64 ? uint64_t(-1) : amount_left.lower();
				out.memo = memo_t("mmx.deposit");
				exec_outputs.push_back(out);
				amount_left -= out.amount;
			}
		}

		// check for left-over amounts
		for(auto& entry : amounts) {
			if(entry.second) {
				if(!tx->deploy) {
					throw std::logic_error("left-over amount without deploy");
				}
				auto& amount_left = entry.second;
				while(amount_left > 0) {
					txout_t out;
					out.address = tx->id;
					out.contract = entry.first;
					out.amount = amount_left >> 64 ? uint64_t(-1) : amount_left.lower();
					out.memo = memo_t("mmx.deposit");
					exec_outputs.push_back(out);
					amount_left -= out.amount;
				}
			}
		}

		// check for activation fee
		auto all_outputs = tx->outputs;
		all_outputs.insert(all_outputs.end(), exec_outputs.begin(), exec_outputs.end());
		for(const auto& out : all_outputs) {
			if(!balance_cache.find(out.address, out.contract)) {
				balance_cache.get(out.address, out.contract);
				tx_cost += params->min_txfee_activate;
			}
		}

		if(!tx->exec_result) {
			tx_cost += exec_inputs.size() * params->min_txfee_io;
			tx_cost += exec_outputs.size() * params->min_txfee_io;
		}
		if(tx_cost > params->max_tx_cost) {
			throw mmx::static_failure("tx cost > max_tx_cost");
		}
	} catch(const mmx::static_failure& ex) {
		throw;
	} catch(...) {
		failed_ex = std::current_exception();
	}
	uint64_t tx_fee = 0;

	try {
		uint64_t total_fee = 0;
		{
			const auto amount = (uint128_t(tx_cost) * tx->fee_ratio) / 1024;
			if(amount.upper()) {
				throw mmx::static_failure("tx fee amount overflow");
			}
			total_fee = amount;
		}
		tx_fee = std::min<uint64_t>(total_fee, tx->max_fee_amount);

		const auto dynamic_fee = tx_fee - static_fee;
		const auto balance = balance_cache.find(*tx->sender, addr_t());
		if(!balance || dynamic_fee > *balance) {
			error.code = error_code_e::INSUFFICIENT_FUNDS_TXFEE;
			throw mmx::static_failure("insufficient funds for tx fee: "
					+ std::to_string(dynamic_fee) + " > " + (balance ? balance->str(10) : "0"));
		}
		*balance -= dynamic_fee;

		if(total_fee > tx->max_fee_amount && !failed_ex) {
			error.code = error_code_e::TXFEE_OVERRUN;
			throw std::logic_error("tx fee > max_fee_amount: " + std::to_string(total_fee) + " > " + std::to_string(tx->max_fee_amount));
		}
	} catch(const mmx::static_failure& ex) {
		throw;
	} catch(...) {
		failed_ex = std::current_exception();
	}
	std::shared_ptr<exec_result_t> out;

	if(auto result = tx->exec_result) {
		if(!result->did_fail && failed_ex) {
			try {
				std::rethrow_exception(failed_ex);
			} catch(const std::exception& ex) {
				throw std::logic_error("unexpected execution failure: " + std::string(ex.what()));
			} catch(...) {
				throw std::logic_error("unexpected execution failure");
			}
		}
		if(result->did_fail && !failed_ex) {
			if(result->error) {
				throw std::logic_error("expected execution failure: " + result->error->message);
			} else {
				throw std::logic_error("expected execution failure");
			}
		}
		if(result->total_cost != tx_cost) {
			throw std::logic_error("tx cost mismatch: "
					+ std::to_string(result->total_cost) + " != " + std::to_string(tx_cost));
		}
		if(result->total_fee != tx_fee) {
			throw std::logic_error("tx fee mismatch: "
					+ std::to_string(result->total_fee) + " != " + std::to_string(tx_fee));
		}
		if(result->did_fail) {
			if(result->inputs.size() || result->outputs.size()) {
				throw std::logic_error("failed tx cannot have execution inputs / outputs");
			}
		} else {
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
				if(lhs.contract != rhs.contract || lhs.address != rhs.address || lhs.amount != rhs.amount || lhs.memo != rhs.memo) {
					throw std::logic_error("execution output mismatch at index " + std::to_string(i));
				}
			}
		}
		if(result->error) {
			if(result->error->code != error.code) {
				throw std::logic_error("error code mismatch");
			}
			if(result->error->address != error.address) {
				throw std::logic_error("error address mismatch");
			}
			if(result->error->operation != error.operation) {
				throw std::logic_error("error operation mismatch");
			}
			// Note: error line and message are not enforced by consensus
			// Note: message length already checked in is_valid()
		} else if(result->did_fail) {
			throw std::logic_error("missing error information");
		}
	} else {
		out = std::make_shared<exec_result_t>();
		out->total_cost = tx_cost;
		out->total_fee = tx_fee;

		if(failed_ex) {
			try {
				std::rethrow_exception(failed_ex);
			} catch(const std::exception& ex) {
				std::string msg = ex.what();
				msg.resize(std::min<size_t>(msg.size(), exec_error_t::MAX_MESSAGE_LENGTH));
				error.message = msg;
			}
			out->error = error;
			out->did_fail = true;
		} else {
			out->inputs = exec_inputs;
			out->outputs = exec_outputs;
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
