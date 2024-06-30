/*
 * Node_api.cpp
 *
 *  Created on: Jun 30, 2024
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/contract/PubKey.hxx>
#include <mmx/contract/Executable.hxx>
#include <mmx/contract/VirtualPlot.hxx>
#include <mmx/vm/Engine.h>
#include <mmx/vm_interface.h>

#include <mmx/Node_get_block.hxx>
#include <mmx/Node_get_block_at.hxx>
#include <mmx/Node_get_header.hxx>
#include <mmx/Node_get_header_at.hxx>
#include <mmx/Node_get_tx_ids.hxx>
#include <mmx/Node_get_tx_ids_at.hxx>
#include <mmx/Node_get_tx_ids_since.hxx>
#include <mmx/Node_get_tx_height.hxx>
#include <mmx/Node_get_tx_info.hxx>
#include <mmx/Node_get_tx_info_for.hxx>
#include <mmx/Node_get_transaction.hxx>
#include <mmx/Node_get_transactions.hxx>
#include <mmx/Node_get_history.hxx>
#include <mmx/Node_get_history_memo.hxx>
#include <mmx/Node_get_contract.hxx>
#include <mmx/Node_get_contract_for.hxx>
#include <mmx/Node_get_contracts.hxx>
#include <mmx/Node_get_contracts_by.hxx>
#include <mmx/Node_get_contracts_owned_by.hxx>
#include <mmx/Node_get_balance.hxx>
#include <mmx/Node_get_total_balance.hxx>
#include <mmx/Node_get_balances.hxx>
#include <mmx/Node_get_contract_balances.hxx>
#include <mmx/Node_get_total_balances.hxx>
#include <mmx/Node_get_all_balances.hxx>
#include <mmx/Node_get_exec_history.hxx>
#include <mmx/Node_read_storage.hxx>
#include <mmx/Node_dump_storage.hxx>
#include <mmx/Node_read_storage_var.hxx>
#include <mmx/Node_read_storage_entry_var.hxx>
#include <mmx/Node_read_storage_field.hxx>
#include <mmx/Node_read_storage_entry_addr.hxx>
#include <mmx/Node_read_storage_entry_string.hxx>
#include <mmx/Node_read_storage_array.hxx>
#include <mmx/Node_read_storage_map.hxx>
#include <mmx/Node_read_storage_object.hxx>
#include <mmx/Node_call_contract.hxx>
#include <mmx/Node_get_total_supply.hxx>
#include <mmx/Node_get_virtual_plots.hxx>
#include <mmx/Node_get_virtual_plots_for.hxx>
#include <mmx/Node_get_virtual_plots_owned_by.hxx>
#include <mmx/Node_get_offer.hxx>
#include <mmx/Node_fetch_offers.hxx>
#include <mmx/Node_get_offers.hxx>
#include <mmx/Node_get_offers_by.hxx>
#include <mmx/Node_get_recent_offers.hxx>
#include <mmx/Node_get_recent_offers_for.hxx>
#include <mmx/Node_get_trade_history.hxx>
#include <mmx/Node_get_trade_history_for.hxx>
#include <mmx/Node_get_swaps.hxx>
#include <mmx/Node_get_swap_info.hxx>
#include <mmx/Node_get_swap_user_info.hxx>
#include <mmx/Node_get_swap_history.hxx>
#include <mmx/Node_get_swap_trade_estimate.hxx>
#include <mmx/Node_get_swap_fees_earned.hxx>
#include <mmx/Node_get_swap_equivalent_liquidity.hxx>
#include <mmx/Node_get_swap_liquidity_by.hxx>
#include <mmx/Node_get_farmed_blocks.hxx>
#include <mmx/Node_get_farmed_block_count.hxx>
#include <mmx/Node_get_farmed_block_summary.hxx>

#include <vnx/vnx.h>
#include <vnx/InternalError.hxx>

#include <shared_mutex>


namespace mmx {

std::shared_ptr<const ChainParams> Node::get_params() const {
	return params;
}

std::shared_ptr<const NetworkInfo> Node::get_network_info() const
{
	if(const auto peak = get_peak()) {
		if(!network || peak->height != network->height || is_synced != network->is_synced)
		{
			auto info = NetworkInfo::create();
			info->is_synced = is_synced;
			info->height = peak->height;
			info->time_diff = peak->time_diff;
			info->space_diff = peak->space_diff;
			info->vdf_speed = (peak->time_diff / params->block_time) * (params->time_diff_constant / 1e6);
			info->block_reward = (peak->height >= params->reward_activation ? std::max(peak->next_base_reward, params->min_reward) : 0);
			info->total_space = calc_total_netspace(params, peak->space_diff);
			info->total_supply = get_total_supply(addr_t());
			info->address_count = mmx_address_count;
			info->genesis_hash = get_genesis_hash();
			info->average_txfee = peak->average_txfee;
			info->netspace_ratio = double(peak->netspace_ratio) / (uint64_t(1) << (2 * params->max_diff_adjust));
			{
				size_t num_blocks = 0;
				for(const auto& fork : get_fork_line()) {
					if(fork->block->farmer_sig) {
						info->block_size += fork->block->static_cost / double(params->max_block_size);
						num_blocks++;
					}
				}
				if(num_blocks) {
					info->block_size /= num_blocks;
				}
			}
			network = info;
		}
	}
	return network;
}

hash_t Node::get_genesis_hash() const
{
	if(auto block = get_header_at(0)) {
		return block->hash;
	}
	throw std::logic_error("have no genesis");
}

uint32_t Node::get_height() const
{
	if(auto block = get_peak()) {
		return block->height;
	}
	throw std::logic_error("have no peak");
}

vnx::optional<uint32_t> Node::get_synced_height() const
{
	if(is_synced) {
		return get_height();
	}
	return nullptr;
}

std::shared_ptr<const Block> Node::get_block(const hash_t& hash) const
{
	return std::dynamic_pointer_cast<const Block>(get_block_ex(hash, true));
}

std::shared_ptr<const BlockHeader> Node::get_block_ex(const hash_t& hash, bool full_block) const
{
	auto iter = fork_tree.find(hash);
	if(iter != fork_tree.end()) {
		const auto& block = iter->second->block;
		return full_block ? block : block->get_header();
	}
	uint32_t height = 0;
	if(hash_index.find(hash, height)) {
		return get_block_at_ex(height, full_block);
	}
	return nullptr;
}

std::shared_ptr<const Block> Node::get_block_at(const uint32_t& height) const
{
	return std::dynamic_pointer_cast<const Block>(get_block_at_ex(height, true));
}

std::shared_ptr<const BlockHeader> Node::get_block_at_ex(const uint32_t& height, bool full_block) const
{
	// THREAD SAFE (for concurrent reads)
	if(!full_block) {
		auto iter = history.find(height);
		if(iter != history.end()) {
			return iter->second;
		}
	}
	block_index_t entry;
	if(block_index.find(height, entry)) {
		vnx::File file(block_chain->get_path());
		file.open("rb");
		file.seek_to(entry.file_offset);
		return read_block(file, full_block);
	}
	return nullptr;
}

std::shared_ptr<const BlockHeader> Node::get_header(const hash_t& hash) const
{
	return get_block_ex(hash, false);
}

std::shared_ptr<const BlockHeader> Node::get_header_at(const uint32_t& height) const
{
	return get_block_at_ex(height, false);
}

vnx::optional<hash_t> Node::get_block_hash(const uint32_t& height) const
{
	if(auto hash = get_block_hash_ex(height))  {
		return hash->first;
	}
	return nullptr;
}

vnx::optional<std::pair<hash_t, hash_t>> Node::get_block_hash_ex(const uint32_t& height) const
{
	block_index_t entry;
	if(block_index.find(height, entry)) {
		return std::make_pair(entry.hash, entry.content_hash);
	}
	return nullptr;
}

std::vector<hash_t> Node::get_tx_ids(const uint32_t& limit) const
{
	std::vector<hash_t> out;
	tx_log.reverse_scan([&out, limit](const uint32_t&, const std::vector<hash_t>& list) -> bool {
		for(const auto& id : list) {
			if(out.size() < limit) {
				out.push_back(id);
			} else {
				return false;
			}
		}
		return out.size() < limit;
	});
	return out;
}

std::vector<hash_t> Node::get_tx_ids_at(const uint32_t& height) const
{
	std::vector<hash_t> list;
	tx_log.find(height, list);
	return list;
}

std::vector<hash_t> Node::get_tx_ids_since(const uint32_t& height) const
{
	std::vector<std::vector<hash_t>> list;
	tx_log.find_greater_equal(height, list);

	std::vector<hash_t> out;
	for(const auto& entry : list) {
		out.insert(out.end(), entry.begin(), entry.end());
	}
	return out;
}

vnx::optional<uint32_t> Node::get_tx_height(const hash_t& id) const
{
	tx_index_t entry;
	if(tx_index.find(id, entry)) {
		return entry.height;
	}
	return nullptr;
}

vnx::optional<tx_info_t> Node::get_tx_info(const hash_t& id) const
{
	if(auto tx = get_transaction(id, true)) {
		return get_tx_info_for(tx);
	}
	return nullptr;
}

vnx::optional<tx_info_t> Node::get_tx_info_for(std::shared_ptr<const Transaction> tx) const
{
	if(!tx) {
		return nullptr;
	}
	tx_info_t info;
	info.id = tx->id;
	info.expires = tx->expires;
	if(auto height = get_tx_height(tx->id)) {
		info.height = *height;
		info.block = get_block_hash(*height);
	}
	if(tx->exec_result) {
		info.fee = tx->exec_result->total_fee;
		info.cost = tx->exec_result->total_cost;
		info.did_fail = tx->exec_result->did_fail;
		info.message = tx->exec_result->get_error_msg();
	} else {
		info.cost = tx->static_cost;
	}
	info.note = tx->note;
	info.sender = tx->sender;
	info.inputs = tx->get_inputs();
	info.outputs = tx->get_outputs();
	info.operations = tx->get_operations();
	info.deployed = tx->deploy;

	std::unordered_set<addr_t> contracts;
	for(const auto& in : info.inputs) {
		contracts.insert(in.contract);
		info.input_amounts[in.contract] += in.amount;
	}
	for(const auto& out : info.outputs) {
		contracts.insert(out.contract);
		info.output_amounts[out.contract] += out.amount;
	}
	for(const auto& op : tx->execute) {
		if(op) {
			contracts.insert(op->address);
		}
	}
	for(const auto& addr : contracts) {
		if(auto contract = get_contract(addr)) {
			info.contracts[addr] = contract;
		}
	}
	return info;
}

std::shared_ptr<const Transaction> Node::get_transaction(const hash_t& id, const bool& include_pending) const
{
	if(include_pending) {
		auto iter = tx_pool.find(id);
		if(iter != tx_pool.end()) {
			return iter->second.tx;
		}
		for(const auto& entry : tx_queue) {
			if(const auto& tx = entry.second) {
				if(tx->id == id) {
					return tx;
				}
			}
		}
	}
	tx_index_t entry;
	if(tx_index.find(id, entry)) {
		vnx::File file(block_chain->get_path());
		file.open("rb");
		file.seek_to(entry.file_offset);
		const auto value = vnx::read(file.in);
		if(auto tx = std::dynamic_pointer_cast<const Transaction>(value)) {
			return tx;
		}
	}
	return nullptr;
}

std::vector<std::shared_ptr<const Transaction>> Node::get_transactions(const std::vector<hash_t>& ids) const
{
	std::vector<std::shared_ptr<const Transaction>> list;
	for(const auto& id : ids) {
		std::shared_ptr<const Transaction> tx;
		try {
			tx = get_transaction(id);
		} catch(...) {
			// ignore
		}
		list.push_back(tx);
	}
	return list;
}

std::vector<tx_entry_t> Node::get_history(
		const std::vector<addr_t>& addresses, const uint32_t& since, const uint32_t& until, const int32_t& limit) const
{
	struct entry_t {
		uint32_t height = 0;
		uint128_t recv = 0;
		uint128_t spent = 0;
	};
	std::map<std::tuple<addr_t, hash_t, addr_t, tx_type_e, std::string>, entry_t> delta_map;

	for(const auto& address : std::unordered_set<addr_t>(addresses.begin(), addresses.end())) {
		{
			std::vector<txio_entry_t> entries;
			recv_log.find_last_range(std::make_tuple(address, since, 0), std::make_tuple(address, until, -1), entries, size_t(limit));
			for(const auto& entry : entries) {
				tx_type_e type;
				switch(entry.type) {
					case tx_type_e::REWARD:
					case tx_type_e::VDF_REWARD:
					case tx_type_e::PROJECT_REWARD:
						type = entry.type; break;
					default: break;
				}
				const std::string memo = entry.memo ? *entry.memo : std::string();
				auto& delta = delta_map[std::make_tuple(entry.address, entry.txid, entry.contract, type, memo)];
				delta.height = entry.height;
				delta.recv += entry.amount;
			}
		}
		{
			std::vector<txio_entry_t> entries;
			spend_log.find_last_range(std::make_tuple(address, since, 0), std::make_tuple(address, until, -1), entries, size_t(limit));
			for(const auto& entry : entries) {
				tx_type_e type;
				switch(entry.type) {
					case tx_type_e::TXFEE:
						type = entry.type; break;
					default: break;
				}
				const std::string memo = entry.memo ? *entry.memo : std::string();
				auto& delta = delta_map[std::make_tuple(entry.address, entry.txid, entry.contract, type, memo)];
				delta.height = entry.height;
				delta.spent += entry.amount;
			}
		}
	}

	std::vector<tx_entry_t> res;
	for(const auto& entry : delta_map) {
		const auto& delta = entry.second;
		tx_entry_t out;
		out.height = delta.height;
		out.txid = std::get<1>(entry.first);
		out.address = std::get<0>(entry.first);
		out.contract = std::get<2>(entry.first);
		out.type = std::get<3>(entry.first);
		const auto& memo = std::get<4>(entry.first);
		if(memo.size()) {
			out.memo = memo;
		}
		if(delta.recv > delta.spent) {
			if(!out.type) {
				out.type = tx_type_e::RECEIVE;
			}
			out.amount = delta.recv - delta.spent;
		}
		if(delta.recv < delta.spent) {
			if(!out.type) {
				out.type = tx_type_e::SPEND;
			}
			out.amount = delta.spent - delta.recv;
		}
		if(out.amount) {
			res.push_back(out);
		}
	}
	std::sort(res.begin(), res.end(), [](const tx_entry_t& L, const tx_entry_t& R) -> bool {
		return std::make_tuple(L.height, L.txid, L.type, L.contract, L.address, L.memo) >
			   std::make_tuple(R.height, R.txid, R.type, R.contract, R.address, R.memo);
	});
	if(limit >= 0 && res.size() > size_t(limit)) {
		res.resize(limit);
	}
	std::reverse(res.begin(), res.end());
	return res;
}

std::vector<tx_entry_t> Node::get_history_memo(const std::vector<addr_t>& addresses, const std::string& memo, const int32_t& limit) const
{
	std::vector<tx_entry_t> res;
	for(const auto& address : std::unordered_set<addr_t>(addresses.begin(), addresses.end()))
	{
		const hash_t key(address + memo);
		std::vector<txio_entry_t> entries;
		memo_log.find_last_range(std::make_tuple(key, 0, 0), std::make_tuple(key, -1, -1), entries, size_t(limit));
		for(const auto& entry : entries) {
			tx_type_e type = tx_type_e::RECEIVE;
			switch(entry.type) {
				case tx_type_e::REWARD:
				case tx_type_e::VDF_REWARD:
				case tx_type_e::PROJECT_REWARD:
					type = entry.type; break;
				default: break;
			}
			tx_entry_t out;
			out.type = entry.type;
			out.height = entry.height;
			out.address = entry.address;
			out.contract = entry.contract;
			out.memo = entry.memo;
			out.txid = entry.txid;
			res.push_back(out);
		}
	}
	std::sort(res.begin(), res.end(), [](const tx_entry_t& L, const tx_entry_t& R) -> bool {
		return std::make_tuple(L.height, L.txid, L.type, L.contract, L.address, L.memo) <
			   std::make_tuple(R.height, R.txid, R.type, R.contract, R.address, R.memo);
	});
	return res;
}

std::shared_ptr<const Contract> Node::get_contract(const addr_t& address) const
{
	return get_contract_ex(address);
}

std::shared_ptr<const Contract> Node::get_contract_ex(const addr_t& address, uint64_t* read_cost, const uint64_t gas_limit) const
{
	if(read_cost) {
		tx_index_t index;
		tx_index.find(address, index);

		const auto cost = params->min_txfee_read + index.contract_read_cost;
		if(*read_cost + cost > gas_limit) {
			throw std::runtime_error("not enough gas to read contract");
		}
		*read_cost += cost;
	}
	std::shared_ptr<const Contract> contract;
	{
		std::lock_guard<std::mutex> lock(mutex);
		auto iter = contract_cache.find(address);
		if(iter != contract_cache.end()) {
			contract = iter->second;
		}
	}
	if(!contract) {
		contract_map.find(address, contract);

		if(std::dynamic_pointer_cast<const contract::Binary>(contract))
		{
			std::lock_guard<std::mutex> lock(mutex);
			if((contract_cache.size() + 1) >> 16) {
				contract_cache.clear();
			}
			contract_cache[address] = contract;
		}
	}
	return contract;
}

std::shared_ptr<const Contract> Node::get_contract_for(const addr_t& address) const
{
	return get_contract_for_ex(address);
}

std::shared_ptr<const Contract> Node::get_contract_for_ex(const addr_t& address, uint64_t* read_cost, const uint64_t gas_limit) const
{
	if(auto contract = get_contract_ex(address, read_cost, gas_limit)) {
		return contract;
	}
	auto pubkey = contract::PubKey::create();
	pubkey->address = address;
	return pubkey;
}

std::vector<std::shared_ptr<const Contract>> Node::get_contracts(const std::vector<addr_t>& addresses) const
{
	std::vector<std::shared_ptr<const Contract>> res;
	for(const auto& addr : addresses) {
		res.push_back(get_contract(addr));
	}
	return res;
}

std::vector<addr_t> Node::get_contracts_by(const std::vector<addr_t>& addresses, const vnx::optional<hash_t>& type_hash) const
{
	std::vector<addr_t> result;
	for(const auto& address : addresses) {
		std::vector<std::pair<addr_t, hash_t>> list;
		deploy_map.find_range(std::make_tuple(address, 0, 0), std::make_tuple(address, -1, -1), list);
		for(const auto& entry : list) {
			if(!type_hash || entry.second == *type_hash) {
				result.push_back(entry.first);
			}
		}
	}
	return result;
}

std::vector<addr_t> Node::get_contracts_owned_by(const std::vector<addr_t>& addresses, const vnx::optional<hash_t>& type_hash) const
{
	std::vector<addr_t> result;
	for(const auto& address : addresses) {
		std::vector<std::pair<addr_t, hash_t>> list;
		owner_map.find_range(std::make_tuple(address, 0, 0), std::make_tuple(address, -1, -1), list);
		for(const auto& entry : list) {
			if(!type_hash || entry.second == *type_hash) {
				result.push_back(entry.first);
			}
		}
	}
	return result;
}

uint128 Node::get_balance(const addr_t& address, const addr_t& currency) const
{
	uint128 value = 0;
	balance_table.find(std::make_pair(address, currency), value);
	return value;
}

uint128 Node::get_total_balance(const std::vector<addr_t>& addresses, const addr_t& currency) const
{
	uint128 total = 0;
	for(const auto& address : std::unordered_set<addr_t>(addresses.begin(), addresses.end())) {
		total += get_balance(address, currency);
	}
	return total;
}

std::map<addr_t, uint128> Node::get_balances(const addr_t& address) const
{
	return get_total_balances({address});
}

std::map<addr_t, balance_t> Node::get_contract_balances(const addr_t& address) const
{
	std::map<addr_t, balance_t> out;
	const auto height = get_height() + 1;
	const auto contract = get_contract(address);
	for(const auto& entry : get_total_balances({address})) {
		auto& tmp = out[entry.first];
		if(contract) {
			if(contract->is_locked(height)) {
				tmp.locked = entry.second;
			} else {
				tmp.spendable = entry.second;
			}
		} else {
			tmp.spendable = entry.second;
		}
		tmp.total = entry.second;
	}
	return out;
}

std::map<addr_t, uint128> Node::get_total_balances(const std::vector<addr_t>& addresses) const
{
	std::map<addr_t, uint128> totals;
	for(const auto& address : std::unordered_set<addr_t>(addresses.begin(), addresses.end())) {
		std::vector<std::pair<std::pair<addr_t, addr_t>, uint128>> result;
		balance_table.find_range(std::make_pair(address, addr_t()), std::make_pair(address, addr_t::ones()), result);
		for(const auto& entry : result) {
			if(entry.second) {
				totals[entry.first.second] += entry.second;
			}
		}
	}
	return totals;
}

std::map<std::pair<addr_t, addr_t>, uint128> Node::get_all_balances(const std::vector<addr_t>& addresses) const
{
	std::map<std::pair<addr_t, addr_t>, uint128> totals;
	for(const auto& address : std::unordered_set<addr_t>(addresses.begin(), addresses.end())) {
		std::vector<std::pair<std::pair<addr_t, addr_t>, uint128>> result;
		balance_table.find_range(std::make_pair(address, addr_t()), std::make_pair(address, addr_t::ones()), result);
		for(const auto& entry : result) {
			if(entry.second) {
				totals[entry.first] += entry.second;
			}
		}
	}
	return totals;
}

std::vector<exec_entry_t> Node::get_exec_history(const addr_t& address, const int32_t& limit, const vnx::bool_t& recent) const
{
	std::vector<exec_entry_t> entries;
	if(recent) {
		exec_log.find_last_range(std::make_tuple(address, 0, 0), std::make_tuple(address, -1, -1), entries, limit);
	} else {
		exec_log.find_range(std::make_tuple(address, 0, 0), std::make_tuple(address, -1, -1), entries, limit);
	}
	return entries;
}

std::map<std::string, vm::varptr_t> Node::read_storage(const addr_t& contract, const uint32_t& height) const
{
	std::map<std::string, vm::varptr_t> out;
	if(auto exec = std::dynamic_pointer_cast<const contract::Executable>(get_contract(contract))) {
		if(auto bin = std::dynamic_pointer_cast<const contract::Binary>(get_contract(exec->binary))) {
			for(const auto& entry : bin->fields) {
				if(auto var = storage->read(contract, entry.second)) {
					out[entry.first] = std::move(var);
				}
			}
		}
	}
	return out;
}

std::map<uint64_t, vm::varptr_t> Node::dump_storage(const addr_t& contract, const uint32_t& height) const
{
	const auto entries = storage->find_range(contract, vm::MEM_STATIC, -1, height);
	return std::map<uint64_t, vm::varptr_t>(entries.begin(), entries.end());
}

vm::varptr_t Node::read_storage_var(const addr_t& contract, const uint64_t& address, const uint32_t& height) const
{
	return storage->read_ex(contract, address, height);
}

vm::varptr_t Node::read_storage_entry_var(const addr_t& contract, const uint64_t& address, const uint64_t& key, const uint32_t& height) const
{
	return storage->read_ex(contract, address, key, height);
}

std::pair<vm::varptr_t, uint64_t> Node::read_storage_field(const addr_t& contract, const std::string& name, const uint32_t& height) const
{
	if(auto exec = get_contract_as<contract::Executable>(contract)) {
		if(auto bin = get_contract_as<contract::Binary>(exec->binary)) {
			if(auto addr = bin->find_field(name)) {
				return std::make_pair(read_storage_var(contract, *addr, height), *addr);
			}
		}
	}
	return {};
}

std::tuple<vm::varptr_t, uint64_t, uint64_t> Node::read_storage_entry_var(const addr_t& contract, const std::string& name, const vm::varptr_t& key, const uint32_t& height) const
{
	const auto field = read_storage_field(contract, name, height);
	if(auto var = field.first) {
		uint64_t address = field.second;
		if(var->type == vm::TYPE_REF) {
			address = vm::to_ref(var);
		}
		if(auto entry = storage->lookup(contract, key)) {
			return std::make_tuple(read_storage_entry_var(contract, address, entry, height), address, entry);
		}
	}
	return {};
}

std::tuple<vm::varptr_t, uint64_t, uint64_t> Node::read_storage_entry_addr(const addr_t& contract, const std::string& name, const addr_t& key, const uint32_t& height) const
{
	return read_storage_entry_var(contract, name, vm::to_binary(key), height);
}

std::tuple<vm::varptr_t, uint64_t, uint64_t> Node::read_storage_entry_string(const addr_t& contract, const std::string& name, const std::string& key, const uint32_t& height) const
{
	return read_storage_entry_var(contract, name, vm::to_binary(key), height);
}

std::vector<vm::varptr_t> Node::read_storage_array(const addr_t& contract, const uint64_t& address, const uint32_t& height) const
{
	return storage->read_array(contract, address, height);
}

std::map<vm::varptr_t, vm::varptr_t> Node::read_storage_map(const addr_t& contract, const uint64_t& address, const uint32_t& height) const
{
	std::map<vm::varptr_t, vm::varptr_t> out;
	if(auto exec = std::dynamic_pointer_cast<const contract::Executable>(get_contract(contract))) {
		if(auto bin = std::dynamic_pointer_cast<const contract::Binary>(get_contract(exec->binary))) {
			auto engine = std::make_shared<vm::Engine>(contract, storage, true);
			engine->gas_limit = params->max_tx_cost;
			vm::load(engine, bin);
			for(const auto& entry : storage->find_entries(contract, address, height)) {
				// need to use engine to support constant keys
				if(auto key = engine->read(entry.first)) {
					out[vm::clone(key)] = entry.second;
				}
			}
		}
	}
	return out;
}

std::map<std::string, vm::varptr_t> Node::read_storage_object(const addr_t& contract, const uint64_t& address, const uint32_t& height) const
{
	std::map<std::string, vm::varptr_t> out;
	for(const auto& entry : read_storage_map(contract, address, height)) {
		out[to_string_value(entry.first)] = entry.second;
	}
	return out;
}

vnx::Variant Node::call_contract(
		const addr_t& address, const std::string& method, const std::vector<vnx::Variant>& args,
		const vnx::optional<addr_t>& user, const vnx::optional<std::pair<addr_t, uint64_t>>& deposit) const
{
	if(auto exec = std::dynamic_pointer_cast<const contract::Executable>(get_contract(address))) {
		if(auto bin = std::dynamic_pointer_cast<const contract::Binary>(get_contract(exec->binary))) {
			auto func = vm::find_method(bin, method);
			if(!func) {
				throw std::runtime_error("no such method: " + method);
			}
			auto cache = std::make_shared<vm::StorageCache>(storage);
			auto engine = std::make_shared<vm::Engine>(address, cache, func->is_const);
			engine->gas_limit = params->max_tx_cost;
			vm::load(engine, bin);
			engine->write(vm::MEM_EXTERN + vm::EXTERN_TXID, vm::var_t());
			engine->write(vm::MEM_EXTERN + vm::EXTERN_HEIGHT, vm::uint_t(get_height()));
			engine->write(vm::MEM_EXTERN + vm::EXTERN_ADDRESS, vm::to_binary(address));
			if(user) {
				engine->write(vm::MEM_EXTERN + vm::EXTERN_USER, vm::to_binary(*user));
			} else {
				engine->write(vm::MEM_EXTERN + vm::EXTERN_USER, vm::var_t());
			}
			if(deposit) {
				vm::set_deposit(engine, deposit->first, deposit->second);
			}
			vm::set_balance(engine, get_balances(address));
			vm::set_args(engine, args);
			vm::execute(engine, *func, true);
			return vm::read(engine, vm::MEM_STACK);
		}
		throw std::runtime_error("no such binary");
	}
	throw std::runtime_error("no such contract");
}

uint128 Node::get_total_supply(const addr_t& currency) const
{
	uint128 total = 0;
	total_supply_map.find(currency, total);
	return total;
}

std::vector<virtual_plot_info_t> Node::get_virtual_plots(const std::vector<addr_t>& addresses) const
{
	std::vector<virtual_plot_info_t> result;
	for(const auto& address : addresses) {
		if(auto plot = get_contract_as<const contract::VirtualPlot>(address)) {
			virtual_plot_info_t info;
			info.address = address;
			info.farmer_key = plot->farmer_key;
			info.reward_address = plot->reward_address;
			info.balance = get_balance(address, addr_t());
			info.size_bytes = get_virtual_plot_size(params, info.balance);
			info.owner = to_addr(read_storage_field(address, "owner").first);
			result.push_back(info);
		}
	}
	return result;
}

std::vector<virtual_plot_info_t> Node::get_virtual_plots_for(const pubkey_t& farmer_key) const
{
	std::vector<addr_t> addresses;
	vplot_map.find(farmer_key, addresses);
	return get_virtual_plots(addresses);
}

std::vector<virtual_plot_info_t> Node::get_virtual_plots_owned_by(const std::vector<addr_t>& addresses) const
{
	return get_virtual_plots(get_contracts_owned_by(addresses, params->plot_binary));
}

offer_data_t Node::get_offer(const addr_t& address) const
{
	auto data = read_storage(address);
	if(data.empty()) {
		throw std::runtime_error("no such offer: " + address.to_string());
	}
	offer_data_t out;
	out.address = address;
	if(auto height = get_tx_height(address)) {
		out.height = *height;
	}
	out.owner = to_addr(data["owner"]);
	out.bid_currency = to_addr(data["bid_currency"]);
	out.ask_currency = to_addr(data["ask_currency"]);
	out.bid_balance = get_balance(address, out.bid_currency);
	out.ask_balance = get_balance(address, out.ask_currency);
	out.inv_price = to_uint(data["inv_price"]);
	out.price = pow(2, 64) / out.inv_price.to_double();
	out.ask_amount = out.get_ask_amount(out.bid_balance);
	return out;
}

int Node::get_offer_state(const addr_t& address) const
{
	if(get_balance(address, to_addr(read_storage_field(address, "bid_currency").first))) {
		return 1;
	}
	if(get_balance(address, to_addr(read_storage_field(address, "ask_currency").first))) {
		return 2;
	}
	return 0;
}

std::vector<offer_data_t> Node::fetch_offers(const std::vector<addr_t>& addresses, const vnx::bool_t& state, const vnx::bool_t& closed) const
{
	std::vector<offer_data_t> out;
	for(const auto& address : addresses) {
		const int offer_state = state ? get_offer_state(address) : -1;
		if(!state || offer_state == 1 || (closed && offer_state == 2)) {
			const auto data = get_offer(address);
			if(!data.is_scam()) {
				out.push_back(data);
			}
		}
	}
	return out;
}

std::vector<offer_data_t> Node::fetch_offers_for(	const std::vector<addr_t>& addresses,
													const vnx::optional<addr_t>& bid, const vnx::optional<addr_t>& ask,
													const bool state, const bool filter) const
{
	std::vector<offer_data_t> out;
	for(const auto& address : addresses) {
		if(!state || get_offer_state(address) == 1) {
			const auto data = get_offer(address);
			if((!bid || data.bid_currency == *bid) && (!ask || data.ask_currency == *ask)) {
				if(!filter || !data.is_scam()) {
					out.push_back(data);
				}
			}
		}
	}
	return out;
}

std::vector<offer_data_t> Node::get_offers(const uint32_t& since, const vnx::bool_t& state) const
{
	std::vector<addr_t> entries;
	contract_log.find_range(std::make_tuple(params->offer_binary, since, 0), std::make_tuple(params->offer_binary, -1, -1), entries);
	return fetch_offers(entries, state);
}

std::vector<offer_data_t> Node::get_offers_by(const std::vector<addr_t>& owners, const vnx::bool_t& state) const
{
	return fetch_offers(get_contracts_owned_by(owners, params->offer_binary), state, true);
}

std::vector<offer_data_t> Node::get_recent_offers(const int32_t& limit, const vnx::bool_t& state) const
{
	std::vector<offer_data_t> result;
	std::tuple<hash_t, uint32_t, uint32_t> offer_log_end(params->offer_binary, -1, -1);
	std::tuple<hash_t, uint32_t, uint32_t> offer_log_begin(params->offer_binary, 0, 0);

	while(result.size() < size_t(limit)) {
		std::vector<std::pair<std::tuple<hash_t, uint32_t, uint32_t>, addr_t>> entries;
		if(!contract_log.find_last_range(offer_log_begin, offer_log_end, entries, std::max<size_t>(limit, 100))) {
			break;
		}
		offer_log_end = entries.back().first;

		std::vector<addr_t> list;
		for(const auto& entry : entries) {
			list.push_back(entry.second);
		}
		const auto tmp = fetch_offers(list, state);
		result.insert(result.end(), tmp.begin(), tmp.end());
	}
	result.resize(std::min(result.size(), size_t(limit)));
	return result;
}

std::vector<offer_data_t> Node::get_recent_offers_for(
		const vnx::optional<addr_t>& bid, const vnx::optional<addr_t>& ask, const uint64_t& min_bid, const int32_t& limit, const vnx::bool_t& state) const
{
	std::vector<offer_data_t> result;
	std::unordered_set<addr_t> bid_set;
	std::unordered_set<addr_t> ask_set;
	std::unordered_set<addr_t> offer_set;
	std::tuple<addr_t, uint32_t, uint32_t> bid_history_end(bid ? *bid : addr_t(), -1, -1);
	std::tuple<addr_t, uint32_t, uint32_t> ask_history_end(ask ? *ask : addr_t(), -1, -1);

	while(result.size() < size_t(limit)) {
		std::vector<std::pair<std::tuple<addr_t, uint32_t, uint32_t>, addr_t>> bid_list;
		std::vector<std::pair<std::tuple<addr_t, uint32_t, uint32_t>, addr_t>> ask_list;
		if(bid) {
			if(offer_bid_map.find_last_range(std::make_tuple(*bid, 0, 0), bid_history_end, bid_list, std::max<size_t>(limit, 100))) {
				bid_history_end = bid_list.back().first;
			}
		}
		if(ask) {
			if(offer_ask_map.find_last_range(std::make_tuple(*ask, 0, 0), ask_history_end, ask_list, std::max<size_t>(limit, 100))) {
				ask_history_end = ask_list.back().first;
			}
		}
		std::vector<offer_data_t> tmp;
		if(bid && ask) {
			std::vector<addr_t> list;
			for(const auto& entry : bid_list) {
				bid_set.insert(entry.second);
			}
			for(const auto& entry : ask_list) {
				ask_set.insert(entry.second);
			}
			for(const auto& address : bid_set) {
				if(ask_set.count(address)) {
					list.push_back(address);
				}
			}
			for(const auto& address : list) {
				bid_set.erase(address);
				ask_set.erase(address);
			}
			tmp = fetch_offers_for(list, bid, ask, state, true);
		}
		else if(bid) {
			std::vector<addr_t> list;
			for(const auto& entry : bid_list) {
				list.push_back(entry.second);
			}
			tmp = fetch_offers_for(list, bid, ask, state, true);
		}
		else if(ask) {
			std::vector<addr_t> list;
			for(const auto& entry : ask_list) {
				list.push_back(entry.second);
			}
			tmp = fetch_offers_for(list, bid, ask, state, true);
		}
		else {
			tmp = get_recent_offers(limit, state);
		}
		for(const auto& entry : tmp) {
			if(entry.bid_balance >= min_bid) {
				if(offer_set.insert(entry.address).second) {
					result.push_back(entry);
				}
			}
		}
		if(bid_list.empty() && ask_list.empty()) {
			break;
		}
	}
	std::sort(result.begin(), result.end(),
		[](const offer_data_t& L, const offer_data_t& R) -> bool {
			return L.height > R.height;
		});
	result.resize(std::min(result.size(), size_t(limit)));
	return result;
}

std::vector<trade_entry_t> Node::get_trade_history(const int32_t& limit, const uint32_t& since) const
{
	std::vector<std::pair<std::pair<uint32_t, uint32_t>, std::tuple<addr_t, hash_t, uint64_t>>> entries;
	trade_log.find_last_range(std::make_pair(since, 0), std::make_pair(-1, -1), entries, limit);

	std::vector<trade_entry_t> result;
	for(const auto& entry : entries) {
		trade_entry_t out;
		out.height = entry.first.first;
		out.address = std::get<0>(entry.second);
		out.txid = std::get<1>(entry.second);
		out.ask_amount = std::get<2>(entry.second);
		const auto data = get_offer(out.address);
		out.bid_currency = data.bid_currency;
		out.ask_currency = data.ask_currency;
		out.bid_amount = (uint256_t(out.ask_amount) * data.inv_price) >> 64;
		out.price = data.price;
		result.push_back(out);
	}
	return result;
}

std::vector<trade_entry_t> Node::get_trade_history_for(
			const vnx::optional<addr_t>& bid, const vnx::optional<addr_t>& ask, const int32_t& limit, const uint32_t& since) const
{
	const auto entries = get_trade_history(limit > 0 && (bid || ask) ? limit * 10 : limit, since);

	std::vector<trade_entry_t> result;
	for(const auto& entry : entries) {
		if((!bid || entry.bid_currency == *bid) && (!ask || entry.ask_currency == *ask)) {
			result.push_back(entry);
		}
	}
	return result;
}

std::vector<swap_info_t> Node::get_swaps(const uint32_t& since, const vnx::optional<addr_t>& token, const vnx::optional<addr_t>& currency) const
{
	std::vector<addr_t> entries;
	contract_log.find_range(std::make_tuple(params->swap_binary, since, 0), std::make_tuple(params->swap_binary, -1, -1), entries);

	std::vector<swap_info_t> result;
	for(const auto& address : entries) {
		const auto info = get_swap_info(address);
		if((!token || info.tokens[0] == *token) && (!currency || info.tokens[1] == *currency)) {
			result.push_back(info);
		}
	}
	if(token) {
		std::sort(result.begin(), result.end(), [](const swap_info_t& L, const swap_info_t& R) -> bool {
			return L.balance[0] > R.balance[0];
		});
	} else if(currency) {
		std::sort(result.begin(), result.end(), [](const swap_info_t& L, const swap_info_t& R) -> bool {
			return L.balance[1] > R.balance[1];
		});
	} else {
		std::sort(result.begin(), result.end(), [](const swap_info_t& L, const swap_info_t& R) -> bool {
			if(!L.user_total[0] || !L.user_total[1]) {
				return false;
			}
			if(!R.user_total[0] || !R.user_total[1]) {
				return true;
			}
			return (L.avg_apy_7d[0] + L.avg_apy_7d[1]) > (R.avg_apy_7d[0] + R.avg_apy_7d[1]);
		});
	}
	return result;
}

swap_info_t Node::get_swap_info(const addr_t& address) const
{
	auto swap = get_contract_as<const contract::Executable>(address);
	if(!swap) {
		throw std::runtime_error("no such swap: " + address.to_string());
	}
	const auto height = get_height();

	swap_info_t out;
	out.name = swap->name;
	out.address = address;

	auto data = read_storage(address);
	const auto ref_volume = to_ref(data["volume"]);
	const auto tokens = read_storage_array(address, to_ref(data["tokens"]));
	const auto volume = read_storage_array(address, ref_volume);
	for(size_t i = 0; i < 2 && i < tokens.size(); ++i) {
		out.tokens[i] = to_addr(tokens[i]);
		out.volume[i] = to_uint(volume[i]);
		out.wallet[i] = get_balance(address, out.tokens[i]);
	}

	const auto fee_rates = read_storage_array(address, to_ref(data["fee_rates"]));
	for(const auto& value : fee_rates) {
		out.fee_rates.push_back(uint128(to_uint(value)).to_double() / pow(2, 64));
	}
	const auto state = read_storage_array(address, to_ref(data["state"]));

	uint256_t prev_fees_paid_1d[2] = {};
	uint256_t prev_fees_paid_7d[2] = {};

	for(const auto& entry : state) {
		auto obj = read_storage_object(address, to_ref(entry));
		const auto balance = read_storage_array(address, to_ref(obj["balance"]));
		const auto fees_paid = read_storage_array(address, to_ref(obj["fees_paid"]));
		const auto fees_claimed = read_storage_array(address, to_ref(obj["fees_claimed"]));
		const auto user_total = read_storage_array(address, to_ref(obj["user_total"]));

		swap_pool_info_t pool;
		for(size_t i = 0; i < 2 && i < balance.size(); ++i) {
			pool.balance[i] = to_uint(balance[i]);
		}
		for(size_t i = 0; i < 2 && i < fees_paid.size(); ++i) {
			pool.fees_paid[i] = to_uint(fees_paid[i]);
		}
		for(size_t i = 0; i < 2 && i < fees_claimed.size(); ++i) {
			pool.fees_claimed[i] = to_uint(fees_claimed[i]);
		}
		for(size_t i = 0; i < 2 && i < user_total.size(); ++i) {
			pool.user_total[i] = to_uint(user_total[i]);
		}
		for(size_t i = 0; i < 2; ++i) {
			out.balance[i] += pool.balance[i];
			out.fees_paid[i] += pool.fees_paid[i];
			out.fees_claimed[i] += pool.fees_claimed[i];
			out.user_total[i] += pool.user_total[i];
		}
		const auto ref_fees_paid = to_ref(obj["fees_paid"]);
		{
			const auto prev_fees_paid = read_storage_array(address, ref_fees_paid, height - std::min(8640u, height));
			for(size_t i = 0; i < 2 && i < prev_fees_paid.size(); ++i) {
				prev_fees_paid_1d[i] += to_uint(prev_fees_paid[i]);
			}
		}
		{
			const auto prev_fees_paid = read_storage_array(address, ref_fees_paid, height - std::min(60480u, height));
			for(size_t i = 0; i < 2 && i < prev_fees_paid.size(); ++i) {
				prev_fees_paid_7d[i] += to_uint(prev_fees_paid[i]);
			}
		}
		out.pools.push_back(pool);
	}

	out.volume_1d = out.volume;
	out.volume_7d = out.volume;
	{
		const auto prev_volume = read_storage_array(address, ref_volume, height - std::min(8640u, height));
		for(size_t i = 0; i < 2 && i < prev_volume.size(); ++i) {
			out.volume_1d[i] = out.volume[i] - to_uint(prev_volume[i]);
		}
	}
	{
		const auto prev_volume = read_storage_array(address, ref_volume, height - std::min(60480u, height));
		for(size_t i = 0; i < 2 && i < prev_volume.size(); ++i) {
			out.volume_7d[i] = out.volume[i] - to_uint(prev_volume[i]);
		}
	}
	for(size_t i = 0; i < 2; ++i) {
		out.avg_apy_1d[i] = uint128(365 * (out.fees_paid[i] - prev_fees_paid_1d[i])).to_double() / out.user_total[i].to_double();
	}
	for(size_t i = 0; i < 2; ++i) {
		out.avg_apy_7d[i] = uint128(52 * (out.fees_paid[i] - prev_fees_paid_7d[i])).to_double() / out.user_total[i].to_double();
	}
	return out;
}

swap_user_info_t Node::get_swap_user_info(const addr_t& address, const addr_t& user) const
{
	swap_user_info_t out;
	const auto key = storage->lookup(address, vm::to_binary(user));
	const auto users = read_storage_field(address, "users");
	const auto user_ref = storage->read(address, to_ref(users.first), key);
	if(!user_ref) {
		return out;
	}
	auto data = read_storage_object(address, to_ref(user_ref.get()));

	out.pool_idx = to_uint(data["pool_idx"]).lower().lower();
	out.unlock_height = to_uint(data["unlock_height"]);
	const auto balance = read_storage_array(address, to_ref(data["balance"]));
	const auto last_user_total = read_storage_array(address, to_ref(data["last_user_total"]));
	const auto last_fees_paid = read_storage_array(address, to_ref(data["last_fees_paid"]));
	for(size_t i = 0; i < 2 && i < balance.size(); ++i) {
		out.balance[i] = to_uint(balance[i]);
	}
	for(size_t i = 0; i < 2 && i < last_user_total.size(); ++i) {
		out.last_user_total[i] = to_uint(last_user_total[i]);
	}
	for(size_t i = 0; i < 2 && i < last_fees_paid.size(); ++i) {
		out.last_fees_paid[i] = to_uint(last_fees_paid[i]);
	}
	out.fees_earned = get_swap_fees_earned(address, user);
	out.equivalent_liquidity = get_swap_equivalent_liquidity(address, user);
	return out;
}

std::vector<swap_entry_t> Node::get_swap_history(const addr_t& address, const int32_t& limit) const
{
	const auto info = get_swap_info(address);
	std::array<std::shared_ptr<const contract::TokenBase>, 2> tokens;
	for(int i = 0; i < 2; ++i) {
		tokens[i] = get_contract_as<const contract::TokenBase>(info.tokens[i]);
	}

	std::vector<swap_entry_t> result;
	for(const auto& entry : get_exec_history(address, limit, true)) {
		swap_entry_t out;
		out.height = entry.height;
		out.txid = entry.txid;
		out.user = entry.user;
		out.index = -1;
		if(entry.method == "trade") {
			if(entry.deposit && entry.args.size() >= 2) {
				const auto index = entry.args[0].to<uint32_t>();
				out.type = (index ? "BUY" : "SELL");
				out.index = index;
				out.amount = entry.deposit->second;
				out.user = entry.args[1].to<addr_t>();
			}
		} else if(entry.method == "add_liquid" || entry.method == "rem_liquid") {
			out.type = (entry.method == "add_liquid") ? "ADD" : "REMOVE";
			if(entry.args.size() >= 1) {
				out.index = entry.args[0].to<uint32_t>();
				if(entry.deposit) {
					out.amount = entry.deposit->second;
				} else if(entry.args.size() >= 2) {
					out.amount = entry.args[1].to<uint64_t>();
				}
			}
		} else if(entry.method == "rem_all_liquid") {
			out.type = "REMOVE_ALL";
		} else if(entry.method == "payout") {
			out.type = "PAYOUT";
		} else if(entry.method == "switch_pool") {
			out.type = "SWITCH";
		}
		if(out.index < 2) {
			if(auto token = tokens[out.index]) {
				out.value = to_value(out.amount, token->decimals);
				out.symbol = token->symbol;
			} else if(info.tokens[out.index] == addr_t()) {
				out.value = to_value(out.amount, params->decimals);
				out.symbol = "MMX";
			}
		}
		result.push_back(out);
	}
	return result;
}

std::array<uint128, 2> Node::get_swap_trade_estimate(const addr_t& address, const uint32_t& i, const uint64_t& amount, const int32_t& num_iter) const
{
	const auto info = get_swap_info(address);

	std::vector<vnx::Variant> args;
	args.emplace_back(i);
	args.emplace_back(address.to_string());
	args.emplace_back(nullptr);
	args.emplace_back(num_iter);
	const auto ret = call_contract(address, "trade", args, nullptr, std::make_pair(info.tokens[i], amount)).to<std::array<uint128, 2>>();
	return {ret[0] - ret[1], ret[1]};
}

std::array<uint128, 2> Node::get_swap_fees_earned(const addr_t& address, const addr_t& user) const
{
	return call_contract(address, "get_earned_fees", {vnx::Variant(user.to_string())}).to<std::array<uint128, 2>>();
}

std::array<uint128, 2> Node::get_swap_equivalent_liquidity(const addr_t& address, const addr_t& user) const
{
	return call_contract(address, "rem_all_liquid", {vnx::Variant(true)}, user).to<std::array<uint128, 2>>();
}

std::map<addr_t, std::array<std::pair<addr_t, uint128>, 2>> Node::get_swap_liquidity_by(const std::vector<addr_t>& addresses) const
{
	std::map<addr_t, std::array<uint128, 2>> swaps;
	for(const auto& address : addresses) {
		std::vector<std::pair<std::pair<addr_t, addr_t>, std::array<uint128, 2>>> entries;
		swap_liquid_map.find_range(std::make_pair(address, addr_t()), std::make_pair(address, addr_t::ones()), entries);
		for(const auto& entry : entries) {
			auto& out = swaps[entry.first.second];
			for(int i = 0; i < 2; ++i) {
				out[i] += entry.second[i];
			}
		}
	}
	std::map<addr_t, std::array<std::pair<addr_t, uint128>, 2>> result;
	for(const auto& entry : swaps) {
		auto& out = result[entry.first];
		const auto info = get_swap_info(entry.first);
		for(int i = 0; i < 2; ++i) {
			out[i] = std::make_pair(info.tokens[i], entry.second[i]);
		}
	}
	return result;
}

std::vector<std::shared_ptr<const BlockHeader>> Node::get_farmed_blocks(
		const std::vector<pubkey_t>& farmer_keys, const vnx::bool_t& full_blocks, const uint32_t& since, const int32_t& limit) const
{
	std::vector<farmed_block_info_t> entries;
	for(const auto& key : farmer_keys) {
		std::vector<farmed_block_info_t> tmp;
		farmer_block_map.find_last(key, tmp, size_t(limit));
		for(const auto& entry : tmp) {
			if(entry.height >= since) {
				entries.push_back(entry);
			}
		}
	}
	std::sort(entries.begin(), entries.end(), [](const farmed_block_info_t& L, const farmed_block_info_t& R) -> bool {
		return L.height > R.height;
	});
	if(limit >= 0) {
		if(entries.size() > size_t(limit)) {
			entries.resize(limit);
		}
	} else {
		std::reverse(entries.begin(), entries.end());
	}
	std::vector<std::shared_ptr<const BlockHeader>> out;
	for(const auto& entry : entries) {
		out.push_back(get_block_at_ex(entry.height, full_blocks));
	}
	return out;
}

std::map<pubkey_t, uint32_t> Node::get_farmed_block_count(const uint32_t& since) const
{
	std::map<pubkey_t, uint32_t> out;
	// TODO: optimize somehow
	farmer_block_map.scan([&out, since](const pubkey_t& key, const farmed_block_info_t& info) -> bool {
		if(info.height >= since) {
			out[key]++;
		}
		return true;
	});
	return out;
}

farmed_block_summary_t Node::get_farmed_block_summary(const std::vector<pubkey_t>& farmer_keys, const uint32_t& since) const
{
	farmed_block_summary_t out;
	for(const auto& key : farmer_keys) {
		std::vector<farmed_block_info_t> tmp;
		farmer_block_map.find(key, tmp);
		for(const auto& entry : tmp) {
			if(entry.height >= since) {
				out.num_blocks++;
				out.last_height = std::max(entry.height, out.last_height);
				out.total_rewards += entry.reward;
				out.reward_map[entry.reward_addr] += entry.reward;
			}
		}
	}
	return out;
}

void Node::http_request_async(	std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
								const vnx::request_id_t& request_id) const
{
	http->http_request(request, sub_path, request_id, vnx_request->session);
}

void Node::http_request_chunk_async(std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
									const int64_t& offset, const int64_t& max_bytes, const vnx::request_id_t& request_id) const
{
	throw std::logic_error("not implemented");
}

void Node::async_api_call(std::shared_ptr<const vnx::Value> method, const vnx::request_id_t& request_id)
{
	try {
		std::shared_ptr<vnx::Value> ret;
		{
			std::shared_lock lock(db_mutex);
			ret = NodeBase::vnx_call_switch(method, request_id);
		}
		vnx_async_return(request_id, ret);
	}
	catch(const std::exception& ex) {
		vnx_async_return_ex(request_id, ex);
	}
	catch(...) {
		vnx_async_return(request_id, vnx::InternalError::create());
	}
}

std::shared_ptr<vnx::Value> Node::vnx_call_switch(std::shared_ptr<const vnx::Value> method, const vnx::request_id_t& request_id)
{
	switch(method->get_type_hash())
	{
		// Note: NOT thread-safe:
		// - http_request()
		// - get_network_info()
		// - get_synced_height()
		case Node_get_block::VNX_TYPE_ID:
		case Node_get_block_at::VNX_TYPE_ID:
		case Node_get_header::VNX_TYPE_ID:
		case Node_get_header_at::VNX_TYPE_ID:
		case Node_get_tx_ids::VNX_TYPE_ID:
		case Node_get_tx_ids_at::VNX_TYPE_ID:
		case Node_get_tx_ids_since::VNX_TYPE_ID:
		case Node_get_tx_height::VNX_TYPE_ID:
		case Node_get_tx_info::VNX_TYPE_ID:
		case Node_get_tx_info_for::VNX_TYPE_ID:
		case Node_get_transaction::VNX_TYPE_ID:
		case Node_get_transactions::VNX_TYPE_ID:
		case Node_get_history::VNX_TYPE_ID:
		case Node_get_history_memo::VNX_TYPE_ID:
		case Node_get_contract::VNX_TYPE_ID:
		case Node_get_contract_for::VNX_TYPE_ID:
		case Node_get_contracts::VNX_TYPE_ID:
		case Node_get_contracts_by::VNX_TYPE_ID:
		case Node_get_contracts_owned_by::VNX_TYPE_ID:
		case Node_get_balance::VNX_TYPE_ID:
		case Node_get_total_balance::VNX_TYPE_ID:
		case Node_get_balances::VNX_TYPE_ID:
		case Node_get_contract_balances::VNX_TYPE_ID:
		case Node_get_total_balances::VNX_TYPE_ID:
		case Node_get_all_balances::VNX_TYPE_ID:
		case Node_get_exec_history::VNX_TYPE_ID:
		case Node_read_storage::VNX_TYPE_ID:
		case Node_dump_storage::VNX_TYPE_ID:
		case Node_read_storage_var::VNX_TYPE_ID:
		case Node_read_storage_entry_var::VNX_TYPE_ID:
		case Node_read_storage_field::VNX_TYPE_ID:
		case Node_read_storage_entry_addr::VNX_TYPE_ID:
		case Node_read_storage_entry_string::VNX_TYPE_ID:
		case Node_read_storage_array::VNX_TYPE_ID:
		case Node_read_storage_map::VNX_TYPE_ID:
		case Node_read_storage_object::VNX_TYPE_ID:
		case Node_call_contract::VNX_TYPE_ID:
		case Node_get_total_supply::VNX_TYPE_ID:
		case Node_get_virtual_plots::VNX_TYPE_ID:
		case Node_get_virtual_plots_for::VNX_TYPE_ID:
		case Node_get_virtual_plots_owned_by::VNX_TYPE_ID:
		case Node_get_offer::VNX_TYPE_ID:
		case Node_fetch_offers::VNX_TYPE_ID:
		case Node_get_offers::VNX_TYPE_ID:
		case Node_get_offers_by::VNX_TYPE_ID:
		case Node_get_recent_offers::VNX_TYPE_ID:
		case Node_get_recent_offers_for::VNX_TYPE_ID:
		case Node_get_trade_history::VNX_TYPE_ID:
		case Node_get_trade_history_for::VNX_TYPE_ID:
		case Node_get_swaps::VNX_TYPE_ID:
		case Node_get_swap_info::VNX_TYPE_ID:
		case Node_get_swap_user_info::VNX_TYPE_ID:
		case Node_get_swap_history::VNX_TYPE_ID:
		case Node_get_swap_trade_estimate::VNX_TYPE_ID:
		case Node_get_swap_fees_earned::VNX_TYPE_ID:
		case Node_get_swap_equivalent_liquidity::VNX_TYPE_ID:
		case Node_get_swap_liquidity_by::VNX_TYPE_ID:
		case Node_get_farmed_blocks::VNX_TYPE_ID:
		case Node_get_farmed_block_count::VNX_TYPE_ID:
		case Node_get_farmed_block_summary::VNX_TYPE_ID:
			api_threads->add_task(std::bind(&Node::async_api_call, this, method, request_id));
			return nullptr;
		default:
			return NodeBase::vnx_call_switch(method, request_id);
	}
}


} // mmx
