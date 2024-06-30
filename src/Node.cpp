/*
 * Node.cpp
 *
 *  Created on: Dec 7, 2021
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/Challenge.hxx>
#include <mmx/ProofOfStake.hxx>
#include <mmx/ProofOfSpaceOG.hxx>
#include <mmx/ProofOfSpaceNFT.hxx>
#include <mmx/contract/Binary.hxx>
#include <mmx/contract/Executable.hxx>
#include <mmx/contract/VirtualPlot.hxx>
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
	:	NodeBase(_vnx_name),
		opencl_vdf(3)
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
					for(size_t i = 0; i < opencl_vdf.size(); ++i) {
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
	api_threads = std::make_shared<vnx::ThreadPool>(num_threads);
	vdf_threads = std::make_shared<vnx::ThreadPool>(num_vdf_threads);
	fetch_threads = std::make_shared<vnx::ThreadPool>(2);

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
		db = std::make_shared<DataBase>(num_db_threads);

		db->open_async(recv_log, database_path + "recv_log");
		db->open_async(spend_log, database_path + "spend_log");
		db->open_async(exec_log, database_path + "exec_log");
		db->open_async(memo_log, database_path + "memo_log");

		db->open_async(contract_map, database_path + "contract_map");
		db->open_async(contract_log, database_path + "contract_log");
		db->open_async(deploy_map, database_path + "deploy_map");
		db->open_async(vplot_map, database_path + "vplot_map");
		db->open_async(owner_map, database_path + "owner_map");
		db->open_async(offer_bid_map, database_path + "offer_bid_map");
		db->open_async(offer_ask_map, database_path + "offer_ask_map");
		db->open_async(trade_log, database_path + "trade_log");
		db->open_async(swap_liquid_map, database_path + "swap_liquid_map");

		db->open_async(tx_log, database_path + "tx_log");
		db->open_async(tx_index, database_path + "tx_index");
		db->open_async(hash_index, database_path + "hash_index");
		db->open_async(block_index, database_path + "block_index");
		db->open_async(balance_table, database_path + "balance_table");
		db->open_async(farmer_block_map, database_path + "farmer_block_map");
		db->open_async(total_supply_map, database_path + "total_supply_map");

		db->sync();
	}
	storage = std::make_shared<vm::StorageDB>(database_path, db);

	if(db_replay) {
		replay_height = 0;
	}
	{
		const auto height = std::min(db->min_version(), replay_height);
		revert(height);

		uint64_t balance_count = 0;
		balance_table.scan([this, &balance_count](const std::pair<addr_t, addr_t>& key, const uint128& value) -> bool {
			if(value >= 1000000 && key.second == addr_t()) {
				mmx_address_count++;
			}
			balance_count++;
			return true;
		});

		if(height) {
			log(INFO) << "Loaded DB at height " << (height - 1) << ", " << balance_count << " balances, " << mmx_address_count << " addresses"
					<< ", took " << (vnx::get_wall_time_millis() - time_begin) / 1e3 << " sec";
		}
	}
	block_chain = std::make_shared<vnx::File>(storage_path + "block_chain.dat");

	if(block_chain->exists() && (replay_height || db_replay))
	{
		block_chain->open("rb+");

		bool is_replay = true;
		uint32_t height = -1;
		block_index_t entry;
		// set block_chain position past last readable and valid block
		while(block_index.find_last(height, entry)) {
			try {
				block_chain->seek_to(entry.file_offset);
				const auto block = std::dynamic_pointer_cast<const Block>(read_block(*block_chain, true));
				if(!block) {
					throw std::runtime_error("not a block");
				}
				if(!block->is_valid()) {
					throw std::runtime_error("invalid block");
				}
				if(block->height != height) {
					throw std::runtime_error("invalid block height");
				}
				if(block->hash != state_hash) {
					throw std::runtime_error("invalid block hash");
				}
				is_replay = false;
				break;
			}
			catch(const std::exception& ex) {
				log(WARN) << "Validating on-disk peak " << height << " failed with: " << ex.what();
			}
			revert(height);
		}

		if(is_replay) {
			log(INFO) << "Creating DB (this may take a while) ...";
			int64_t last_time = vnx::get_wall_time_millis();
			int64_t block_offset = 0;
			std::vector<int64_t> tx_offsets;
			std::list<std::shared_ptr<const Block>> history;

			block_chain->seek_begin();
			try {
				while(auto header = read_block(*block_chain, true, &block_offset, &tx_offsets))
				{
					if(auto block = std::dynamic_pointer_cast<const Block>(header))
					{
						if(!block->is_valid()) {
							throw std::runtime_error("invalid block " + std::to_string(block->height));
						}
						std::shared_ptr<execution_context_t> result;
						if(block->height) {
							try {
								result = validate(block);
							} catch(std::exception& ex) {
								log(ERROR) << "Block validation at height " << block->height << " failed with: " << ex.what();
								throw;
							}
						}
						for(size_t i = 0; i < tx_offsets.size(); ++i) {
							const auto& tx = block->tx_list[i];
							tx_index.insert(tx->id, tx->get_tx_index(params, block->height, tx_offsets[i]));
						}
						block_index.insert(block->height, block->get_block_index(block_offset));

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
						{
							const auto now = vnx::get_wall_time_millis();
							if(now - last_time >= 1000) {
								log(INFO) << "DB replay height " << block->height << " ...";
							}
							last_time = now;
						}
						vnx_process(false);
					}
					if(!vnx::do_run()) {
						log(WARN) << "DB replay aborted";
						return;
					}
				}
			} catch(const std::exception& ex) {
				log(WARN) << "DB replay stopped due to error: " << ex.what();
				block_chain->seek_to(block_offset);
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
		block->vdf_output[0] = hash_t(params->network);
		block->vdf_output[1] = hash_t(params->network);
		block->tx_list.push_back(vnx::read_from_file<Transaction>("data/tx_plot_binary.dat"));
		block->tx_list.push_back(vnx::read_from_file<Transaction>("data/tx_offer_binary.dat"));
		block->tx_list.push_back(vnx::read_from_file<Transaction>("data/tx_swap_binary.dat"));
		block->tx_list.push_back(vnx::read_from_file<Transaction>("data/tx_token_binary.dat"));
		block->tx_list.push_back(vnx::read_from_file<Transaction>("data/tx_plot_nft_binary.dat"));

		for(auto tx : block->tx_list) {
			if(!tx) {
				throw std::logic_error("failed to load genesis transactions");
			}
		}
		block->finalize();

		if(!block->is_valid()) {
			throw std::logic_error("invalid genesis block");
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
	set_timer_millis(3600 * 1000, std::bind(&Node::update_control, this));
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

	threads->close();
	api_threads->close();
	vdf_threads->close();
	fetch_threads->close();

	opencl_vdf.clear();

#ifdef WITH_OPENCL
	OCL_VDF::release();
	automy::basic_opencl::release_context(opencl_context);
#endif
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
	if(tx->exec_result) {
		auto tmp = vnx::clone(tx);
		tmp->reset(params);
		tx = tmp;
	}
	if(pre_validate) {
		const auto res = validate(tx);
		if(res.did_fail) {
			throw std::runtime_error(res.get_error_msg());
		}
	}
	// Note: tx->is_valid() already checked by Router
	tx_queue[tx->content_hash] = tx;

	if(!vnx_sample) {
		publish(tx, output_transactions);
	}
}

bool Node::recv_height(const uint32_t& height) const
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
	log(INFO) << fork_tree.size() << " blocks in memory, "
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
	is_synced = false;
	sync_pos = 0;
	sync_peak = nullptr;
	sync_retry = 0;
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
				copy->reset(params);
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

std::shared_ptr<Node::fork_t> Node::find_best_fork(const uint32_t at_height) const
{
	const auto root = get_root();
	if(at_height <= root->height) {
		return nullptr;
	}
	uint32_t max_height = 0;
	uint128_t max_weight = 0;
	std::shared_ptr<fork_t> best_fork;
	const auto begin = fork_index.upper_bound(root->height);
	const auto end = fork_index.upper_bound(at_height);
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
		if(fork->is_all_proof_verified && !fork->is_invalid)
		{
			const int cond_a = block->total_weight > max_weight ? 1 : (block->total_weight == max_weight ? 0 : -1);
			const int cond_b = block->height > max_height ? 1 : (block->height == max_height ? 0 : -1);
			const int cond_c = (!best_fork || block->hash < best_fork->block->hash) ? 1 : 0;

			if(cond_a > 0 || (cond_a == 0 && cond_b > 0) || (cond_a == 0 && cond_b == 0 && cond_c > 0))
			{
				best_fork = fork;
				max_height = block->height;
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
		throw std::logic_error("apply(): block->prev != state_hash");
	}
	try {
		if(!is_replay) {
			write_block(block);
		}
		uint32_t counter = 0;
		std::vector<hash_t> tx_ids;
		std::unordered_set<hash_t> tx_set;
		balance_cache_t balance_cache(&balance_table);

		for(const auto& out : block->get_outputs(params))
		{
			if(out.memo) {
				const auto key = hash_t(out.address + (*out.memo));
				memo_log.insert(std::make_tuple(key, block->height, counter++), out);
			}
			recv_log.insert(std::make_tuple(out.address, block->height, counter++), out);
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
				if(!tx->exec_result || !tx->exec_result->did_fail) {
					apply(block, tx, counter);
				}
				tx_pool_erase(tx->id);
				tx_set.insert(tx->id);
				tx_ids.push_back(tx->id);
			}
		}
		if(!tx_ids.empty()) {
			tx_log.insert(block->height, tx_ids);
		}

		for(auto iter = tx_queue.begin(); iter != tx_queue.end();) {
			if(tx_set.count(iter->second->id)) {
				iter = tx_queue.erase(iter);
			} else {
				iter++;
			}
		}
		std::unordered_map<addr_t, std::array<uint128_t, 2>> supply_delta;

		for(const auto& entry : balance_cache.balance) {
			const auto& key = entry.first;
			const auto& value = entry.second;
			const auto& address = key.first;
			const auto& currency = key.second;
			uint128 prev = 0;
			balance_table.find(key, prev);
			if(value > prev) {
				supply_delta[currency][address != addr_t() ? 1 : 0] += value - prev;
			} else if(value < prev) {
				supply_delta[currency][0] += prev - value;
			}
			balance_table.insert(key, value);
		}
		for(const auto& entry : supply_delta) {
			const auto& currency = entry.first;
			const auto& delta = entry.second;
			if(delta[0] != delta[1]) {
				uint128 value = 0;
				total_supply_map.find(currency, value);
				value += delta[1];
				if(delta[0] > value) {
					throw std::logic_error("negative supply for " + currency.to_string());
				}
				value -= delta[0];
				total_supply_map.insert(currency, value);
			}
		}

		if(context) {
			if(auto storage = context->storage) {
				storage->commit();
			} else {
				throw std::logic_error("storage == nullptr");
			}
		}
		if(auto proof = block->proof) {
			farmed_block_info_t info;
			info.height = block->height;
			info.reward = block->reward_amount;
			info.reward_addr = (block->reward_addr ? *block->reward_addr : addr_t());
			farmer_block_map.insert(proof->farmer_key, info);
		}
		hash_index.insert(block->hash, block->height);

		state_hash = block->hash;
		contract_cache.clear();

		db->commit(block->height + 1);
	}
	catch(const std::exception& ex) {
		try {
			revert(block->height);
		} catch(const std::exception& ex) {
			log(ERROR) << "revert() failed with: " << ex.what();
		}
		throw std::runtime_error("apply() failed with: " + std::string(ex.what()));
	}
}

void Node::apply(	std::shared_ptr<const Block> block,
					std::shared_ptr<const Transaction> tx,
					uint32_t& counter)
{
	if(auto contract = tx->deploy)
	{
		const auto ticket = counter++;
		auto type_hash = hash_t(contract->get_type_name());

		contract_map.insert(tx->id, contract);
		contract_log.insert(std::make_tuple(type_hash, block->height, ticket), tx->id);

		if(auto exec = std::dynamic_pointer_cast<const contract::Executable>(contract)) {
			type_hash = exec->binary;
			if(exec->binary == params->offer_binary) {
				if(exec->init_args.size() >= 3) {
					owner_map.insert(std::make_tuple(exec->init_args[0].to<addr_t>(), block->height, ticket), std::make_pair(tx->id, type_hash));
					offer_bid_map.insert(std::make_tuple(exec->init_args[1].to<addr_t>(), block->height, ticket), tx->id);
					offer_ask_map.insert(std::make_tuple(exec->init_args[2].to<addr_t>(), block->height, ticket), tx->id);
				}
			}
			if(exec->binary == params->plot_binary) {
				if(exec->init_args.size() >= 1) {
					owner_map.insert(std::make_tuple(exec->init_args[0].to<addr_t>(), block->height, ticket), std::make_pair(tx->id, type_hash));
				}
			}
			contract_log.insert(std::make_tuple(exec->binary, block->height, ticket), tx->id);
		}
		if(tx->sender) {
			deploy_map.insert(std::make_tuple(*tx->sender, block->height, ticket), std::make_pair(tx->id, type_hash));
		}
		if(auto owner = contract->get_owner()) {
			owner_map.insert(std::make_tuple(*owner, block->height, ticket), std::make_pair(tx->id, type_hash));
		}
		if(auto plot = std::dynamic_pointer_cast<const contract::VirtualPlot>(contract)) {
			vplot_map.insert(plot->farmer_key, tx->id);
		}
	}
	for(const auto& op : tx->execute)
	{
		const auto ticket = counter++;
		const auto address = (op->address == addr_t() ? addr_t(tx->id) : op->address);
		const auto contract = (address == tx->id ? tx->deploy : get_contract(address));

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
								swap_liquid_map.insert(key, balance);
							}
							if(exec->method == "rem_liquid" && entry.args.size() > 1) {
								balance[index] -= entry.args[1].to<uint128>();
								swap_liquid_map.insert(key, balance);
							}
							if(exec->method == "rem_all_liquid") {
								balance = std::array<uint128, 2>();
								swap_liquid_map.insert(key, balance);
							}
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
	}
}

void Node::revert(const uint32_t height)
{
	const auto time_begin = vnx::get_wall_time_millis();
	if(block_chain) {
		block_index_t entry;
		if(block_index.find(height, entry)) {
			block_chain->seek_to(entry.file_offset);
		}
	}
	db->revert(height);
	contract_cache.clear();

	block_index_t entry;
	if(block_index.find_last(entry)) {
		state_hash = entry.hash;
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

bool Node::find_challenge(std::shared_ptr<const BlockHeader> block, hash_t& challenge, uint32_t offset) const
{
	if(offset > params->challenge_delay) {
		throw std::logic_error("offset out of range");
	}
	if(auto vdf_block = find_prev_header(block, params->challenge_delay - offset, true)) {
		challenge = vdf_block->vdf_output[1];
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

uint64_t Node::calc_block_reward(std::shared_ptr<const BlockHeader> block, const uint64_t total_fees) const
{
	if(!block->proof) {
		return 0;
	}
	if(block->height < params->reward_activation) {
		return 0;
	}
	uint32_t avg_txfee = 0;
	uint64_t base_reward = 0;
	if(auto prev = find_prev_header(block, 1)) {
		avg_txfee = prev->average_txfee;
		base_reward = prev->next_base_reward;
	}
	uint64_t reward = base_reward;
	if(params->min_reward > avg_txfee) {
		reward += params->min_reward - avg_txfee;
	}
	if(std::dynamic_pointer_cast<const ProofOfStake>(block->proof)) {
		reward = 0;
	}
	return mmx::calc_final_block_reward(params, reward, total_fees);
}

std::shared_ptr<const BlockHeader> Node::read_block(
		vnx::File& file, bool full_block, int64_t* block_offset, std::vector<int64_t>* tx_offsets) const
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
								tx_offsets->push_back(offset);
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

	std::vector<std::pair<hash_t, tx_index_t>> tx_list;
	for(const auto& tx : block->tx_list) {
		tx_list.emplace_back(tx->id, tx->get_tx_index(params, block->height, out.get_output_pos()));
		vnx::write(out, tx);
	}
	for(const auto& entry : tx_list) {
		tx_index.insert(entry.first, entry.second);
	}
	vnx::write(out, nullptr);	// end of block
	const auto end = out.get_output_pos();
	vnx::write(out, nullptr);	// temporary end of block_chain.dat
	block_chain->flush();
	block_chain->seek_to(end);

	block_index.insert(block->height, block->get_block_index(offset));
}


} // mmx
