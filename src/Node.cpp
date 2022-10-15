/*
 * Node.cpp
 *
 *  Created on: Dec 7, 2021
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/Context.hxx>
#include <mmx/Challenge.hxx>
#include <mmx/ProofOfStake.hxx>
#include <mmx/ProofOfSpaceOG.hxx>
#include <mmx/ProofOfSpaceNFT.hxx>
#include <mmx/contract/Token.hxx>
#include <mmx/contract/PubKey.hxx>
#include <mmx/contract/Binary.hxx>
#include <mmx/contract/Executable.hxx>
#include <mmx/contract/VirtualPlot.hxx>
#include <mmx/operation/Mutate.hxx>
#include <mmx/operation/Execute.hxx>
#include <mmx/operation/Deposit.hxx>
#include <mmx/utils.h>
#include <mmx/vm/Engine.h>
#include <mmx/vm_interface.h>
#include <mmx/Router.h>

#include <vnx/vnx.h>

#include <tuple>
#include <atomic>
#include <algorithm>

#ifdef WITH_JEMALLOC
#include <jemalloc/jemalloc.h>
#endif


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
	const auto devices = automy::basic_opencl::get_devices();
	std::vector<std::string> device_names;
	for(const auto id : devices) {
		device_names.push_back(automy::basic_opencl::get_device_name(id));
	}
	vnx::write_config(vnx_name + ".opencl_device_list", device_names);

	if(opencl_device >= 0) {
		if(size_t(opencl_device) < devices.size()) {
			for(int i = 0; i < 2; ++i) {
				opencl_vdf[i] = std::make_shared<OCL_VDF>(opencl_device);
			}
			log(INFO) << "Using OpenCL GPU device [" << opencl_device << "] "
					<< automy::basic_opencl::get_device_name(devices[opencl_device])
					<< " (total of " << devices.size() << " found)";
		}
		else if(devices.size()) {
			log(WARN) <<  "No such OpenCL GPU device: " << opencl_device;
		}
	}
#endif
	vdf_threads = std::make_shared<vnx::ThreadPool>(num_vdf_threads);

	router = std::make_shared<RouterAsyncClient>(router_name);
	timelord = std::make_shared<TimeLordAsyncClient>(timelord_name);
	http = std::make_shared<vnx::addons::HttpInterface<Node>>(this, vnx_name);
	add_async_client(router);
	add_async_client(timelord);
	add_async_client(http);

	vnx::Directory(storage_path).create();
	vnx::Directory(database_path).create();

	const auto time_begin = vnx::get_wall_time_millis();
	{
		db.add(recv_log.open(database_path + "recv_log"));
		db.add(spend_log.open(database_path + "spend_log"));
		db.add(exec_log.open(database_path + "exec_log"));

		db.add(contract_map.open(database_path + "contract_map"));
		db.add(deploy_map.open(database_path + "deploy_map"));
		db.add(vplot_map.open(database_path + "vplot_map"));
		db.add(offer_log.open(database_path + "offer_log"));
		db.add(offer_bid_map.open(database_path + "offer_bid_map"));
		db.add(offer_ask_map.open(database_path + "offer_ask_map"));
		db.add(trade_log.open(database_path + "trade_log"));
		db.add(trade_bid_history.open(database_path + "trade_bid_history"));
		db.add(trade_ask_history.open(database_path + "trade_ask_history"));

		db.add(tx_log.open(database_path + "tx_log"));
		db.add(tx_index.open(database_path + "tx_index"));
		db.add(hash_index.open(database_path + "hash_index"));
		db.add(block_index.open(database_path + "block_index"));
		db.add(balance_table.open(database_path + "balance_table"));
		db.add(farmer_block_map.open(database_path + "farmer_block_map"));
	}
	storage = std::make_shared<vm::StorageDB>(database_path, db);
	{
		const auto height = std::min(db.min_version(), replay_height);
		revert(height);
		if(height) {
			log(INFO) << "Loaded DB at height " << (height - 1) << ", " << balance_map.size()
					<< " balances, took " << (vnx::get_wall_time_millis() - time_begin) / 1e3 << " sec";
		}
	}
	block_chain = std::make_shared<vnx::File>(storage_path + "block_chain.dat");

	if(block_chain->exists())
	{
		block_chain->open("rb+");

		bool is_replay = true;
		uint32_t height = -1;
		std::pair<int64_t, hash_t> entry;
		// set state to last block
		if(block_index.find_last(height, entry))
		{
			is_replay = false;
			block_chain->seek_to(entry.first);
			const auto block = std::dynamic_pointer_cast<const Block>(read_block(*block_chain, true));
			if(!block) {
				throw std::runtime_error("failed to read block " + std::to_string(height));
			}
			if(block->height != height) {
				throw std::runtime_error("expected block height " + std::to_string(height) + " but got " + std::to_string(block->height));
			}
			if(block->hash != state_hash) {
				throw std::runtime_error("expected block hash " + state_hash.to_string());
			}
		}
		if(is_replay) {
			log(INFO) << "Creating DB (this may take a while) ...";
			int64_t block_offset = 0;
			std::list<std::shared_ptr<const Block>> history;
			std::vector<std::pair<hash_t, int64_t>> tx_offsets;
			while(auto header = read_block(*block_chain, true, &block_offset, &tx_offsets))
			{
				if(auto block = std::dynamic_pointer_cast<const Block>(header))
				{
					std::shared_ptr<execution_context_t> result;
					if(block->height) {
						try {
							result = validate(block);
						} catch(std::exception& ex) {
							log(ERROR) << "Block validation at height " << block->height << " failed with: " << ex.what();
							block_chain->seek_to(block_offset);
							break;
						}
					}
					for(const auto& entry : tx_offsets) {
						tx_index.insert(entry.first, std::make_pair(entry.second, block->height));
					}
					block_index.insert(block->height, std::make_pair(block_offset, block->hash));

					apply(block, result, true);

					auto fork = std::make_shared<fork_t>();
					fork->block = block;
					fork->is_vdf_verified = true;
					add_fork(fork);

					history.push_back(block);
					if(history.size() > params->commit_delay || block->height == 0) {
						commit(history.front());
						history.pop_front();
					}
					if(block->height % 1000 == 0) {
						log(INFO) << "Height " << block->height << " ...";
					}
					vnx_process(false);
				}
				if(!vnx::do_run()) {
					log(WARN) << "DB replay interrupted";
					return;
				}
			}
			if(auto peak = get_peak()) {
				log(INFO) << "Replayed height " << peak->height << " from disk, took " << (vnx::get_wall_time_millis() - time_begin) / 1e3 << " sec";
			}
		} else {
			// load history and fork tree
			for(int64_t i = max_history; i >= 0; --i) {
				if(i <= height) {
					const auto index = height - i;
					if(i < params->commit_delay && index > 0) {
						if(auto block = get_block_at(index)) {
							auto fork = std::make_shared<fork_t>();
							fork->block = block;
							fork->is_vdf_verified = true;
							add_fork(fork);
						}
					} else {
						if(auto header = get_header_at(index)) {
							history[header->height] = header;
						}
					}
				}
			}
		}
		const auto offset = block_chain->get_input_pos();
		block_chain->seek_to(offset);
		vnx::write(block_chain->out, nullptr);	// temporary end of block_chain.dat
		block_chain->seek_to(offset);
		block_chain->flush();
	} else {
		block_chain->open("wb");
		block_chain->open("rb+");
	}
	is_synced = !do_sync;

	if(state_hash == hash_t())
	{
		auto genesis = Block::create();
		genesis->nonce = params->port;
		genesis->time_diff = params->initial_time_diff;
		genesis->space_diff = params->initial_space_diff;
		genesis->vdf_output[0] = hash_t(params->vdf_seed);
		genesis->vdf_output[1] = hash_t(params->vdf_seed);
		genesis->finalize();

		if(!genesis->is_valid()) {
			throw std::logic_error("genesis not valid");
		}
		apply(genesis, nullptr);
		commit(genesis);
	}

	subscribe(input_vdfs, max_queue_ms);
	subscribe(input_proof, max_queue_ms);
	subscribe(input_blocks, max_queue_ms);
	subscribe(input_transactions, max_queue_ms);
	subscribe(input_timelord_vdfs, max_queue_ms);
	subscribe(input_harvester_proof, max_queue_ms);

	set_timer_millis(30 * 1000, std::bind(&Node::purge_tx_pool, this));
	set_timer_millis(300 * 1000, std::bind(&Node::print_stats, this));
	set_timer_millis(validate_interval_ms, std::bind(&Node::validate_new, this));

	update_timer = set_timer_millis(update_interval_ms, std::bind(&Node::update, this));
	stuck_timer = set_timer_millis(sync_loss_delay * 1000, std::bind(&Node::on_stuck_timeout, this));

	vnx::Handle<mmx::Router> router = new mmx::Router("Router");
	router->node_server = vnx_name;
	router->storage_path = storage_path;
	router.start();

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
		if(!network || peak->height != network->height || is_synced != network->height) {
			auto info = NetworkInfo::create();
			info->is_synced = is_synced;
			info->height = peak->height;
			info->time_diff = peak->time_diff;
			info->space_diff = peak->space_diff;
			info->block_reward = mmx::calc_block_reward(params, peak->space_diff);
			info->total_space = calc_total_netspace(params, peak->space_diff);
			info->total_supply = get_total_supply(addr_t());
			info->address_count = balance_map.size();
			info->genesis_hash = get_genesis_hash();
			{
				size_t num_blocks = 0;
				for(const auto& fork : get_fork_line()) {
					if(fork->block->proof) {
						info->block_size += fork->block->tx_cost / double(params->max_block_cost);
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
	std::pair<int64_t, hash_t> entry;
	if(block_index.find(height, entry)) {
		vnx::File file(block_chain->get_path());
		file.open("rb");
		file.seek_to(entry.first);
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
	std::pair<int64_t, hash_t> entry;
	if(block_index.find(height, entry)) {
		return entry.second;
	}
	return nullptr;
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
	std::pair<int64_t, uint32_t> entry;
	if(tx_index.find(id, entry)) {
		return entry.second;
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
		info.message = tx->exec_result->message;
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
	// THREAD SAFE (for concurrent reads)
	if(include_pending) {
		auto iter = tx_pool.find(id);
		if(iter != tx_pool.end()) {
			return iter->second.tx;
		}
		for(const auto& entry : pending_transactions) {
			if(const auto& tx = entry.second) {
				if(tx->id == id) {
					return tx;
				}
			}
		}
	}
	std::pair<int64_t, uint32_t> entry;
	if(tx_index.find(id, entry)) {
		vnx::File file(block_chain->get_path());
		file.open("rb");
		file.seek_to(entry.first);
		const auto value = vnx::read(file.in);
		if(auto tx = std::dynamic_pointer_cast<const Transaction>(value)) {
			return tx;
		}
		if(auto header = std::dynamic_pointer_cast<const BlockHeader>(value)) {
			if(auto tx = header->tx_base) {
				if(tx->id == id) {
					return std::dynamic_pointer_cast<const Transaction>(tx);
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

std::vector<tx_entry_t> Node::get_history(const std::vector<addr_t>& addresses, const int32_t& since) const
{
	const uint32_t height = get_height();
	const uint32_t min_height = since >= 0 ? since : std::max<int32_t>(height + since, 0);

	struct entry_t {
		uint32_t height = 0;
		uint128_t recv = 0;
		uint128_t spent = 0;
	};
	std::map<std::tuple<addr_t, hash_t, addr_t, tx_type_e>, entry_t> delta_map;

	for(const auto& address : std::unordered_set<addr_t>(addresses.begin(), addresses.end())) {
		{
			std::vector<txio_entry_t> entries;
			recv_log.find_range(std::make_tuple(address, min_height, 0), std::make_tuple(address, -1, -1), entries);
			for(const auto& entry : entries) {
				auto& delta = delta_map[std::make_tuple(
						entry.address, entry.txid, entry.contract,
						entry.type == tx_type_e::REWARD ? entry.type : tx_type_e())];
				delta.height = entry.height;
				delta.recv += entry.amount;
			}
		}
		{
			std::vector<txio_entry_t> entries;
			spend_log.find_range(std::make_tuple(address, min_height, 0), std::make_tuple(address, -1, -1), entries);
			for(const auto& entry : entries) {
				auto& delta = delta_map[std::make_tuple(
						entry.address, entry.txid, entry.contract,
						entry.type == tx_type_e::TXFEE ? entry.type : tx_type_e())];
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
		return std::make_tuple(L.height, L.txid, L.type, L.contract, L.address) <
			   std::make_tuple(R.height, R.txid, R.type, R.contract, R.address);
	});
	return res;
}

std::shared_ptr<const Contract> Node::get_contract(const addr_t& address) const
{
	{
		std::lock_guard<std::mutex> lock(mutex);
		auto iter = contract_cache.find(address);
		if(iter != contract_cache.end()) {
			return iter->second;
		}
	}
	std::shared_ptr<const Contract> contract;
	contract_map.find(address, contract);

	if(std::dynamic_pointer_cast<const contract::Binary>(contract)) {
		std::lock_guard<std::mutex> lock(mutex);
		contract_cache[address] = contract;
	}
	return contract;
}

std::shared_ptr<const Contract> Node::get_contract_for(const addr_t& address) const
{
	if(auto contract = get_contract(address)) {
		return contract;
	}
	auto pubkey = contract::PubKey::create();
	pubkey->address = address;
	return pubkey;
}

std::shared_ptr<const Contract> Node::get_contract_at(const addr_t& address, const hash_t& block_hash) const
{
	const auto root = get_root();
	const auto fork = find_fork(block_hash);
	const auto block = fork ? fork->block : get_header(block_hash);
	if(!block) {
		throw std::logic_error("no such block");
	}
	std::shared_ptr<const Contract> contract;
	contract_map.find(address, contract, std::min(root->height, block->height));

	if(fork) {
		for(const auto& fork_i : get_fork_line(fork)) {
			const auto& block_i = fork_i->block;
			for(const auto& tx : block_i->get_all_transactions()) {
				if(!tx->did_fail()) {
					if(tx->id == address) {
						contract = tx->deploy;
					}
					for(const auto& op : tx->get_operations()) {
						if(op->address == address) {
							if(auto mutate = std::dynamic_pointer_cast<const operation::Mutate>(op)) {
								if(auto copy = vnx::clone(contract)) {
									copy->vnx_call(vnx::clone(mutate->method));
									contract = copy;
								}
							}
						}
					}
				}
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

std::map<addr_t, std::shared_ptr<const Contract>> Node::get_contracts_by(const std::vector<addr_t>& addresses) const
{
	std::map<addr_t, std::shared_ptr<const Contract>> res;
	for(const auto& address : addresses) {
		std::vector<addr_t> list;
		deploy_map.find(address, list);
		for(const auto& addr : list) {
			if(auto contract = get_contract(addr)) {
				res.emplace(addr, contract);
			}
		}
	}
	return res;
}

uint128 Node::get_balance(const addr_t& address, const addr_t& currency) const
{
	const auto iter = balance_map.find(std::make_pair(address, currency));
	if(iter != balance_map.end()) {
		return iter->second;
	}
	return 0;
}

uint128 Node::get_total_balance(const std::vector<addr_t>& addresses, const addr_t& currency) const
{
	uint128_t total = 0;
	for(const auto& address : std::unordered_set<addr_t>(addresses.begin(), addresses.end())) {
		const auto iter = balance_map.find(std::make_pair(address, currency));
		if(iter != balance_map.end()) {
			total += iter->second;
		}
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
	auto context = Context::create();
	context->height = get_height() + 1;
	auto contract = get_contract(address);
	for(const auto& entry : get_total_balances({address})) {
		auto& tmp = out[entry.first];
		if(contract) {
			if(contract->is_locked(context)) {
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
		const auto begin = balance_map.lower_bound(std::make_pair(address, addr_t()));
		const auto end = balance_map.upper_bound(std::make_pair(address, addr_t::ones()));
		for(auto iter = begin; iter != end; ++iter) {
			totals[iter->first.second] += iter->second;
		}
	}
	return totals;
}

std::map<std::pair<addr_t, addr_t>, uint128> Node::get_all_balances(const std::vector<addr_t>& addresses) const
{
	std::map<std::pair<addr_t, addr_t>, uint128> totals;
	for(const auto& address : std::unordered_set<addr_t>(addresses.begin(), addresses.end())) {
		const auto begin = balance_map.lower_bound(std::make_pair(address, addr_t()));
		const auto end = balance_map.upper_bound(std::make_pair(address, addr_t::ones()));
		for(auto iter = begin; iter != end; ++iter) {
			totals[iter->first] += iter->second;
		}
	}
	return totals;
}

std::vector<exec_entry_t> Node::get_exec_history(const addr_t& address, const int32_t& since) const
{
	const uint32_t height = get_height();
	const uint32_t min_height = since >= 0 ? since : std::max<int32_t>(height + since, 0);

	std::vector<exec_entry_t> entries;
	exec_log.find_range(std::make_tuple(address, min_height, 0), std::make_tuple(address, -1, -1), entries);
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

std::pair<vm::varptr_t, uint64_t> Node::read_storage_field(const addr_t& contract, const std::string& name, const uint32_t& height) const
{
	if(auto exec = std::dynamic_pointer_cast<const contract::Executable>(get_contract(contract))) {
		return read_storage_field(exec->binary, contract, name, height);
	}
	return {};
}

std::pair<vm::varptr_t, uint64_t> Node::read_storage_field(const addr_t& binary, const addr_t& contract, const std::string& name, const uint32_t& height) const
{
	if(auto bin = std::dynamic_pointer_cast<const contract::Binary>(get_contract(binary))) {
		auto iter = bin->fields.find(name);
		if(iter != bin->fields.end()) {
			return std::make_pair(read_storage_var(contract, iter->second, height), iter->second);
		}
	}
	return {};
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
			vm::load(engine, bin);
			for(const auto& entry : storage->find_entries(contract, address, height)) {
				if(auto key = engine->read(entry.first)) {
					out[vm::clone(key)] = entry.second;
				}
			}
		}
	}
	return out;
}

vnx::Variant Node::call_contract(const addr_t& address, const std::string& method, const std::vector<vnx::Variant>& args) const
{
	if(auto exec = std::dynamic_pointer_cast<const contract::Executable>(get_contract(address))) {
		if(auto bin = std::dynamic_pointer_cast<const contract::Binary>(get_contract(exec->binary))) {
			auto func = vm::find_method(bin, method);
			if(!func) {
				throw std::runtime_error("no such method: " + method);
			}
			if(!func->is_const) {
				throw std::runtime_error("method is not const: " + method);
			}
			auto engine = std::make_shared<vm::Engine>(address, storage, true);
			engine->total_gas = params->max_block_cost;
			vm::load(engine, bin);
			engine->write(vm::MEM_EXTERN + vm::EXTERN_HEIGHT, vm::uint_t(get_height()));
			engine->write(vm::MEM_EXTERN + vm::EXTERN_TXID, vm::var_t());
			engine->write(vm::MEM_EXTERN + vm::EXTERN_USER, vm::var_t());
			engine->write(vm::MEM_EXTERN + vm::EXTERN_ADDRESS, vm::uint_t(address.to_uint256()));
			vm::set_balance(engine, get_balances(address));
			vm::set_args(engine, args);
			vm::execute(engine, *func);
			return vm::read(engine, vm::MEM_STACK);
		}
		throw std::runtime_error("no such binary");
	}
	throw std::runtime_error("no such contract");
}

uint128 Node::get_total_supply(const addr_t& currency) const
{
	uint128_t total = 0;
	for(const auto& entry : balance_map) {
		if(entry.first.first != addr_t() && entry.first.second == currency) {
			total += entry.second;
		}
	}
	return total;
}

address_info_t Node::get_address_info(const addr_t& address) const
{
	address_info_t info;
	for(const auto& entry : get_history({address}, 0)) {
		switch(entry.type) {
			case tx_type_e::REWARD:
			case tx_type_e::RECEIVE:
				info.num_receive++;
				info.total_receive[entry.contract] += entry.amount;
				info.last_receive_height = std::max(info.last_receive_height, entry.height);
				break;
			case tx_type_e::SPEND:
			case tx_type_e::TXFEE:
				info.num_spend++;
				info.total_spend[entry.contract] += entry.amount;
				info.last_spend_height = std::max(info.last_spend_height, entry.height);
		}
	}
	return info;
}

std::vector<std::pair<addr_t, std::shared_ptr<const Contract>>> Node::get_virtual_plots_for(const bls_pubkey_t& farmer_key) const
{
	std::vector<addr_t> addrs;
	vplot_map.find(farmer_key, addrs);

	std::vector<std::pair<addr_t, std::shared_ptr<const Contract>>> out;
	for(const auto& addr : addrs) {
		if(auto contract = get_contract(addr)) {
			if(auto plot = std::dynamic_pointer_cast<const contract::VirtualPlot>(contract)) {
				if(plot->farmer_key == farmer_key) {
					out.emplace_back(addr, plot);
				}
			}
		}
	}
	return out;
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
	out.bid_currency = to_addr(data["bid_currency"]);
	out.ask_currency = to_addr(data["ask_currency"]);
	out.bid_amount = to_uint(data["bid_amount"]);
	out.ask_amount = to_uint(data["ask_amount"]);
	out.state = to_string_value(data["state"]);
	if(auto var = data["height_close"]) {
		out.close_height = to_uint(var);	// TODO: use close_txid height
	}
	if(auto var = data["trade_txid"]) {
		out.trade_txid = to_hash(var);
	}
	return out;
}

std::string Node::get_offer_state(const addr_t& address) const
{
	static std::mutex mutex;
	static vnx::optional<uint32_t> state_addr;
	if(!state_addr) {
		std::lock_guard<std::mutex> lock(mutex);
		if(!state_addr) {
			if(const auto binary = get_contract_as<const contract::Binary>(params->offer_binary)) {
				state_addr = binary->find_field("state");
			}
		}
	}
	if(state_addr) {
		return to_string_value(read_storage_var(address, *state_addr));
	}
	return "null";
}

std::vector<offer_data_t> Node::fetch_offers(const std::vector<addr_t>& entries, const bool is_open) const
{
	std::vector<offer_data_t> out;
	for(const auto& address : entries) {
		if(!is_open || get_offer_state(address) == "OPEN") {
			const auto data = get_offer(address);
			if(!data.is_scam()) {
				out.push_back(data);
			}
		}
	}
	return out;
}

std::vector<offer_data_t> Node::fetch_offers_for(	const std::vector<addr_t>& entries,
													const vnx::optional<addr_t>& bid, const vnx::optional<addr_t>& ask,
													const bool is_open, const bool filter) const
{
	std::vector<offer_data_t> out;
	for(const auto& address : entries) {
		if(!is_open || get_offer_state(address) == "OPEN") {
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

std::vector<offer_data_t> Node::get_offers(const uint32_t& since, const vnx::bool_t& is_open) const
{
	std::vector<addr_t> entries;
	offer_log.find_range(std::make_pair(since, 0), std::make_pair(-1, -1), entries);
	return fetch_offers(entries, is_open);
}

std::vector<offer_data_t> Node::get_recent_offers(const int32_t& limit, const vnx::bool_t& is_open) const
{
	std::vector<offer_data_t> result;
	std::pair<uint32_t, uint32_t> offer_log_end(-1, -1);

	while(result.size() < size_t(limit)) {
		std::vector<std::pair<std::pair<uint32_t, uint32_t>, addr_t>> entries;
		if(!offer_log.find_last_range(std::make_pair(0, 0), offer_log_end, entries, std::max<size_t>(limit, 100))) {
			break;
		}
		offer_log_end = entries.back().first;

		std::vector<addr_t> list;
		for(const auto& entry : entries) {
			list.push_back(entry.second);
		}
		const auto tmp = fetch_offers(list, is_open);
		result.insert(result.end(), tmp.begin(), tmp.end());
	}
	result.resize(std::min(result.size(), size_t(limit)));
	return result;
}

std::vector<offer_data_t> Node::get_recent_offers_for(
		const vnx::optional<addr_t>& bid, const vnx::optional<addr_t>& ask, const int32_t& limit, const vnx::bool_t& is_open) const
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
			tmp = fetch_offers_for(list, bid, ask, is_open, true);
		}
		else if(bid) {
			std::vector<addr_t> list;
			for(const auto& entry : bid_list) {
				list.push_back(entry.second);
			}
			tmp = fetch_offers_for(list, bid, ask, is_open, true);
		}
		else if(ask) {
			std::vector<addr_t> list;
			for(const auto& entry : ask_list) {
				list.push_back(entry.second);
			}
			tmp = fetch_offers_for(list, bid, ask, is_open, true);
		}
		else {
			tmp = get_recent_offers(limit, is_open);
		}
		for(const auto& entry : tmp) {
			if(offer_set.insert(entry.address).second) {
				result.push_back(entry);
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

std::vector<offer_data_t> Node::get_trade_history(const int32_t& limit, const uint32_t& since) const
{
	std::vector<addr_t> entries;
	trade_log.find_last_range(std::make_pair(since, 0), std::make_pair(-1, -1), entries, limit);

	return fetch_offers_for(entries, nullptr, nullptr);
}

std::vector<offer_data_t> Node::get_trade_history_for(
		const vnx::optional<addr_t>& bid, const vnx::optional<addr_t>& ask, const int32_t& limit, const uint32_t& since) const
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
			if(trade_bid_history.find_last_range(std::make_tuple(*bid, since, 0), bid_history_end, bid_list, std::max<size_t>(limit, 100))) {
				bid_history_end = bid_list.back().first;
			}
		}
		if(ask) {
			if(trade_ask_history.find_last_range(std::make_tuple(*ask, since, 0), ask_history_end, ask_list, std::max<size_t>(limit, 100))) {
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
			tmp = fetch_offers_for(list, bid, ask);
		}
		else if(bid) {
			std::vector<addr_t> list;
			for(const auto& entry : bid_list) {
				list.push_back(entry.second);
			}
			tmp = fetch_offers_for(list, bid, ask);
		}
		else if(ask) {
			std::vector<addr_t> list;
			for(const auto& entry : ask_list) {
				list.push_back(entry.second);
			}
			tmp = fetch_offers_for(list, bid, ask);
		}
		else {
			tmp = get_trade_history(limit, since);
		}
		for(const auto& entry : tmp) {
			if(offer_set.insert(entry.address).second) {
				result.push_back(entry);
			}
		}
		if(bid_list.empty() && ask_list.empty()) {
			break;
		}
	}
	result.resize(std::min(result.size(), size_t(limit)));

	std::sort(result.begin(), result.end(),
		[](const offer_data_t& L, const offer_data_t& R) -> bool {
			return (L.close_height ? *L.close_height : 0) > (R.close_height ? *R.close_height : 0);
		});
	return result;
}

std::vector<std::shared_ptr<const BlockHeader>> Node::get_farmed_blocks(
		const std::vector<bls_pubkey_t>& farmer_keys, const vnx::bool_t& full_blocks, const uint32_t& since) const
{
	std::vector<uint32_t> entries;
	for(const auto& key : farmer_keys) {
		std::vector<uint32_t> tmp;
		farmer_block_map.find(key, tmp);
		entries.insert(entries.end(), tmp.begin(), tmp.end());
	}
	std::sort(entries.begin(), entries.end());

	std::vector<std::shared_ptr<const BlockHeader>> out;
	for(const auto& height : entries) {
		if(height >= since) {
			out.push_back(get_block_at_ex(height, full_blocks));
		}
	}
	return out;
}

std::map<bls_pubkey_t, uint32_t> Node::get_farmed_block_count(const uint32_t& since) const
{
	std::map<bls_pubkey_t, uint32_t> out;
	farmer_block_map.scan([&out, since](const bls_pubkey_t& key, const uint32_t& height) {
		if(height >= since) {
			out[key]++;
		}
	});
	return out;
}

uint32_t Node::get_farmed_block_count_for(const std::vector<bls_pubkey_t>& farmer_keys, const uint32_t& since) const
{
	size_t total = 0;
	for(const auto& key : farmer_keys) {
		std::vector<uint32_t> tmp;
		farmer_block_map.find(key, tmp);
		for(auto height : tmp) {
			total += (height >= since);
		}
	}
	return total;
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

void Node::add_block(std::shared_ptr<const Block> block)
{
	auto fork = std::make_shared<fork_t>();
	fork->recv_time = vnx::get_wall_time_micros();
	fork->block = block;

	if(block->farmer_sig) {
		pending_forks.push_back(fork);
	} else if(block->is_valid()) {
		add_fork(fork);
	} else {
		log(WARN) << "Got invalid block at height " << block->height;
	}
}

void Node::add_fork(std::shared_ptr<fork_t> fork)
{
	if(!fork->recv_time) {
		fork->recv_time = vnx::get_wall_time_micros();
	}
	if(auto block = fork->block) {
		if(fork_tree.emplace(block->hash, fork).second) {
			fork_index.emplace(block->height, fork);
			add_dummy_blocks(block);
		}
	}
}

void Node::add_transaction(std::shared_ptr<const Transaction> tx, const vnx::bool_t& pre_validate)
{
	if(pre_validate) {
		if(auto res = validate(tx)) {
			if(res->did_fail) {
				throw std::runtime_error("tx failed with: " + (res->message ? *res->message : "?"));
			}
		}
	}
	// Note: tx->is_valid() already checked by Router
	pending_transactions[tx->content_hash] = tx;

	if(!vnx_sample) {
		publish(tx, output_transactions);
	}
}

bool Node::recv_height(const uint32_t& height)
{
	if(auto root = get_root()) {
		if(height < root->height) {
			return false;
		}
	}
	if(auto peak = get_peak()) {
		if(height > peak->height && height - peak->height > 1000) {
			return false;
		}
	}
	return true;
}

void Node::handle(std::shared_ptr<const Block> block)
{
	if(!recv_height(block->height)) {
		return;
	}
	if(purged_blocks.count(block->prev)) {
		purged_blocks.insert(block->hash);
		return;
	}
	add_block(block);
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
	if(!recv_height(proof->height)) {
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
	if(!is_synced || !recv_height(value->request->height)) {
		return;
	}
	pending_proofs.push_back(value);
}

#ifdef WITH_JEMALLOC
static void malloc_stats_callback(void* file, const char* data) {
	fwrite(data, 1, strlen(data), (FILE*)file);
}
#endif

void Node::print_stats()
{
#ifdef WITH_JEMALLOC
	static size_t counter = 0;
	if(counter++ % 15 == 0) {
		const std::string path = storage_path + "node_malloc_info.txt";
		FILE* file = fopen(path.c_str(), "w");
		malloc_stats_print(&malloc_stats_callback, file, 0);
		fclose(file);
	}
#endif
	log(INFO) << balance_map.size() << " addresses, " << fork_tree.size() << " blocks in memory, "
			<< tx_pool.size() << " tx pool, " << tx_pool_fees.size() << " tx pool senders";
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
	purged_blocks.clear();

	timelord->stop_vdf(
		[this]() {
			log(INFO) << "Stopped TimeLord";
		});
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
	if(get_height() + max_sync_ahead < sync_pos) {
		return;
	}
	const size_t max_pending = !sync_retry ? std::max(std::min<int>(max_sync_pending, max_sync_jobs), 2) : 2;

	while(sync_pending.size() < max_pending && (!sync_peak || sync_pos < *sync_peak))
	{
		const auto height = sync_pos++;
		sync_pending.insert(height);
		router->get_blocks_at(height,
				std::bind(&Node::sync_result, this, height, std::placeholders::_1),
				[this, height](const vnx::exception& ex) {
					sync_pos = height;
					sync_pending.erase(height);
					log(WARN) << "get_blocks_at() failed with: " << ex.what();
				});
	}
}

void Node::sync_result(const uint32_t& height, const std::vector<std::shared_ptr<const Block>>& blocks)
{
	sync_pending.erase(height);

	uint64_t total_cost = 0;
	for(auto block : blocks) {
		if(block) {
			add_block(block);
			total_cost += block->tx_cost;
		}
	}
	{
		const auto value = max_sync_jobs * (1 - std::min<double>(total_cost / double(params->max_block_cost), 1));
		max_sync_pending = value * 0.1 + max_sync_pending * 0.9;
	}
	if(!is_synced) {
		if(blocks.empty()) {
			if(!sync_peak || height < *sync_peak) {
				sync_peak = height;
			}
		}
		if(!sync_retry && (height % max_sync_jobs == 0 || sync_pending.empty())) {
			update();
		} else {
			sync_more();
		}
	}
}

void Node::fetch_block(const hash_t& hash)
{
	if(!fetch_pending.insert(hash).second) {
		return;
	}
	router->fetch_block(hash, nullptr,
			std::bind(&Node::fetch_result, this, hash, std::placeholders::_1),
			[this, hash](const vnx::exception& ex) {
				log(WARN) << "Fetching block " << hash << " failed with: " << ex.what();
				fetch_pending.erase(hash);
			});
}

void Node::fetch_result(const hash_t& hash, std::shared_ptr<const Block> block)
{
	if(block) {
		add_block(block);
	}
	fetch_pending.erase(hash);
}

std::shared_ptr<const BlockHeader> Node::fork_to(const hash_t& state)
{
	if(state == state_hash) {
		return nullptr;
	}
	if(auto fork = find_fork(state)) {
		return fork_to(fork);
	}
	const auto root = get_root();
	if(state == root->hash) {
		revert(root->height + 1);
		return nullptr;
	}
	throw std::logic_error("cannot fork to " + state.to_string());
}

std::shared_ptr<const BlockHeader> Node::fork_to(std::shared_ptr<fork_t> fork_head)
{
	const auto root = get_root();
	const auto prev_state = state_hash;
	const auto fork_line = get_fork_line(fork_head);

	bool did_fork = false;
	std::shared_ptr<const BlockHeader> forked_at;

	if(state_hash == root->hash) {
		forked_at = root;
	} else {
		// bring state back in line
		auto peak = find_fork(state_hash);
		while(peak) {
			bool found = false;
			for(const auto& fork : fork_line) {
				if(fork->block->hash == peak->block->hash) {
					found = true;
					forked_at = fork->block;
					break;
				}
			}
			if(found) {
				break;
			}
			did_fork = true;

			// add removed tx back to pool
			for(const auto& tx : peak->block->tx_list) {
				tx_pool_t entry;
				auto copy = vnx::clone(tx);
				copy->exec_result = nullptr;
				copy->content_hash = copy->calc_hash(true);
				entry.tx = copy;
				entry.fee = tx->exec_result->total_fee;
				entry.cost = tx->exec_result->total_cost;
				entry.is_valid = true;
				tx_pool_update(entry, true);
			}
			if(peak->block->prev == root->hash) {
				forked_at = root;
				break;
			}
			peak = peak->prev.lock();
		}
	}
	if(forked_at) {
		if(did_fork) {
			revert(forked_at->height + 1);
		}
	} else {
		throw std::logic_error("cannot fork to block at height " + std::to_string(fork_head->block->height));
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
				fork->context = validate(block);

				if(!fork->is_vdf_verified) {
					if(auto prev = find_prev_header(block)) {
						if(auto infuse = find_prev_header(block, params->infuse_delay + 1)) {
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
				log(WARN) << "Block validation failed for height " << block->height << " with: " << ex.what();
				fork->is_invalid = true;
				fork_to(prev_state);
				throw std::runtime_error("validation failed");
			}
			if(is_synced) {
				if(auto point = fork->vdf_point) {
					publish(point->proof, output_verified_vdfs);
				}
				publish(block, output_verified_blocks);
			}
		}
		apply(block, fork->context);
	}
	return did_fork ? forked_at : nullptr;
}

std::shared_ptr<Node::fork_t> Node::find_best_fork(const uint32_t height) const
{
	uint128_t max_weight = 0;
	std::shared_ptr<fork_t> best_fork;
	const auto root = get_root();
	const auto begin = fork_index.upper_bound(root->height);
	const auto end = fork_index.upper_bound(height);
	for(auto iter = begin; iter != end; ++iter)
	{
		const auto& fork = iter->second;
		const auto& block = fork->block;
		const auto prev = fork->prev.lock();
		if(prev && prev->is_invalid) {
			fork->is_invalid = true;
		}
		if((prev && prev->is_all_proof_verified) || (block->prev == root->hash)) {
			fork->is_all_proof_verified = fork->is_proof_verified;
		}
		if(fork->is_all_proof_verified && !fork->is_invalid) {
			if(!best_fork
				|| block->total_weight > max_weight
				|| (block->total_weight == max_weight && block->hash < best_fork->block->hash))
			{
				best_fork = fork;
				max_weight = block->total_weight;
			}
		}
	}
	return best_fork;
}

std::vector<std::shared_ptr<Node::fork_t>> Node::get_fork_line(std::shared_ptr<fork_t> fork_head) const
{
	const auto root = get_root();
	std::vector<std::shared_ptr<fork_t>> line;
	auto fork = fork_head ? fork_head : find_fork(state_hash);
	while(fork && fork->block->height > root->height) {
		line.push_back(fork);
		if(fork->block->prev == root->hash) {
			std::reverse(line.begin(), line.end());
			return line;
		}
		fork = fork->prev.lock();
	}
	return {};
}

void Node::purge_tree()
{
	const auto root = get_root();
	for(auto iter = fork_index.begin(); iter != fork_index.end();)
	{
		const auto& fork = iter->second;
		const auto& block = fork->block;
		if(block->height <= root->height
			|| purged_blocks.count(block->prev)
			|| (!is_synced && fork->is_invalid))
		{
			if(fork_tree.erase(block->hash)) {
				purged_blocks.insert(block->hash);
			}
			iter = fork_index.erase(iter);
		} else {
			iter++;
		}
	}
	for(auto iter = fork_tree.begin(); iter != fork_tree.end();)
	{
		const auto& fork = iter->second;
		const auto& block = fork->block;
		if(block->height <= root->height) {
			iter = fork_tree.erase(iter);
		} else {
			iter++;
		}
	}
	if(purged_blocks.size() > 100000) {
		purged_blocks.clear();
	}
}

void Node::commit(std::shared_ptr<const Block> block)
{
	if(!history.empty()) {
		const auto root = get_root();
		if(block->prev != root->hash) {
			throw std::logic_error("cannot commit height " + std::to_string(block->height) + " after " + std::to_string(root->height));
		}
	}
	const auto height = block->height;
	history[height] = block->get_header();
	{
		const auto range = challenge_map.equal_range(height);
		for(auto iter = range.first; iter != range.second; ++iter) {
			proof_map.erase(iter->second);
		}
		challenge_map.erase(range.first, range.second);
	}
	while(history.size() > max_history) {
		history.erase(history.begin());
	}
	fork_tree.erase(block->hash);

	if(!balance_log.empty()) {
		balance_log.erase(balance_log.begin(), balance_log.upper_bound(height));
	}
	if(!pending_vdfs.empty()) {
		pending_vdfs.erase(pending_vdfs.begin(), pending_vdfs.upper_bound(height));
	}
	if(!verified_vdfs.empty()) {
		verified_vdfs.erase(verified_vdfs.begin(), verified_vdfs.upper_bound(height));
	}
	if(block->height % 16) {
		purge_tree();
	}

	if(is_synced) {
		std::string ksize = "N/A";
		if(auto proof = std::dynamic_pointer_cast<const ProofOfSpaceOG>(block->proof)) {
			ksize = std::to_string(proof->ksize);
		} else if(auto proof = std::dynamic_pointer_cast<const ProofOfSpaceNFT>(block->proof)) {
			ksize = std::to_string(proof->ksize);
		} else if(auto proof = std::dynamic_pointer_cast<const ProofOfStake>(block->proof)) {
			ksize = "VP";
		}
		Node::log(INFO)
				<< "Committed height " << height << " with: ntx = " << block->tx_list.size()
				<< ", k = " << ksize << ", score = " << (block->proof ? block->proof->score : params->score_threshold)
				<< ", tdiff = " << block->time_diff << ", sdiff = " << block->space_diff;
	}
	publish(block, output_committed_blocks, is_synced ? 0 : BLOCKING);
}

void Node::apply(	std::shared_ptr<const Block> block,
					std::shared_ptr<const execution_context_t> context, bool is_replay)
{
	if(block->prev != state_hash) {
		throw std::logic_error("block->prev != state_hash");
	}
	if(!is_replay) {
		write_block(block);
	}
	uint32_t counter = 0;
	std::vector<hash_t> tx_ids;
	std::unordered_set<hash_t> tx_set;
	balance_cache_t balance_cache(&balance_map);

	for(const auto& tx : block->get_all_transactions()) {
		tx_set.insert(tx->id);
		tx_ids.push_back(tx->id);
		apply(block, context, tx, balance_cache, counter);
	}
	tx_log.insert(block->height, tx_ids);

	for(auto iter = pending_transactions.begin(); iter != pending_transactions.end();) {
		if(tx_set.count(iter->second->id)) {
			iter = pending_transactions.erase(iter);
		} else {
			iter++;
		}
	}
	auto& balance_delta = balance_log[block->height];
	for(const auto& entry : balance_cache.balance) {
		const auto& key = entry.first;
		const auto& new_balance = entry.second;
		const auto iter = balance_map.find(key);
		if(iter != balance_map.end()) {
			if(new_balance > iter->second) {
				balance_delta.added[key] = new_balance - iter->second;
			}
			if(new_balance < iter->second) {
				balance_delta.removed[key] = iter->second - new_balance;
			}
			if(new_balance) {
				iter->second = new_balance;
			} else {
				balance_map.erase(iter);
			}
		} else if(new_balance) {
			balance_map[key] = new_balance;
			balance_delta.added[key] = new_balance;
		}
		balance_table.insert(key, new_balance);
	}
	if(context) {
		for(const auto& entry : context->contract_cache.state_map) {
			const auto& state = entry.second;
			if(state->is_mutated) {
				contract_map.insert(entry.first, state->data);
			}
		}
		context->storage->commit();
	}
	if(auto proof = block->proof) {
		farmer_block_map.insert(proof->farmer_key, block->height);
	}
	hash_index.insert(block->hash, block->height);

	state_hash = block->hash;
	contract_cache.clear();

	db.commit(block->height + 1);
}

void Node::apply(	std::shared_ptr<const Block> block,
					std::shared_ptr<const execution_context_t> context,
					std::shared_ptr<const Transaction> tx,
					balance_cache_t& balance_cache, uint32_t& counter)
{
	if(!tx->exec_result) {
		throw std::logic_error("apply(): exec_result missing");
	}
	if(tx->sender)
	{
		txin_t in;
		in.address = *tx->sender;
		in.amount = tx->exec_result->total_fee;

		spend_log.insert(std::make_tuple(in.address, block->height, counter++),
				txio_entry_t::create_ex(tx->id, block->height, tx_type_e::TXFEE, in));

		if(auto balance = balance_cache.find(*tx->sender, addr_t())) {
			clamped_sub_assign(*balance, in.amount);
		}
	}
	if(!tx->exec_result->did_fail)
	{
		const auto outputs = tx->get_outputs();
		for(size_t i = 0; i < outputs.size(); ++i)
		{
			const auto& out = outputs[i];
			recv_log.insert(std::make_tuple(out.address, block->height, counter++),
					txio_entry_t::create_ex(tx->id, block->height, tx->sender ? tx_type_e::RECEIVE : tx_type_e::REWARD, out));

			balance_cache.get(out.address, out.contract) += out.amount;
		}
		const auto inputs = tx->get_inputs();
		for(size_t i = 0; i < inputs.size(); ++i)
		{
			const auto& in = inputs[i];
			spend_log.insert(std::make_tuple(in.address, block->height, counter++),
					txio_entry_t::create_ex(tx->id, block->height, tx_type_e::SPEND, in));

			if(auto balance = balance_cache.find(in.address, in.contract)) {
				clamped_sub_assign(*balance, in.amount);
			}
		}
		if(auto contract = tx->deploy)
		{
			if(tx->sender) {
				deploy_map.insert(*tx->sender, tx->id);
			}
			if(auto exec = std::dynamic_pointer_cast<const contract::Executable>(contract)) {
				if(exec->binary == params->offer_binary) {
					const auto ticket = counter++;
					offer_log.insert(std::make_pair(block->height, ticket), tx->id);
					if(exec->init_args.size() > 1) {
						offer_ask_map.insert(std::make_tuple(exec->init_args[1].to<addr_t>(), block->height, ticket), tx->id);
					}
				}
			}
			if(auto plot = std::dynamic_pointer_cast<const contract::VirtualPlot>(contract)) {
				vplot_map.insert(plot->farmer_key, tx->id);
			}
			contract_map.insert(tx->id, contract);
		}
		for(const auto& op : tx->execute)
		{
			const auto address = op->address == addr_t() ? addr_t(tx->id) : op->address;
			const auto contract = op->address == addr_t() ? tx->deploy :
					(context ? context->contract_cache.find_contract(op->address) : nullptr);

			if(auto exec = std::dynamic_pointer_cast<const operation::Execute>(op)) {
				exec_entry_t entry;
				entry.height = block->height;
				entry.txid = tx->id;
				entry.method = exec->method;
				entry.args = exec->args;
				auto deposit = std::dynamic_pointer_cast<const operation::Deposit>(exec);
				if(deposit) {
					entry.deposit = std::make_pair(deposit->currency, deposit->amount);
				}
				const auto ticket = counter++;
				exec_log.insert(std::make_tuple(address, block->height, ticket), entry);

				if(auto executable = std::dynamic_pointer_cast<const contract::Executable>(contract)) {
					if(executable->binary == params->offer_binary) {
						if(exec->method == "open") {
							if(deposit) {
								offer_bid_map.insert(std::make_tuple(deposit->currency, block->height, ticket), address);
							}
						} else if(exec->method == "trade") {
							trade_log.insert(std::make_pair(block->height, ticket), address);
							if(deposit) {
								trade_ask_history.insert(std::make_tuple(deposit->currency, block->height, ticket), address);
							}
							const auto res = read_storage_field(params->offer_binary, address, "bid_currency");
							if(const auto& currency = res.first) {
								trade_bid_history.insert(std::make_tuple(to_addr(currency), block->height, ticket), address);
							}
						}
					}
				}
			}
			else if(auto mutate = std::dynamic_pointer_cast<const operation::Mutate>(op)) {
				const auto method = mutate->method["__type"].to_string_value();
				if(method == "mmx.contract.VirtualPlot.bls_transfer") {
					vplot_map.insert(mutate->method["new_farmer_key"].to<bls_pubkey_t>(), address);
				}
			}
		}
	}
	tx_pool_erase(tx->id);
}

void Node::revert(const uint32_t height)
{
	const auto time_begin = vnx::get_wall_time_millis();
	if(block_chain) {
		std::pair<int64_t, hash_t> entry;
		if(block_index.find(height, entry)) {
			block_chain->seek_to(entry.first);
		}
	}
	db.revert(height);
	contract_cache.clear();

	if(!balance_log.empty() && balance_log.begin()->first <= height) {
		for(auto iter = balance_log.rbegin(); iter != balance_log.rend() && iter->first >= height; ++iter) {
			for(const auto& entry : iter->second.removed) {
				balance_map[entry.first] += entry.second;
			}
			for(const auto& entry : iter->second.added) {
				if(!(balance_map[entry.first] -= entry.second)) {
					balance_map.erase(entry.first);
				}
			}
		}
		balance_log.erase(balance_log.lower_bound(height), balance_log.end());
	} else {
		balance_map.clear();
		balance_log.clear();
		balance_table.scan(
			[this](const std::pair<addr_t, addr_t>& key, const uint128& value) {
				if(value) {
					balance_map[key] = value;
				}
			});
	}

	// TODO: remove
	if(false) {
		bool fail = false;
		balance_table.scan(
			[this, &fail](const std::pair<addr_t, addr_t>& key, const uint128& value) {
				auto iter = balance_map.find(key);
				if(iter != balance_map.end()) {
					if(iter->second != value) {
						log(ERROR) << "balance_map mismatch: " << iter->second.str(10) << " != " << value.str(10) << " for " << vnx::to_string(key);
						fail = true;
					}
				} else if(value) {
					log(ERROR) << "Missing balance_map entry for " << vnx::to_string(key);
					fail = true;
				}
			});
		if(fail) {
			throw std::logic_error("balance_map error");
		}
	}

	std::pair<int64_t, hash_t> entry;
	if(block_index.find_last(entry)) {
		state_hash = entry.second;
	} else {
		state_hash = hash_t();
	}
	const auto elapsed = (vnx::get_wall_time_millis() - time_begin) / 1e3;
	if(elapsed > 1) {
		log(WARN) << "Reverting to height " << int64_t(height) - 1 << " took " << elapsed << " sec";
	}
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
	return get_header(state_hash);
}

std::shared_ptr<Node::fork_t> Node::find_fork(const hash_t& hash) const
{
	auto iter = fork_tree.find(hash);
	if(iter != fork_tree.end()) {
		return iter->second;
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
	for(size_t i = 0; block && i < distance && (block->height || !clamped); ++i) {
		block = get_header(block->prev);
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

std::shared_ptr<const BlockHeader> Node::get_diff_header(std::shared_ptr<const BlockHeader> block, uint32_t offset) const
{
	if(auto header = find_diff_header(block, offset)) {
		return header;
	}
	throw std::logic_error("cannot find diff header");
}

hash_t Node::get_challenge(std::shared_ptr<const BlockHeader> block, const hash_t& vdf_challenge, uint32_t offset) const
{
	return hash_t(get_diff_header(block, offset)->hash + vdf_challenge);
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

std::shared_ptr<Node::vdf_point_t> Node::find_next_vdf_point(std::shared_ptr<const BlockHeader> block) const
{
	if(auto diff_block = find_diff_header(block, 1))
	{
		const auto height = block->height + 1;
		const auto infused = find_prev_header(block, params->infuse_delay);
		const auto vdf_iters = block->vdf_iters + diff_block->time_diff * params->time_diff_constant;

		for(auto iter = verified_vdfs.lower_bound(height); iter != verified_vdfs.upper_bound(height); ++iter)
		{
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

std::vector<std::pair<hash_t, Node::proof_data_t>> Node::find_proof(const hash_t& challenge) const
{
	std::vector<std::pair<hash_t, Node::proof_data_t>> res;
	const auto iter = proof_map.find(challenge);
	if(iter != proof_map.end()) {
		for(const auto& entry : iter->second) {
			res.push_back(entry);
		}
	}
	return res;
}

uint64_t Node::calc_block_reward(std::shared_ptr<const BlockHeader> block) const
{
	if(!block->proof || std::dynamic_pointer_cast<const ProofOfStake>(block->proof)) {
		return 0;
	}
	const auto reward = mmx::calc_block_reward(params, get_diff_header(block)->space_diff);
	return std::max(reward, params->min_reward);
}

std::shared_ptr<const BlockHeader> Node::read_block(
		vnx::File& file, bool full_block, int64_t* block_offset, std::vector<std::pair<hash_t, int64_t>>* tx_offsets) const
{
	// THREAD SAFE (for concurrent reads)
	auto& in = file.in;
	const auto offset = in.get_input_pos();
	if(block_offset) {
		*block_offset = offset;
	}
	if(tx_offsets) {
		tx_offsets->clear();
	}
	try {
		if(auto header = std::dynamic_pointer_cast<BlockHeader>(vnx::read(in))) {
			if(tx_offsets) {
				if(auto tx = header->tx_base) {
					tx_offsets->emplace_back(tx->id, offset);
				}
			}
			if(full_block) {
				auto block = Block::create();
				block->BlockHeader::operator=(*header);
				while(true) {
					const auto offset = in.get_input_pos();
					if(auto value = vnx::read(in)) {
						if(auto tx = std::dynamic_pointer_cast<const Transaction>(value)) {
							if(tx_offsets) {
								tx_offsets->emplace_back(tx->id, offset);
							}
							block->tx_list.push_back(tx);
						} else {
							throw std::logic_error("expected transaction");
						}
					} else {
						break;
					}
				}
				header = block;
			}
			return header;
		}
	} catch(const std::exception& ex) {
		log(WARN) << "Failed to read block: " << ex.what();
	}
	file.seek_to(offset);
	return nullptr;
}

void Node::write_block(std::shared_ptr<const Block> block)
{
	auto& out = block_chain->out;
	const auto offset = out.get_output_pos();

	std::vector<std::pair<hash_t, int64_t>> tx_list;
	if(auto tx = block->tx_base) {
		tx_list.emplace_back(tx->id, offset);
	}
	vnx::write(out, block->get_header());

	for(const auto& tx : block->tx_list) {
		tx_list.emplace_back(tx->id, out.get_output_pos());
		vnx::write(out, tx);
	}
	for(const auto& entry : tx_list) {
		tx_index.insert(entry.first, std::make_pair(entry.second, block->height));
	}
	vnx::write(out, nullptr);	// end of block
	const auto end = out.get_output_pos();
	vnx::write(out, nullptr);	// temporary end of block_chain.dat
	block_chain->flush();
	block_chain->seek_to(end);

	block_index.insert(block->height, std::make_pair(offset, block->hash));
}


} // mmx
