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
	if(max_blocks_per_height < 1) {
		throw std::logic_error("max_blocks_per_height < 1");
	}

#ifdef WITH_OPENCL
	cl_context opencl_context = nullptr;
	try {
		std::string platform_name;
		vnx::read_config("opencl.platform", platform_name);

		const auto platforms = automy::basic_opencl::get_platforms();
		{
			std::vector<std::string> list;
			for(const auto id : platforms) {
				list.push_back(automy::basic_opencl::get_platform_name(id));
			}
			vnx::write_config("opencl.platform_list", list);
		}

		cl_platform_id platform = nullptr;
		if(platform_name.empty()) {
			if(!platforms.empty()) {
				platform = platforms[0];
			}
		} else {
			platform = automy::basic_opencl::find_platform_by_name(platform_name);
		}

		if(platform) {
			const auto devices = automy::basic_opencl::get_devices(platform, CL_DEVICE_TYPE_GPU);
			{
				std::vector<std::string> list;
				for(const auto id : devices) {
					list.push_back(automy::basic_opencl::get_device_name(id));
				}
				vnx::write_config("opencl.device_list", list);
			}

			if(opencl_device >= 0) {
				log(INFO) << "Using OpenCL platform: " << automy::basic_opencl::get_platform_name(platform);

				if(size_t(opencl_device) < devices.size()) {
					const auto device = devices[opencl_device];
					opencl_context = automy::basic_opencl::create_context(platform, {device});
					for(int i = 0; i < 2; ++i) {
						opencl_vdf[i] = std::make_shared<OCL_VDF>(opencl_context, device);
					}
					log(INFO) << "Using OpenCL GPU device [" << opencl_device << "] "
							<< automy::basic_opencl::get_device_name(device)
							<< " (total of " << devices.size() << " found)";
				}
				else if(devices.size()) {
					log(WARN) <<  "No such OpenCL GPU device: " << opencl_device;
				}
			}
		} else {
			log(INFO) << "No OpenCL platform found.";
		}
	}
	catch(const std::exception& ex) {
		log(WARN) << "Failed to create OpenCL GPU context: " << ex.what();
	}
