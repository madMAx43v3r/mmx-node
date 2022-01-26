/*
 * Node.cpp
 *
 *  Created on: Dec 7, 2021
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/Context.hxx>
#include <mmx/Challenge.hxx>
#include <mmx/contract/Token.hxx>
#include <mmx/TimeLordClient.hxx>
#include <mmx/utxo_entry_t.hpp>
#include <mmx/stxo_entry_t.hpp>
#include <mmx/utils.h>

#include <vnx/vnx.h>

#include <atomic>
#include <algorithm>


namespace mmx {

Node::Node(const std::string& _vnx_name)
	:	NodeBase(_vnx_name)
{
	params = mmx::get_params();
}

void Node::init()
{
	vnx::open_pipe(vnx_name, this, max_queue_ms);
}

void Node::main()
{
#ifdef WITH_OPENCL
	if(opencl_device >= 0) {
		const auto devices = automy::basic_opencl::get_devices();
		if(size_t(opencl_device) < devices.size()) {
			for(int i = 0; i < 2; ++i) {
				opencl_vdf[i] = std::make_shared<OCL_VDF>(opencl_device);
			}
			log(INFO) << "Using OpenCL GPU device: " << opencl_device;
		}
		else if(devices.size()) {
			log(WARN) <<  "No such OpenCL GPU device: " << opencl_device;
		}
	}
#endif
	vdf_threads = std::make_shared<vnx::ThreadPool>(num_vdf_threads);

	if(light_mode) {
		vnx::read_config("light_address_set", light_address_set);
		log(INFO) << "Got " << light_address_set.size() << " addresses for light mode.";
	}
	router = std::make_shared<RouterAsyncClient>(router_name);
	http = std::make_shared<vnx::addons::HttpInterface<Node>>(this, vnx_name);
	add_async_client(router);
	add_async_client(http);

	vnx::Directory(storage_path).create();
	vnx::Directory(database_path).create();
	{
		::rocksdb::Options options;
		options.max_open_files = 16;
		options.keep_log_file_num = 3;
		options.max_manifest_file_size = 64 * 1024 * 1024;
		options.OptimizeForSmallDb();

		stxo_index.open(database_path + "stxo_index", options);
		saddr_map.open(database_path + "saddr_map", options);
		stxo_log.open(database_path + "stxo_log", options);
		tx_index.open(database_path + "tx_index", options);
		owner_map.open(database_path + "owner_map", options);
		tx_log.open(database_path + "tx_log", options);
	}
	block_chain = std::make_shared<vnx::File>(storage_path + "block_chain.dat");

	if(block_chain->exists()) {
		const auto time_begin = vnx::get_wall_time_millis();
		block_chain->open("rb+");
		int64_t last_pos = 0;
		while(auto block = read_block(true, &last_pos)) {
			if(block->height >= replay_height) {
				block_chain->seek_to(last_pos);
				// preemptively mark end of file since we purge DB entries now
				vnx::write(block_chain->out, nullptr);
				block_chain->seek_to(last_pos);
				break;
			}
			apply(block);
			commit(block);
		}
		if(auto peak = get_peak()) {
			log(INFO) << "Loaded " << peak->height + 1 << " blocks from disk, took " << (vnx::get_wall_time_millis() - time_begin) / 1e3 << " sec";
		}
	} else {
		block_chain->open("wb");
		block_chain->open("rb+");
	}
	is_replay = false;
	is_synced = !do_sync;

	if(state_hash == hash_t())
	{
		auto genesis = Block::create();
		genesis->time_diff = params->initial_time_diff;
		genesis->space_diff = params->initial_space_diff;
		genesis->vdf_output[0] = hash_t(params->vdf_seed);
		genesis->vdf_output[1] = hash_t(params->vdf_seed);
		genesis->finalize();

		apply(genesis);
		commit(genesis);
	}

	if(auto peak = get_peak()) {
		const auto next_height = peak->height + 1;
		{
			std::vector<txio_key_t> keys;
			if(stxo_log.find(next_height, keys, vnx::rocksdb::GREATER_EQUAL)) {
				for(const auto& key : keys) {
					stxo_index.erase(key);
				}
				log(INFO) << "Purged " << keys.size() << " STXO entries";
			}
			stxo_log.erase_all(next_height, vnx::rocksdb::GREATER_EQUAL);
		}
		{
			std::vector<hash_t> keys;
			if(tx_log.find(next_height, keys, vnx::rocksdb::GREATER_EQUAL)) {
				for(const auto& key : keys) {
					tx_index.erase(key);
				}
				log(INFO) << "Purged " << keys.size() << " TX entries";
			}
			tx_log.erase_all(next_height, vnx::rocksdb::GREATER_EQUAL);
		}
	}

	if(!light_mode) {
		subscribe(input_transactions, max_queue_ms);
	}
	subscribe(input_vdfs, max_queue_ms);
	subscribe(input_proof, max_queue_ms);
	subscribe(input_blocks, max_queue_ms);
	subscribe(input_timelord_vdfs, max_queue_ms);
	subscribe(input_harvester_proof, max_queue_ms);

	update_timer = set_timer_millis(update_interval_ms, std::bind(&Node::update, this));
	stuck_timer = set_timer_millis(sync_loss_delay * 1000, std::bind(&Node::on_stuck_timeout, this));
	set_timer_millis(60 * 1000, std::bind(&Node::print_stats, this));

	update();

	Super::main();

	vnx::write(block_chain->out, nullptr);
	block_chain->close();

	if(vdf_threads) {
		vdf_threads->close();
	}
}

std::shared_ptr<const ChainParams> Node::get_params() const {
	return params;
}

std::shared_ptr<const NetworkInfo> Node::get_network_info() const
{
	if(const auto peak = get_peak()) {
		if(!network || peak->height != network->height) {
			auto info = NetworkInfo::create();
			info->height = peak->height;
			info->time_diff = peak->time_diff;
			info->space_diff = peak->space_diff;
			info->block_reward = mmx::calc_block_reward(params, peak->space_diff);
			info->utxo_count = utxo_map.size();
			info->total_space = calc_total_netspace(params, peak->space_diff);
			{
				std::unordered_set<addr_t> addr_set;
				for(const auto& entry : utxo_map) {
					const auto& utxo = entry.second;
					if(utxo.contract == addr_t()) {
						info->total_supply += utxo.amount;
					}
					addr_set.insert(utxo.address);
				}
				info->address_count = addr_set.size();
			}
			network = info;
		}
	}
	return network;
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
	if(auto block = find_block(hash)) {
		return block;
	}
	auto iter = hash_index.find(hash);
	if(iter != hash_index.end()) {
		return get_block_at(iter->second);
	}
	return nullptr;
}

std::shared_ptr<const Block> Node::get_block_at(const uint32_t& height) const
{
	auto iter = block_index.find(height);
	if(iter != block_index.end()) {
		const auto prev_pos = block_chain->get_output_pos();
		block_chain->seek_to(iter->second.first);
		const auto block = ((Node*)this)->read_block();
		block_chain->seek_to(prev_pos);
		return block;
	}
	const auto line = get_fork_line();
	if(!line.empty()) {
		const auto offset = line[0]->block->height;
		if(height >= offset) {
			const auto index = height - offset;
			if(index < line.size()) {
				return line[index]->block;
			}
		}
	}
	return nullptr;
}

std::shared_ptr<const BlockHeader> Node::get_header(const hash_t& hash) const
{
	if(auto header = find_header(hash)) {
		return header;
	}
	if(auto block = get_block(hash)) {
		return block->get_header();
	}
	return nullptr;
}

std::shared_ptr<const BlockHeader> Node::get_header_at(const uint32_t& height) const
{
	{
		auto iter = history.find(height);
		if(iter != history.end()) {
			return iter->second;
		}
	}
	if(auto block = get_block_at(height)) {
		return block->get_header();
	}
	return nullptr;
}

vnx::optional<hash_t> Node::get_block_hash(const uint32_t& height) const
{
	auto iter = block_index.find(height);
	if(iter != block_index.end()) {
		return iter->second.second;
	}
	if(auto block = get_block_at(height)) {
		return block->hash;
	}
	return nullptr;
}

std::vector<hash_t> Node::get_tx_ids_at(const uint32_t& height) const
{
	if(auto peak = get_peak()) {
		if(height <= peak->height) {
			const auto offset = peak->height - height;
			if(offset < change_log.size()) {
				auto iter = change_log.rbegin();
				std::advance(iter, offset);
				return (*iter)->tx_added;
			}
		}
	}
	std::vector<hash_t> list;
	tx_log.find(height, list);
	return list;
}

vnx::optional<uint32_t> Node::get_tx_height(const hash_t& id) const
{
	{
		auto iter = tx_map.find(id);
		if(iter != tx_map.end()) {
			return iter->second;
		}
	}
	{
		std::pair<int64_t, uint32_t> entry;
		if(tx_index.find(id, entry)) {
			return entry.second;
		}
	}
	return nullptr;
}

vnx::optional<tx_info_t> Node::get_tx_info(const hash_t& id) const
{
	if(auto tx = get_transaction(id, true)) {
		tx_info_t info;
		info.id = id;
		if(auto height = get_tx_height(id)) {
			info.height = *height;
			info.block = get_block_hash(*height);
		}
		info.cost = tx->calc_cost(params);
		info.operations = tx->execute;
		info.deployed = tx->deploy;

		std::unordered_set<addr_t> contracts;
		for(const auto& in : tx->inputs) {
			txi_info_t entry;
			entry.prev = in.prev;
			if(auto txo = get_txo_info(in.prev)) {
				const auto& utxo = txo->output;
				entry.utxo = utxo;
				contracts.insert(utxo.contract);
				info.input_amounts[utxo.contract] += utxo.amount;
			}
			info.inputs.push_back(entry);
		}
		for(size_t i = 0; i < tx->outputs.size() + tx->exec_outputs.size(); ++i) {
			txo_info_t entry;
			if(auto txo = get_txo_info(txio_key_t::create_ex(id, i))) {
				entry = *txo;
				info.output_amounts[txo->output.contract] += txo->output.amount;
			}
			info.outputs.push_back(entry);
			contracts.insert(entry.output.contract);
		}
		for(const auto& op : tx->execute) {
			contracts.insert(op->address);
		}
		for(const auto& addr : contracts) {
			if(auto contract = get_contract(addr)) {
				info.contracts[addr] = contract;
			}
		}
		auto iter = info.input_amounts.find(addr_t());
		auto iter2 = info.output_amounts.find(addr_t());
		info.fee = int64_t(iter != info.input_amounts.end() ? iter->second : 0)
						- (iter2 != info.output_amounts.end() ? iter2->second : 0);
		return info;
	}
	return nullptr;
}

vnx::optional<txo_info_t> Node::get_txo_info(const txio_key_t& key) const
{
	{
		auto iter = utxo_map.find(key);
		if(iter != utxo_map.end()) {
			txo_info_t info;
			info.output = iter->second;
			return info;
		}
	}
	{
		stxo_t stxo;
		if(stxo_index.find(key, stxo)) {
			txo_info_t info;
			info.output = stxo;
			info.spent = stxo.spent;
			return info;
		}
	}
	for(const auto& log : change_log) {
		auto iter = log->utxo_removed.find(key);
		if(iter != log->utxo_removed.end()) {
			txo_info_t info;
			info.output = iter->second;
			info.spent = iter->second.spent;
			return info;
		}
	}
	if(auto tx = get_transaction(key.txid)) {
		txo_info_t info;
		info.output = utxo_t::create_ex(tx->get_output(key.index));
		return info;
	}
	return nullptr;
}

std::vector<vnx::optional<txo_info_t>> Node::get_txo_infos(const std::vector<txio_key_t>& keys) const
{
	std::vector<vnx::optional<txo_info_t>> res;
	for(const auto& key : keys) {
		res.push_back(get_txo_info(key));
	}
	return res;
}

std::shared_ptr<const Transaction> Node::get_transaction(const hash_t& id, const vnx::bool_t& include_pending) const
{
	// THREAD SAFE (for concurrent reads)
	if(include_pending || tx_map.count(id))
	{
		auto iter = tx_pool.find(id);
		if(iter != tx_pool.end()) {
			return iter->second;
		}
	}
	{
		std::pair<int64_t, uint32_t> entry;
		if(tx_index.find(id, entry)) {
			vnx::File file(block_chain->get_path());
			file.open("rb");
			file.seek_to(entry.first);
			const auto value = vnx::read(file.in);
			if(auto tx = std::dynamic_pointer_cast<Transaction>(value)) {
				return tx;
			}
			if(auto header = std::dynamic_pointer_cast<BlockHeader>(value)) {
				if(auto tx = header->tx_base) {
					if(tx->id == id) {
						return std::dynamic_pointer_cast<const Transaction>(tx);
					}
				}
			}
		}
	}
	return nullptr;
}

std::vector<std::shared_ptr<const Transaction>> Node::get_transactions(const std::vector<hash_t>& ids) const
{
	std::vector<std::shared_ptr<const Transaction>> list;
	for(const auto& id : ids) {
		try {
			list.push_back(get_transaction(id));
		} catch(...) {
			// ignore
		}
	}
	return list;
}

std::vector<tx_entry_t> Node::get_history_for(const std::vector<addr_t>& addresses, const int32_t& since) const
{
	const uint32_t height = get_height();
	const uint32_t min_height = since >= 0 ? since : std::max<int32_t>(height + since, 0);
	const std::unordered_set<addr_t> addr_set(addresses.begin(), addresses.end());

	struct txio_t {
		std::vector<utxo_t> outputs;
		std::unordered_map<size_t, stxo_entry_t> inputs;
	};
	std::unordered_map<hash_t, txio_t> txio_map;

	for(const auto& entry : get_utxo_list(addresses, 1)) {
		if(entry.output.height >= min_height) {
			txio_map[entry.key.txid].outputs.push_back(entry.output);
		}
	}
	for(const auto& entry : get_stxo_list(addresses)) {
		if(entry.output.height >= min_height) {
			txio_map[entry.key.txid].outputs.push_back(entry.output);
		}
		txio_map[entry.spent.txid].inputs[entry.spent.index] = entry;
	}
	std::multimap<uint32_t, tx_entry_t> list;

	for(const auto& iter : txio_map) {
		const auto& txio = iter.second;

		vnx::optional<uint32_t> height;
		if(!txio.inputs.empty()) {
			height = get_tx_height(iter.first);
		}
		if(height && *height < min_height) {
			continue;
		}
		std::unordered_map<hash_t, int64_t> amount;
		for(const auto& utxo : txio.outputs) {
			amount[utxo.contract] += utxo.amount;
		}
		for(const auto& entry : txio.inputs) {
			const auto& utxo = entry.second.output;
			amount[utxo.contract] -= utxo.amount;
		}
		for(const auto& entry : txio.inputs) {
			const auto& utxo = entry.second.output;
			if(amount[utxo.contract] > 0 && height) {
				tx_entry_t entry;
				entry.height = *height;
				entry.txid = iter.first;
				entry.type = tx_type_e::SPEND;
				entry.contract = utxo.contract;
				entry.address = utxo.address;
				entry.amount = utxo.amount;
				list.emplace(entry.height, entry);
			}
		}
		for(const auto& utxo : txio.outputs) {
			if(amount[utxo.contract] > 0) {
				tx_entry_t entry;
				entry.height = utxo.height;
				entry.txid = iter.first;
				entry.type = tx_type_e::RECEIVE;
				entry.contract = utxo.contract;
				entry.address = utxo.address;
				entry.amount = utxo.amount;
				list.emplace(entry.height, entry);
			}
		}
		if(!txio.inputs.empty() && height) {
			if(auto tx = get_transaction(iter.first)) {
				for(size_t i = 0; i < tx->inputs.size(); ++i) {
					if(!txio.inputs.count(i)) {
						const auto& in = tx->inputs[i];
						if(auto info = get_txo_info(in.prev)) {
							const auto& utxo = info->output;
							if(amount[utxo.contract] < 0 && !addr_set.count(utxo.address)) {
								tx_entry_t entry;
								entry.height = *height;
								entry.txid = tx->id;
								entry.type = tx_type_e::INPUT;
								entry.contract = utxo.contract;
								entry.address = utxo.address;
								entry.amount = utxo.amount;
								list.emplace(entry.height, entry);
							}
						}
					}
				}
				for(const auto& utxo : tx->outputs) {
					if(amount[utxo.contract] < 0 && !addr_set.count(utxo.address)) {
						tx_entry_t entry;
						entry.height = *height;
						entry.txid = tx->id;
						entry.type = tx_type_e::SEND;
						entry.contract = utxo.contract;
						entry.address = utxo.address;
						entry.amount = utxo.amount;
						list.emplace(entry.height, entry);
					}
				}
			}
		}
	}
	std::vector<tx_entry_t> res;
	for(const auto& entry : list) {
		res.push_back(entry.second);
	}
	return res;
}

std::shared_ptr<const Contract> Node::get_contract(const addr_t& address) const
{
	// THREAD SAFE
	{
		std::shared_lock lock(cache_mutex);
		auto iter = contract_map.find(address);
		if(iter != contract_map.end()) {
			return iter->second;
		}
	}
	std::shared_ptr<const Contract> contract;
	if(auto tx = get_transaction(address)) {
		contract = tx->deploy;
	}
	{
		std::unique_lock lock(cache_mutex);
		if(contract_map.emplace(address, contract).second) {
			contract_cache_queue.push(address);
			if(contract_cache_queue.size() > max_contract_cache) {
				contract_map.erase(contract_cache_queue.front());
				contract_cache_queue.pop();
			}
		}
	}
	return contract;
}

std::vector<std::shared_ptr<const Contract>> Node::get_contracts(const std::vector<addr_t>& addresses) const
{
	std::vector<std::shared_ptr<const Contract>> res;
	for(const auto& addr : addresses) {
		res.push_back(get_contract(addr));
	}
	return res;
}

std::map<addr_t, std::shared_ptr<const Contract>> Node::get_contracts_owned(const std::vector<addr_t>& owners) const
{
	std::map<addr_t, std::shared_ptr<const Contract>> res;
	for(const auto& owner : owners) {
		std::vector<addr_t> list;
		owner_map.find(owner, list);
		for(const auto& addr : list) {
			if(auto contract = get_contract(addr)) {
				res.emplace(addr, contract);
			}
		}
	}
	const std::unordered_set<addr_t> owner_set(owners.begin(), owners.end());
	for(const auto& log : change_log) {
		for(const auto& entry : log->deployed) {
			if(auto owner = entry.second->get_owner()) {
				if(owner_set.count(*owner)) {
					res.emplace(entry.first, entry.second);
				}
			}
		}
	}
	return res;
}

bool Node::include_transaction(std::shared_ptr<const Transaction> tx)
{
	for(const auto& in : tx->inputs) {
		if(utxo_map.count(in.prev)) {
			return true;
		}
	}
	for(const auto& out : tx->outputs) {
		if(light_address_set.count(out.address)) {
			return true;
		}
	}
	for(const auto& out : tx->exec_outputs) {
		if(light_address_set.count(out.address)) {
			return true;
		}
	}
	for(const auto& op : tx->execute) {
		if(light_address_set.count(op->address)) {
			return true;
		}
	}
	if(const auto& contract = tx->deploy) {
		if(auto owner = contract->get_owner()) {
			if(light_address_set.count(*owner)) {
				return true;
			}
		}
		if(std::dynamic_pointer_cast<const contract::Token>(contract)) {
			return true;
		}
	}
	return false;
}

void Node::add_block(std::shared_ptr<const Block> block)
{
	if(fork_tree.count(block->hash)) {
		return;
	}
	const auto root = get_root();
	if(block->height <= root->height) {
		return;
	}
	if(!block->is_valid()) {
		return;
	}
	if(light_mode) {
		auto copy = vnx::clone(block);
		if(auto tx = std::dynamic_pointer_cast<const Transaction>(copy->tx_base)) {
			if(!include_transaction(tx)) {
				auto dummy = TransactionBase::create();
				dummy->id = tx->id;
				copy->tx_base = dummy;
			}
		}
		for(auto& base : copy->tx_list) {
			if(auto tx = std::dynamic_pointer_cast<const Transaction>(base)) {
				if(!include_transaction(tx)) {
					auto dummy = TransactionBase::create();
					dummy->id = tx->id;
					base = dummy;
				}
			}
		}
		block = copy;
	}
	auto fork = std::make_shared<fork_t>();
	fork->recv_time = vnx::get_wall_time_micros();
	fork->block = block;
	add_fork(fork);

	// fetch missing previous
	if(is_synced && !fork_tree.count(block->prev))
	{
		const auto height = block->height - 1;
		if(!sync_pending.count(height) && height > root->height)
		{
			router->get_blocks_at(height, std::bind(&Node::sync_result, this, height, std::placeholders::_1));
			sync_pending.insert(height);
			log(WARN) << "Fetching missed block at height " << height << " with hash " << block->prev;
		}
	}
}

void Node::add_transaction(std::shared_ptr<const Transaction> tx, const vnx::bool_t& pre_validate)
{
	if(tx_pool.size() >= tx_pool_limit) {
		return;
	}
	if(tx_pool.count(tx->id)) {
		return;
	}
	if(!tx->is_valid()) {
		return;
	}
	if(pre_validate) {
		validate(tx);
	}
	tx_pool[tx->id] = tx;

	if(!vnx_sample) {
		publish(tx, output_transactions);
	}
}

uint64_t Node::get_balance(const addr_t& address, const addr_t& contract, const uint32_t& min_confirm) const
{
	return get_total_balance({address}, contract, min_confirm);
}

uint64_t Node::get_total_balance(const std::vector<addr_t>& addresses, const addr_t& contract, const uint32_t& min_confirm) const
{
	uint64_t total = 0;
	for(const auto& entry : get_utxo_list(addresses, min_confirm)) {
		const auto& utxo = entry.output;
		if(utxo.contract == contract) {
			total += utxo.amount;
		}
	}
	return total;
}

std::map<addr_t, uint64_t> Node::get_total_balances(const std::vector<addr_t>& addresses, const uint32_t& min_confirm) const
{
	std::map<addr_t, uint64_t> amounts;
	for(const auto& entry : get_utxo_list(addresses, min_confirm)) {
		const auto& utxo = entry.output;
		amounts[utxo.contract] += utxo.amount;
	}
	return amounts;
}

uint64_t Node::get_total_supply(const addr_t& contract) const
{
	uint64_t total = 0;
	for(const auto& entry : utxo_map) {
		const auto& utxo = entry.second;
		if(utxo.contract == contract) {
			total += utxo.amount;
		}
	}
	return total;
}

std::vector<utxo_entry_t> Node::get_utxo_list(const std::vector<addr_t>& addresses, const uint32_t& min_confirm) const
{
	const auto height = get_height();
	const std::unordered_set<addr_t> addr_set(addresses.begin(), addresses.end());

	std::vector<utxo_entry_t> res;
	for(const auto& addr : addr_set) {
		const auto begin = addr_map.lower_bound(std::make_pair(addr, txio_key_t()));
		const auto end   = addr_map.upper_bound(std::make_pair(addr, txio_key_t::create_ex(hash_t::ones(), -1)));
		for(auto iter = begin; iter != end; ++iter) {
			auto iter2 = utxo_map.find(iter->second);
			if(iter2 != utxo_map.end()) {
				const auto& utxo = iter2->second;
				if((height - utxo.height) + 1 >= min_confirm) {
					res.push_back(utxo_entry_t::create_ex(iter2->first, utxo));
				}
			}
		}
		auto iter = taddr_map.find(addr);
		if(iter != taddr_map.end()) {
			for(const auto& key : iter->second) {
				auto iter2 = utxo_map.find(key);
				if(iter2 != utxo_map.end()) {
					const auto& utxo = iter2->second;
					if((height - utxo.height) + 1 >= min_confirm) {
						res.push_back(utxo_entry_t::create_ex(iter2->first, utxo));
					}
				}
			}
		}
	}
	return res;
}

std::vector<stxo_entry_t> Node::get_stxo_list(const std::vector<addr_t>& addresses) const
{
	const std::unordered_set<addr_t> addr_set(addresses.begin(), addresses.end());

	std::vector<stxo_entry_t> res;
	for(const auto& addr : addr_set) {
		std::vector<txio_key_t> keys;
		saddr_map.find(addr, keys);
		for(const auto& key : std::unordered_set<txio_key_t>(keys.begin(), keys.end())) {
			stxo_t stxo;
			if(stxo_index.find(key, stxo)) {
				res.push_back(stxo_entry_t::create_ex(key, stxo));
			}
		}
	}
	for(const auto& log : change_log) {
		for(const auto& entry : log->utxo_removed) {
			const auto& stxo = entry.second;
			if(addr_set.count(stxo.address)) {
				res.push_back(stxo_entry_t::create_ex(entry.first, stxo));
			}
		}
	}
	return res;
}

void Node::http_request_async(	std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
								const vnx::request_id_t& request_id) const
{
	http->http_request(request, sub_path, request_id);
}

void Node::http_request_chunk_async(std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
									const int64_t& offset, const int64_t& max_bytes, const vnx::request_id_t& request_id) const
{
	throw std::logic_error("not implemented");
}

void Node::handle(std::shared_ptr<const Block> block)
{
	if(!is_synced) {
		const auto peak = get_peak();
		if(peak && block->height > peak->height && block->height - peak->height > 2 * params->commit_delay) {
			return;
		}
	}
	if(block->proof) {
		add_block(block);
	}
}

void Node::handle(std::shared_ptr<const Transaction> tx)
{
	if(!is_synced) {
		return;
	}
	add_transaction(tx);
}

void Node::handle(std::shared_ptr<const ProofOfTime> proof)
{
	const auto peak = get_peak();
	if(peak && proof->height > peak->height && proof->height - peak->height > 2 * params->commit_delay) {
		return;
	}
	if(find_vdf_point(	proof->height, proof->start, proof->get_vdf_iters(),
						proof->input, {proof->get_output(0), proof->get_output(1)}))
	{
		return;		// already verified
	}
	if(vdf_verify_pending.count(proof->height)) {
		pending_vdfs.emplace(proof->height, proof);
		return;
	}
	try {
		vdf_verify_pending.insert(proof->height);
		verify_vdf(proof);
	}
	catch(const std::exception& ex) {
		vdf_verify_pending.erase(proof->height);
		if(is_synced) {
			log(WARN) << "VDF verification for height " << proof->height << " failed with: " << ex.what();
		}
	}
}

void Node::handle(std::shared_ptr<const ProofResponse> value)
{
	if(!is_synced || !value->proof || !value->request) {
		return;
	}
	const auto peak = get_peak();
	const auto request = value->request;
	if(!peak || request->height < peak->height) {
		return;
	}
	const auto challenge = request->challenge;
	try {
		const auto diff_block = find_diff_header(peak, request->height - peak->height);
		if(!diff_block) {
			throw std::logic_error("cannot verify");
		}
		if(request->space_diff != diff_block->space_diff) {
			throw std::logic_error("invalid space_diff");
		}
		auto response = vnx::clone(value);
		response->score = verify_proof(value->proof, challenge, diff_block->space_diff);

		if(response->score >= params->score_threshold) {
			throw std::logic_error("invalid score");
		}
		auto iter = proof_map.find(challenge);
		if(iter == proof_map.end() || response->score < iter->second->score)
		{
			if(iter == proof_map.end()) {
				challenge_map.emplace(request->height, challenge);
			}
			proof_map[challenge] = response;

			log(DEBUG) << "Got new best proof for height " << request->height << " with score " << response->score;
		}
		publish(response, output_verified_proof);
	}
	catch(const std::exception& ex) {
		log(WARN) << "Got invalid proof: " << ex.what();
	}
}

void Node::print_stats()
{
	 log(INFO) << tx_pool.size() << " tx pool, " << contract_map.size() << " contracts, "
			 << utxo_map.size() << " utxo, " << change_log.size() << " / " << fork_tree.size() << " blocks";
}

void Node::on_stuck_timeout()
{
	if(is_synced) {
		log(WARN) << "Lost sync due to progress timeout!";
	}
	start_sync(false);
}

void Node::start_sync(const vnx::bool_t& force)
{
	if((!is_synced || !do_sync) && !force) {
		return;
	}
	sync_pos = 0;
	sync_peak = nullptr;
	sync_retry = 0;
	is_synced = false;

	try {
		TimeLordClient timelord(timelord_name);
		timelord.stop_vdf();
		log(INFO) << "Stopped TimeLord";
	} catch(...) {
		// ignore
	}
	sync_more();
}

void Node::sync_more()
{
	if(is_synced) {
		return;
	}
	if(!sync_pos) {
		sync_pos = get_root()->height + 1;
		log(INFO) << "Starting sync at height " << sync_pos;
	}
	if(vdf_threads->get_num_pending()) {
		return;
	}
	const auto max_pending = !sync_retry ? max_sync_jobs : 1;
	while(sync_pending.size() < max_pending) {
		if(sync_peak && sync_pos >= *sync_peak) {
			break;
		}
		const auto height = sync_pos++;
		router->get_blocks_at(height, std::bind(&Node::sync_result, this, height, std::placeholders::_1));
		sync_pending.insert(height);
	}
}

void Node::sync_result(const uint32_t& height, const std::vector<std::shared_ptr<const Block>>& blocks)
{
	sync_pending.erase(height);

	for(auto block : blocks) {
		if(block) {
			add_block(block);
		}
	}
	if(!is_synced) {
		if(blocks.empty()) {
			if(!sync_peak || height < *sync_peak) {
				sync_peak = height;
			}
		}
		if(!sync_retry && height % 64 == 0) {
			add_task(std::bind(&Node::update, this));
		}
		sync_more();
	}
}

std::shared_ptr<const BlockHeader> Node::fork_to(const hash_t& state)
{
	if(auto fork = find_fork(state)) {
		return fork_to(fork);
	}
	if(auto root = get_root()) {
		if(state == root->hash) {
			while(revert());
			return nullptr;
		}
	}
	throw std::logic_error("cannot fork");
}

std::shared_ptr<const BlockHeader> Node::fork_to(std::shared_ptr<fork_t> fork_head)
{
	const auto prev_state = state_hash;
	const auto fork_line = get_fork_line(fork_head);

	bool did_fork = false;
	std::shared_ptr<const BlockHeader> forked_at;

	// bring state back in line
	while(true) {
		bool found = false;
		for(const auto& fork : fork_line)
		{
			const auto& block = fork->block;
			if(block->hash == state_hash) {
				found = true;
				forked_at = block;
				break;
			}
		}
		if(found) {
			break;
		}
		did_fork = true;

		if(!revert()) {
			forked_at = get_root();
			break;
		}
	}

	// verify and apply
	for(const auto& fork : fork_line)
	{
		const auto& block = fork->block;
		if(block->prev != state_hash) {
			// already verified and applied
			continue;
		}
		if(!fork->is_verified) {
			try {
				if(!light_mode) {
					validate(block);
				}
				if(!fork->is_vdf_verified) {
					if(auto prev = find_prev_header(block)) {
						if(auto infuse = find_prev_header(block, params->finality_delay + 1, true)) {
							log(INFO) << "Checking VDF for block at height " << block->height << " ...";
							vdf_threads->add_task(std::bind(&Node::check_vdf_task, this, fork, prev, infuse));
						} else {
							throw std::logic_error("cannot verify");
						}
					} else {
						throw std::logic_error("cannot verify");
					}
				}
				fork->is_verified = true;
			}
			catch(const std::exception& ex) {
				log(WARN) << "Block verification failed for height " << block->height << " with: " << ex.what();
				fork->is_invalid = true;
				fork_to(prev_state);
				throw;
			}
			if(is_synced) {
				if(auto point = fork->vdf_point) {
					if(auto proof = point->proof) {
						publish(proof, output_verified_vdfs);
					}
				}
				publish(block, output_verified_blocks);
			}
		}
		apply(block);
	}
	return did_fork ? forked_at : nullptr;
}

std::shared_ptr<Node::fork_t> Node::find_best_fork(std::shared_ptr<const BlockHeader> root, const uint32_t* at_height) const
{
	if(!root) {
		root = get_root();
	}
	uint32_t curr_height = 0;
	uint128_t max_weight = 0;
	std::shared_ptr<fork_t> best_fork;
	std::shared_ptr<fork_t> prev_best;
	const auto begin = at_height ? fork_index.lower_bound(*at_height) : fork_index.upper_bound(root->height);
	const auto end =   at_height ? fork_index.upper_bound(*at_height) : fork_index.end();
	for(auto iter = begin; iter != end; ++iter)
	{
		const auto& fork = iter->second;
		const auto prev = fork->prev.lock();
		if(prev && prev->is_invalid) {
			fork->is_invalid = true;
		}
		if(!fork->is_proof_verified || fork->is_invalid) {
			continue;
		}
		if(iter->first != curr_height) {
			prev_best = best_fork;
			curr_height = iter->first;
		}
		if(prev) {
			fork->total_weight = prev->total_weight;
			if(!prev_best || prev == prev_best || !fork->vdf_point || prev_best->recv_time > fork->vdf_point->recv_time) {
				if(!fork->has_weak_proof) {
					fork->total_weight += std::min<int32_t>(prev->weight_buffer, params->score_threshold);
				}
				fork->total_weight += fork->weight;
			} else {
				fork->total_weight -= params->score_threshold;		// orphan penalty
			}
			fork->weight_buffer = std::min<int32_t>(std::max(prev->weight_buffer + fork->buffer_delta, 0), params->max_weight_buffer);
		} else {
			fork->total_weight = fork->weight;
		}
		if(fork->total_weight >> 127) {
			fork->total_weight = 0;		// clamp to zero
		}
		if(!best_fork
			|| fork->total_weight > max_weight
			|| (fork->total_weight == max_weight && fork->block->hash < best_fork->block->hash))
		{
			best_fork = fork;
			max_weight = fork->total_weight;
		}
	}
	return best_fork;
}

std::vector<std::shared_ptr<Node::fork_t>> Node::get_fork_line(std::shared_ptr<fork_t> fork_head) const
{
	std::vector<std::shared_ptr<fork_t>> line;
	auto fork = fork_head ? fork_head : find_fork(state_hash);
	while(fork) {
		line.push_back(fork);
		fork = fork->prev.lock();
	}
	std::reverse(line.begin(), line.end());
	return line;
}

void Node::commit(std::shared_ptr<const Block> block) noexcept
{
	if(change_log.empty()) {
		return;
	}
	if(!history.empty() && block->prev != get_root()->hash) {
		return;
	}
	const auto log = change_log.front();

	for(const auto& entry : log->utxo_removed) {
		const auto& stxo = entry.second;
		if(!is_replay) {
			stxo_log.insert(block->height, entry.first);
			stxo_index.insert(entry.first, entry.second);
			saddr_map.insert(stxo.address, entry.first);
		}
		addr_map.erase(std::make_pair(stxo.address, entry.first));
	}
	for(const auto& entry : log->utxo_added) {
		const auto& utxo = entry.second;
		addr_map.emplace(utxo.address, entry.first);
		taddr_map[utxo.address].erase(entry.first);
	}
	for(const auto& txid : log->tx_added) {
		tx_map.erase(txid);
		tx_pool.erase(txid);
	}
	if(!is_replay) {
		for(const auto& entry : log->deployed) {
			if(auto owner = entry.second->get_owner()) {
				owner_map.insert(*owner, entry.first);
			}
		}
	}
	if(block->height % 16 == 0) {
		for(auto iter = taddr_map.begin(); iter != taddr_map.end();) {
			if(iter->second.empty()) {
				iter = taddr_map.erase(iter);
			} else {
				iter++;
			}
		}
	}
	{
		const auto range = challenge_map.equal_range(block->height);
		for(auto iter = range.first; iter != range.second; ++iter) {
			proof_map.erase(iter->second);
		}
		challenge_map.erase(range.first, range.second);
	}
	hash_index[block->hash] = block->height;
	history[block->height] = block->get_header();
	change_log.pop_front();

	if(!is_replay) {
		write_block(block);
	}
	while(history.size() > max_history) {
		history.erase(history.begin());
	}
	fork_tree.erase(block->hash);
	pending_vdfs.erase(pending_vdfs.begin(), pending_vdfs.upper_bound(block->height));
	verified_vdfs.erase(verified_vdfs.begin(), verified_vdfs.upper_bound(block->height));

	publish(block, output_committed_blocks, is_replay ? BLOCKING : 0);
}

void Node::purge_tree()
{
	const auto root = get_root();
	std::unordered_set<hash_t> purged;
	for(auto iter = fork_index.begin(); iter != fork_index.end();)
	{
		const auto& fork = iter->second;
		const auto& block = fork->block;
		if(block->height <= root->height
			|| purged.count(block->prev)
			|| (!is_synced && fork->is_invalid))
		{
			if(fork_tree.erase(block->hash)) {
				purged.insert(block->hash);
			}
			iter = fork_index.erase(iter);
		} else {
			iter++;
		}
	}
}

void Node::apply(std::shared_ptr<const Block> block) noexcept
{
	if(block->prev != state_hash) {
		return;
	}
	const auto log = std::make_shared<change_log_t>();
	log->prev_state = state_hash;

	if(auto tx = std::dynamic_pointer_cast<const Transaction>(block->tx_base)) {
		apply(block, tx, *log);
		log->tx_base = tx->id;
	}
	for(size_t i = 0; i < block->tx_list.size(); ++i) {
		if(auto tx = std::dynamic_pointer_cast<const Transaction>(block->tx_list[i])) {
			apply(block, tx, *log);
		}
	}
	state_hash = block->hash;
	change_log.push_back(log);
}

void Node::apply(std::shared_ptr<const Block> block, std::shared_ptr<const Transaction> tx, change_log_t& log) noexcept
{
	for(size_t i = 0; i < tx->inputs.size(); ++i)
	{
		auto iter = utxo_map.find(tx->inputs[i].prev);
		if(iter != utxo_map.end()) {
			const auto key = txio_key_t::create_ex(tx->id, i);
			const auto& stxo = iter->second;
			log.utxo_removed.emplace(iter->first, stxo_t::create_ex(stxo, key));
			taddr_map[stxo.address].erase(iter->first);
			utxo_map.erase(iter);
		}
	}
	for(size_t i = 0; i < tx->outputs.size(); ++i) {
		apply_output(block, tx, tx->outputs[i], i, log);
	}
	for(size_t i = 0; i < tx->exec_outputs.size(); ++i) {
		apply_output(block, tx, tx->exec_outputs[i], tx->outputs.size() + i, log);
	}
	if(auto contract = tx->deploy) {
		if(light_mode) {
			light_address_set.insert(tx->id);
		}
		contract_map.erase(tx->id);
		log.deployed.emplace(tx->id, contract);
	}
	tx_map[tx->id] = block->height;
	tx_pool.emplace(tx->id, tx);
	log.tx_added.push_back(tx->id);
}

void Node::apply_output(std::shared_ptr<const Block> block, std::shared_ptr<const Transaction> tx,
						const tx_out_t& output, const size_t index, change_log_t& log) noexcept
{
	const auto key = txio_key_t::create_ex(tx->id, index);
	const auto utxo = utxo_t::create_ex(output, block->height);
	utxo_map[key] = utxo;
	taddr_map[utxo.address].insert(key);
	log.utxo_added.emplace(key, utxo);
}

bool Node::revert() noexcept
{
	if(change_log.empty()) {
		return false;
	}
	const auto log = change_log.back();

	for(const auto& entry : log->utxo_added) {
		const auto& utxo = entry.second;
		utxo_map.erase(entry.first);
		taddr_map[utxo.address].erase(entry.first);
	}
	for(const auto& entry : log->utxo_removed) {
		const auto& utxo = entry.second;
		utxo_map.emplace(entry.first, utxo);
		taddr_map[utxo.address].insert(entry.first);
	}
	for(const auto& txid : log->tx_added) {
		tx_map.erase(txid);
	}
	if(const auto& txid = log->tx_base) {
		tx_pool.erase(*txid);
	}
	for(const auto& entry : log->deployed) {
		contract_map.erase(entry.first);
		light_address_set.erase(entry.first);
	}
	change_log.pop_back();
	state_hash = log->prev_state;
	return true;
}

std::shared_ptr<const BlockHeader> Node::get_root() const
{
	if(history.empty()) {
		throw std::logic_error("have no root");
	}
	return (--history.end())->second;
}

std::shared_ptr<const BlockHeader> Node::get_peak() const
{
	return find_header(state_hash);
}

std::shared_ptr<Node::fork_t> Node::find_fork(const hash_t& hash) const
{
	auto iter = fork_tree.find(hash);
	if(iter != fork_tree.end()) {
		return iter->second;
	}
	return nullptr;
}

std::shared_ptr<const Block> Node::find_block(const hash_t& hash) const
{
	if(auto fork = find_fork(hash)) {
		return fork->block;
	}
	return nullptr;
}

std::shared_ptr<const BlockHeader> Node::find_header(const hash_t& hash) const
{
	if(auto block = find_block(hash)) {
		return block;
	}
	auto iter = hash_index.find(hash);
	if(iter != hash_index.end()) {
		auto iter2 = history.find(iter->second);
		if(iter2 != history.end()) {
			return iter2->second;
		}
	}
	return nullptr;
}

std::shared_ptr<Node::fork_t> Node::find_prev_fork(std::shared_ptr<fork_t> fork, const size_t distance) const
{
	for(size_t i = 0; fork && i < distance; ++i) {
		fork = fork->prev.lock();
	}
	return fork;
}

std::shared_ptr<const BlockHeader> Node::find_prev_header(	std::shared_ptr<const BlockHeader> block,
															const size_t distance, bool clamped) const
{
	if(distance > 1 && block && (block->height >= distance || clamped))
	{
		const auto height = block->height > distance ? block->height - distance : 0;
		auto iter = history.find(height);
		if(iter != history.end()) {
			return iter->second;
		}
	}
	for(size_t i = 0; block && i < distance; ++i)
	{
		if(clamped && block->height == 0) {
			break;
		}
		block = find_header(block->prev);
	}
	return block;
}

std::shared_ptr<const BlockHeader> Node::find_diff_header(std::shared_ptr<const BlockHeader> block, uint32_t offset) const
{
	if(offset > params->challenge_interval) {
		throw std::logic_error("offset out of range");
	}
	if(block) {
		uint32_t height = block->height + offset;
		height -= (height % params->challenge_interval);
		if(auto prev = find_prev_header(block, (block->height + params->challenge_interval) - height, true)) {
			return prev;
		}
	}
	return nullptr;
}

hash_t Node::get_challenge(std::shared_ptr<const BlockHeader> block, const hash_t& vdf_challenge, uint32_t offset) const
{
	if(auto diff_block = find_diff_header(block, offset)) {
		return hash_t(diff_block->hash + vdf_challenge);
	}
	return hash_t();
}

bool Node::find_vdf_challenge(std::shared_ptr<const BlockHeader> block, hash_t& vdf_challenge, uint32_t offset) const
{
	if(offset > params->challenge_delay) {
		throw std::logic_error("offset out of range");
	}
	if(auto vdf_block = find_prev_header(block, params->challenge_delay - offset, true)) {
		vdf_challenge = vdf_block->vdf_output[1];
		return true;
	}
	return false;
}

std::shared_ptr<Node::vdf_point_t>
Node::find_vdf_point(	const uint32_t height, const uint64_t vdf_start, const uint64_t vdf_iters,
						const std::array<hash_t, 2>& input, const std::array<hash_t, 2>& output) const
{
	for(auto iter = verified_vdfs.lower_bound(height); iter != verified_vdfs.upper_bound(height); ++iter) {
		const auto& point = iter->second;
		if(vdf_start == point->vdf_start && vdf_iters == point->vdf_iters
			&& input == point->input && output == point->output)
		{
			return point;
		}
	}
	return nullptr;
}

std::shared_ptr<Node::vdf_point_t> Node::find_next_vdf_point(std::shared_ptr<const BlockHeader> block)
{
	if(auto diff_block = find_diff_header(block, 1))
	{
		const auto height = block->height + 1;
		const auto infused = find_prev_header(block, params->finality_delay, true);
		const auto vdf_iters = block->vdf_iters + diff_block->time_diff * params->time_diff_constant;

		for(auto iter = verified_vdfs.lower_bound(height); iter != verified_vdfs.upper_bound(height); ++iter) {
			const auto& point = iter->second;
			if(block->vdf_iters == point->vdf_start && vdf_iters == point->vdf_iters && block->vdf_output == point->input
				&& ((!infused && !point->infused) || (infused && point->infused && infused->hash == *point->infused)))
			{
				return point;
			}
		}
	}
	return nullptr;
}

uint64_t Node::calc_block_reward(std::shared_ptr<const BlockHeader> block) const
{
	if(!block->proof) {
		return 0;
	}
	if(auto diff_block = find_diff_header(block)) {
		return mmx::calc_block_reward(params, diff_block->space_diff);
	}
	return 0;
}

std::shared_ptr<const Block> Node::read_block(bool is_replay, int64_t* file_offset)
{
	auto& in = block_chain->in;
	const auto offset = in.get_input_pos();
	if(file_offset) {
		*file_offset = offset;
	}
	try {
		if(auto header = std::dynamic_pointer_cast<BlockHeader>(vnx::read(in))) {
			if(is_replay) {
				block_index[header->height] = std::make_pair(offset, header->hash);
			}
			auto block = Block::create();
			block->BlockHeader::operator=(*header);
			while(true) {
				if(auto value = vnx::read(in)) {
					if(auto tx = std::dynamic_pointer_cast<TransactionBase>(value)) {
						block->tx_list.push_back(tx);
					}
				} else {
					break;
				}
			}
			return block;
		}
	} catch(const std::exception& ex) {
		log(WARN) << "Failed to read block: " << ex.what();
	}
	block_chain->seek_to(offset);
	return nullptr;
}

void Node::write_block(std::shared_ptr<const Block> block)
{
	auto& out = block_chain->out;
	const auto offset = out.get_output_pos();
	if(auto tx = std::dynamic_pointer_cast<const Transaction>(block->tx_base)) {
		tx_log.insert(block->height, tx->id);
		tx_index.insert(tx->id, std::make_pair(offset, block->height));
	}
	block_index[block->height] = std::make_pair(offset, block->hash);

	auto header = block->get_header();
	if(light_mode) {
		auto copy = vnx::clone(header);
		copy->proof = nullptr;
		header = copy;
	}
	vnx::write(out, header);

	for(const auto& tx : block->tx_list) {
		bool is_full = false;
		const auto offset = out.get_output_pos();
		if(!light_mode || std::dynamic_pointer_cast<const Transaction>(tx)) {
			is_full = true;
			tx_log.insert(block->height, tx->id);
			tx_index.insert(tx->id, std::make_pair(offset, block->height));
		}
		if(!light_mode || is_full) {
			vnx::write(out, tx);
		}
	}
	vnx::write(out, nullptr);
	block_chain->flush();
}


} // mmx