#endif

	threads = std::make_shared<vnx::ThreadPool>(num_threads);
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
		db.add(contract_type_map.open(database_path + "contract_type_map"));
		db.add(deploy_map.open(database_path + "deploy_map"));
		db.add(vplot_map.open(database_path + "vplot_map"));
		db.add(owner_map.open(database_path + "owner_map"));
		db.add(offer_log.open(database_path + "offer_log"));
		db.add(offer_bid_map.open(database_path + "offer_bid_map"));
		db.add(offer_ask_map.open(database_path + "offer_ask_map"));
		db.add(trade_log.open(database_path + "trade_log"));
		db.add(swap_log.open(database_path + "swap_log"));
		db.add(swap_liquid_map.open(database_path + "swap_liquid_map"));

		db.add(tx_log.open(database_path + "tx_log"));
		db.add(tx_index.open(database_path + "tx_index"));
		db.add(hash_index.open(database_path + "hash_index"));
		db.add(block_index.open(database_path + "block_index"));
		db.add(balance_table.open(database_path + "balance_table"));
		db.add(farmer_block_map.open(database_path + "farmer_block_map"));
	}
	storage = std::make_shared<vm::StorageDB>(database_path, db);

	if(db_replay) {
		replay_height = 0;
	}
	{
		const auto height = std::min(db.min_version(), replay_height);
		revert(height);
		if(height) {
			log(INFO) << "Loaded DB at height " << (height - 1) << ", " << balance_map.size()
					<< " balances, took " << (vnx::get_wall_time_millis() - time_begin) / 1e3 << " sec";
		}
	}
	block_chain = std::make_shared<vnx::File>(storage_path + "block_chain.dat");

	if(block_chain->exists() && (replay_height || db_replay))
	{
		block_chain->open("rb+");

		bool is_replay = true;
		uint32_t height = -1;
		std::pair<int64_t, std::pair<hash_t, hash_t>> entry;
		// set block_chain position past last readable and valid block
		while(block_index.find_last(height, entry)) {
			try {
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
				is_replay = false;
				break;
			}
			catch(const std::exception& ex) {
				log(WARN) << ex.what();
			}
			db.revert(height);
		}
		if(is_replay) {
			log(INFO) << "Creating DB (this may take a while) ...";
			int64_t block_offset = 0;
			std::list<std::shared_ptr<const Block>> history;
			std::vector<std::pair<hash_t, int64_t>> tx_offsets;

			block_chain->seek_begin();
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
					block_index.insert(block->height, std::make_pair(block_offset, std::make_pair(block->hash, block->content_hash)));

					if(block->height) {
						auto fork = std::make_shared<fork_t>();
						fork->block = block;
						fork->is_vdf_verified = true;
						add_fork(fork);
					}
					apply(block, result, true);

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
		auto block = Block::create();
		block->nonce = params->port;
		block->time_diff = params->initial_time_diff;
		block->space_diff = params->initial_space_diff;
		block->vdf_output[0] = hash_t(params->vdf_seed);
		block->vdf_output[1] = hash_t(params->vdf_seed);
		block->tx_list.push_back(vnx::read_from_file<Transaction>("data/tx_plot_binary.dat"));
		block->tx_list.push_back(vnx::read_from_file<Transaction>("data/tx_offer_binary.dat"));
		block->tx_list.push_back(vnx::read_from_file<Transaction>("data/tx_swap_binary.dat"));

		for(auto tx : block->tx_list) {
			if(!tx) {
				throw std::logic_error("failed to load genesis transactions");
			}
		}
		block->finalize();

		if(!block->is_valid()) {
			throw std::logic_error("genesis not valid");
		}
		apply(block, nullptr);
		commit(block);
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

#ifdef WITH_OPENCL
	automy::basic_opencl::release_context(opencl_context);
#endif
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
	std::pair<int64_t, std::pair<hash_t, hash_t>> entry;
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
	if(auto hash = get_block_hash_ex(height))  {
		return hash->first;
	}
	return nullptr;
}

vnx::optional<std::pair<hash_t, hash_t>> Node::get_block_hash_ex(const uint32_t& height) const
{
	std::pair<int64_t, std::pair<hash_t, hash_t>> entry;
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
				tx_type_e type;
				switch(entry.type) {
					case tx_type_e::REWARD:
					case tx_type_e::VDF_REWARD:
					case tx_type_e::PROJECT_REWARD:
						type = entry.type; break;
					default: break;
				}
				auto& delta = delta_map[std::make_tuple(entry.address, entry.txid, entry.contract, type)];
				delta.height = entry.height;
				delta.recv += entry.amount;
			}
		}
		{
			std::vector<txio_entry_t> entries;
			spend_log.find_range(std::make_tuple(address, min_height, 0), std::make_tuple(address, -1, -1), entries);
			for(const auto& entry : entries) {
				tx_type_e type;
				switch(entry.type) {
					case tx_type_e::TXFEE:
						type = entry.type; break;
					default: break;
				}
				auto& delta = delta_map[std::make_tuple(entry.address, entry.txid, entry.contract, type)];
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
		if(contract_cache.size() >> 16) {
			contract_cache.clear();
		}
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
			for(const auto& tx : block_i->get_transactions()) {
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

std::vector<addr_t> Node::get_contracts_by(const std::vector<addr_t>& addresses) const
{
	std::vector<addr_t> result;
	for(const auto& address : addresses) {
		std::vector<addr_t> list;
		deploy_map.find_range(std::make_tuple(address, 0, 0), std::make_tuple(address, -1, -1), list);
		result.insert(result.end(), list.begin(), list.end());
	}
	return result;
}

std::vector<addr_t> Node::get_contracts_owned_by(const std::vector<addr_t>& addresses) const
{
	std::vector<addr_t> result;
	for(const auto& address : addresses) {
		std::vector<addr_t> list;
		owner_map.find_range(std::make_tuple(address, 0, 0), std::make_tuple(address, -1, -1), list);
		result.insert(result.end(), list.begin(), list.end());
	}
	return result;
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
			engine->total_gas = params->max_block_cost;
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
			engine->total_gas = params->max_block_cost;
			vm::load(engine, bin);
			engine->write(vm::MEM_EXTERN + vm::EXTERN_TXID, vm::var_t());
			engine->write(vm::MEM_EXTERN + vm::EXTERN_HEIGHT, vm::uint_t(get_height()));
			engine->write(vm::MEM_EXTERN + vm::EXTERN_ADDRESS, vm::uint_t(address.to_uint256()));
			if(user) {
				engine->write(vm::MEM_EXTERN + vm::EXTERN_USER, vm::uint_t(user->to_uint256()));
			} else {
				engine->write(vm::MEM_EXTERN + vm::EXTERN_USER, vm::var_t());
			}
			if(deposit) {
				vm::set_deposit(engine, deposit->first, deposit->second);
			}
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
	for(const auto& entry : get_balances(address)) {
		if(entry.second) {
			info.num_active++;
		}
	}
	for(const auto& entry : get_history({address}, 0)) {
		switch(entry.type) {
			case tx_type_e::REWARD:
			case tx_type_e::VDF_REWARD:
			case tx_type_e::PROJECT_REWARD:
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

std::vector<address_info_t> Node::get_address_infos(const std::vector<addr_t>& addresses, const int32_t& since) const
{
	std::unordered_map<addr_t, size_t> index_map;
	std::vector<address_info_t> result(addresses.size());
	for(size_t i = 0; i < addresses.size(); ++i) {
		auto& info = result[i];
		info.address = addresses[i];
		for(const auto& entry : get_balances(info.address)) {
			if(entry.second) {
				info.num_active++;
			}
		}
		index_map[addresses[i]] = i;
	}
	for(const auto& entry : get_history(addresses, since)) {
		auto& info = result[index_map[entry.address]];
		switch(entry.type) {
			case tx_type_e::REWARD:
			case tx_type_e::VDF_REWARD:
			case tx_type_e::PROJECT_REWARD:
			case tx_type_e::RECEIVE:
				info.num_receive++;
				info.total_receive[entry.contract] += entry.amount;
				info.last_receive_height = std::max(info.last_receive_height, entry.height);
				break;
			case tx_type_e::SPEND:
				info.num_spend++;
				/* no break */
			case tx_type_e::TXFEE:
				info.total_spend[entry.contract] += entry.amount;
				info.last_spend_height = std::max(info.last_spend_height, entry.height);
				break;
		}
	}
	return result;
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
			info.size_bytes = calc_virtual_plot_size(params, info.balance);
			info.owner = to_addr(storage->read(address, vm::MEM_STATIC + 1));
			result.push_back(info);
		}
	}
	return result;
}

std::vector<virtual_plot_info_t> Node::get_virtual_plots_for(const bls_pubkey_t& farmer_key) const
{
	std::vector<addr_t> addresses;
	vplot_map.find(farmer_key, addresses);
	return get_virtual_plots(addresses);
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
	if(get_balance(address, to_addr(read_storage_var(address, vm::MEM_STATIC + 3)))) {
		return 1;
	}
	if(get_balance(address, to_addr(read_storage_var(address, vm::MEM_STATIC + 4)))) {
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
													const vnx::bool_t& state, const bool filter) const
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
	offer_log.find_range(std::make_pair(since, 0), std::make_pair(-1, -1), entries);
	return fetch_offers(entries, state);
}

std::vector<offer_data_t> Node::get_offers_by(const std::vector<addr_t>& owners, const vnx::bool_t& state) const
{
	std::vector<addr_t> list;
	for(const auto& address : get_contracts_owned_by(owners)) {
		hash_t type;
		if(contract_type_map.find(address, type) && type == params->offer_binary) {
			list.push_back(address);
		}
	}
	return fetch_offers(list, state, true);
}

std::vector<offer_data_t> Node::get_recent_offers(const int32_t& limit, const vnx::bool_t& state) const
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
		const auto tmp = fetch_offers(list, state);
		result.insert(result.end(), tmp.begin(), tmp.end());
	}
	result.resize(std::min(result.size(), size_t(limit)));
	return result;
}

std::vector<offer_data_t> Node::get_recent_offers_for(
		const vnx::optional<addr_t>& bid, const vnx::optional<addr_t>& ask, const int32_t& limit, const vnx::bool_t& state) const
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
	swap_log.find_range(std::make_pair(since, 0), std::make_pair(-1, -1), entries);

	std::vector<swap_info_t> result;
	for(const auto& address : entries) {
		const auto info = get_swap_info(address);
		if((!token || info.tokens[0] == *token) && (!currency || info.tokens[1] == *currency)) {
			if((info.tokens[0] == addr_t() || contract_type_map.count(info.tokens[0]))
				&& (info.tokens[1] == addr_t() || contract_type_map.count(info.tokens[1])))
			{
				result.push_back(info);
			}
		}
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
	const auto tokens = read_storage_array(address, to_ref(data["tokens"]));
	const auto volume = read_storage_array(address, to_ref(data["volume"]));
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
	const auto key = storage->lookup(address, vm::uint_t(user.to_uint256()));
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
		} else if(entry.method == "payout") {
			out.type = "PAYOUT";
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

std::array<uint128, 2> Node::get_swap_trade_estimate(const addr_t& address, const uint32_t& i, const uint64_t& amount) const
{
	const auto info = get_swap_info(address);

	std::vector<vnx::Variant> args;
	args.emplace_back(i);
	args.emplace_back(address.to_string());
	args.emplace_back(nullptr);
	const auto ret = call_contract(address, "trade", args, nullptr, std::make_pair(info.tokens[i], amount));

	uint128 fee_amount = 0;
	uint128 trade_amount = 0;
	for(const auto& entry : ret.to<std::map<uint32_t, vnx::Object>>()) {
		fee_amount += entry.second["fee_amount"].to<uint128>();
		trade_amount += entry.second["trade_amount"].to<uint128>();
	}
	trade_amount -= fee_amount;

	if(trade_amount.upper()) {
		throw std::logic_error("amount overflow");
	}
	return {trade_amount, fee_amount};
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

void Node::trigger_update()
{
	if(!update_pending) {
		update_pending = true;
		add_task(std::bind(&Node::update, this));
	}
}

void Node::add_block(std::shared_ptr<const Block> block)
{
	auto fork = std::make_shared<fork_t>();
	fork->recv_time = vnx::get_wall_time_micros();
	fork->block = block;

	if(block->farmer_sig) {
		// need to verify farmer_sig first
		pending_forks.push_back(fork);
		if(is_synced) {
			trigger_update();
		}
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
		// compute balance deltas
		fork->balance = balance_log_t();
		for(const auto& out : block->get_outputs(params)) {
			fork->balance.added[std::make_pair(out.address, out.contract)] += out.amount;
		}
		for(const auto& in : block->get_inputs(params)) {
			fork->balance.removed[std::make_pair(in.address, in.contract)] += in.amount;
		}
		if(fork_tree.emplace(block->hash, fork).second) {
			fork_index.emplace(block->height, fork);
			add_dummy_block(block);
		}
	}
}

void Node::add_transaction(std::shared_ptr<const Transaction> tx, const vnx::bool_t& pre_validate)
{
	if(pre_validate) {
		const auto res = validate(tx);
		if(res.did_fail) {
			throw std::runtime_error(res.get_error_msg());
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
	if(!is_synced || !value->request || !recv_height(value->request->height)) {
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
	if(counter++ % 3 == 0) {
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

void Node::revert_sync(const uint32_t& height)
{
	add_task([this, height]() {
		do_restart = true;
		log(WARN) << "Reverting to height " << height << " ...";
		vnx::write_config(vnx_name + ".replay_height", height);
		vnx_stop();
	});
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
	if(vdf_threads->get_num_running() && fork_tree.size() > 10 * max_sync_ahead) {
		return;	// limit blocks in memory during sync
	}
	if(get_height() + max_sync_ahead < sync_pos) {
		return;
	}
	const size_t max_pending = !sync_retry ? std::max(std::min<int>(max_sync_pending, max_sync_jobs), 4) : 2;

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

	uint64_t total_size = 0;
	for(auto block : blocks) {
		if(block) {
			add_block(block);
			total_size += block->static_cost;
		}
	}
	{
		const auto value = max_sync_jobs * (1 - std::min<double>(total_size / double(params->max_block_size), 1));
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

std::shared_ptr<Node::fork_t> Node::find_best_fork(const uint32_t max_height) const
{
	const auto root = get_root();
	if(max_height <= root->height) {
		return nullptr;
	}
	uint128_t max_weight = 0;
	std::shared_ptr<fork_t> best_fork;
	const auto begin = fork_index.upper_bound(root->height);
	const auto end = fork_index.upper_bound(max_height);
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
	if(!fork) {
		return {};
	}
	while(fork && fork->block->height > root->height) {
		line.push_back(fork);
		if(fork->block->prev == root->hash) {
			std::reverse(line.begin(), line.end());
			return line;
		}
		fork = fork->prev.lock();
	}
	throw std::logic_error("disconnected fork");
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
	if(purged_blocks.size() > 10000) {
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

	for(const auto& out : block->get_outputs(params))
	{
		if(out.type != tx_type_e::PROJECT_REWARD) {
			recv_log.insert(std::make_tuple(out.address, block->height, counter++), out);
		}
		balance_cache.get(out.address, out.contract) += out.amount;
	}
	for(const auto& in : block->get_inputs(params))
	{
		if(auto balance = balance_cache.find(in.address, in.contract)) {
			clamped_sub_assign(*balance, in.amount);
		}
		spend_log.insert(std::make_tuple(in.address, block->height, counter++), in);
	}
	for(const auto& tx : block->get_transactions()) {
		if(tx) {
			tx_set.insert(tx->id);
			tx_ids.push_back(tx->id);
			apply(block, context, tx, counter);
		}
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
					uint32_t& counter)
{
	if(!tx->exec_result || !tx->exec_result->did_fail)
	{
		if(auto contract = tx->deploy)
		{
			const auto ticket = counter++;
			auto type_hash = hash_t(contract->get_type_name());
			if(tx->sender) {
				deploy_map.insert(std::make_tuple(*tx->sender, block->height, ticket), tx->id);
			}
			if(auto owner = contract->get_owner()) {
				owner_map.insert(std::make_tuple(*owner, block->height, ticket), tx->id);
			}
			if(auto exec = std::dynamic_pointer_cast<const contract::Executable>(contract)) {
				if(exec->binary == params->swap_binary) {
					swap_log.insert(std::make_pair(block->height, ticket), tx->id);
				}
				if(exec->binary == params->offer_binary) {
					if(exec->init_args.size() >= 3) {
						owner_map.insert(std::make_tuple(exec->init_args[0].to<addr_t>(), block->height, ticket), tx->id);
						offer_bid_map.insert(std::make_tuple(exec->init_args[1].to<addr_t>(), block->height, ticket), tx->id);
						offer_ask_map.insert(std::make_tuple(exec->init_args[2].to<addr_t>(), block->height, ticket), tx->id);
					}
					offer_log.insert(std::make_pair(block->height, ticket), tx->id);
				}
				if(exec->binary == params->plot_binary) {
					if(exec->init_args.size() >= 1) {
						owner_map.insert(std::make_tuple(exec->init_args[0].to<addr_t>(), block->height, ticket), tx->id);
					}
				}
				type_hash = exec->binary;
			}
			if(auto plot = std::dynamic_pointer_cast<const contract::VirtualPlot>(contract)) {
				vplot_map.insert(plot->farmer_key, tx->id);
			}
			contract_map.insert(tx->id, contract);
			contract_type_map.insert(tx->id, type_hash);
		}
		for(const auto& op : tx->execute)
		{
			const auto ticket = counter++;
			const auto address = op->address == addr_t() ? addr_t(tx->id) : op->address;
			const auto contract = op->address == addr_t() ? tx->deploy :
					(context ? context->contract_cache.find_contract(op->address) : nullptr);

			if(auto exec = std::dynamic_pointer_cast<const operation::Execute>(op)) {
				exec_entry_t entry;
				entry.height = block->height;
				entry.txid = tx->id;
				entry.method = exec->method;
				entry.args = exec->args;
				entry.user = exec->user;
				auto deposit = std::dynamic_pointer_cast<const operation::Deposit>(exec);
				if(deposit) {
					entry.deposit = std::make_pair(deposit->currency, deposit->amount);
				}
				exec_log.insert(std::make_tuple(address, block->height, ticket), entry);

				if(auto executable = std::dynamic_pointer_cast<const contract::Executable>(contract)) {
					if(executable->binary == params->swap_binary) {
						if(exec->user && entry.args.size() > 0) {
							const auto key = std::make_pair(*exec->user, op->address);
							const auto index = entry.args[0].to<uint32_t>();
							if(index < 2) {
								std::array<uint128, 2> balance;
								swap_liquid_map.find(key, balance);
								if(exec->method == "add_liquid" && deposit) {
									balance[index] += deposit->amount;
								}
								if(exec->method == "rem_liquid" && entry.args.size() > 1) {
									balance[index] -= entry.args[1].to<uint128>();
								}
								swap_liquid_map.insert(key, balance);
							}
						}
					}
					if(executable->binary == params->offer_binary) {
						if((exec->method == "trade" || exec->method == "accept") && deposit) {
							trade_log.insert(std::make_pair(block->height, ticket), std::make_tuple(address, tx->id, deposit->amount));
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
		std::pair<int64_t, std::pair<hash_t, hash_t>> entry;
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

	std::pair<int64_t, std::pair<hash_t, hash_t>> entry;
	if(block_index.find_last(entry)) {
		state_hash = entry.second.first;
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

std::vector<Node::proof_data_t> Node::find_proof(const hash_t& challenge) const
{
	const auto iter = proof_map.find(challenge);
	if(iter != proof_map.end()) {
		return iter->second;
	}
	return {};
}

uint64_t Node::calc_block_reward(std::shared_ptr<const BlockHeader> block) const
{
	if(!block->proof || std::dynamic_pointer_cast<const ProofOfStake>(block->proof)) {
		return 0;
	}
	const auto reward = mmx::calc_block_reward(params, get_diff_header(block)->space_diff);
	return std::max(reward, params->min_reward);
}

uint64_t Node::calc_final_block_reward(std::shared_ptr<const BlockHeader> block, const uint64_t block_reward, const uint64_t total_fees) const
{
	return mmx::calc_final_block_reward(block_reward, get_diff_header(block)->average_txfee, total_fees);
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

	vnx::write(out, block->get_header());

	std::vector<std::pair<hash_t, int64_t>> tx_list;
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

	block_index.insert(block->height, std::make_pair(offset, std::make_pair(block->hash, block->content_hash)));
}


} // mmx
