/*
 * Node.cpp
 *
 *  Created on: Dec 7, 2021
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/Challenge.hxx>
#include <mmx/ProofOfSpaceOG.hxx>
#include <mmx/ProofOfSpaceNFT.hxx>
#include <mmx/contract/Binary.hxx>
#include <mmx/contract/Executable.hxx>
#include <mmx/contract/VirtualPlot.hxx>
#include <mmx/operation/Execute.hxx>
#include <mmx/operation/Deposit.hxx>
#include <mmx/utils.h>
#include <mmx/helpers.h>
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

					// TODO: optimize vdf_verify_max_pending according to GPU size
					for(uint32_t i = 0; i < vdf_verify_max_pending; ++i) {
						opencl_vdf.push_back(std::make_shared<OCL_VDF>(opencl_context, device));
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

	if(opencl_vdf.empty()) {
		vdf_verify_max_pending = 1;
	} else {
		opencl_vdf_enable = true;
	}
	threads = std::make_shared<vnx::ThreadPool>(num_threads);
	api_threads = std::make_shared<vnx::ThreadPool>(num_api_threads);
	vdf_threads = std::make_shared<vnx::ThreadPool>(std::max(num_vdf_threads, vdf_verify_max_pending));
	fetch_threads = std::make_shared<vnx::ThreadPool>(2);

	router = std::make_shared<RouterAsyncClient>(router_name);
	timelord = std::make_shared<TimeLordAsyncClient>(timelord_name);
	http = std::make_shared<vnx::addons::HttpInterface<Node>>(this, vnx_name);
	add_async_client(router);
	add_async_client(timelord);
	add_async_client(http);

	vnx::Directory(storage_path).create();
	vnx::Directory(storage_path + "forks").create();
	vnx::Directory(database_path).create();

	const auto time_begin = vnx::get_wall_time_millis();
	{
		db = std::make_shared<DataBase>(num_db_threads);

		db->open_async(txio_log, database_path + "txio_log");
		db->open_async(exec_log, database_path + "exec_log");
		db->open_async(memo_log, database_path + "memo_log");

		db->open_async(contract_map, database_path + "contract_map");
		db->open_async(contract_log, database_path + "contract_log");
		db->open_async(deploy_map, database_path + "deploy_map");
		db->open_async(owner_map, database_path + "owner_map");
		db->open_async(swap_index, database_path + "swap_index");
		db->open_async(offer_index, database_path + "offer_index");
		db->open_async(trade_log, database_path + "trade_log");
		db->open_async(trade_index, database_path + "trade_index");
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

	storage->read_balance = [this](const addr_t& address, const addr_t& currency) -> std::unique_ptr<uint128> {
		uint128 value = 0;
		if(balance_table.find(std::make_pair(address, currency), value)) {
			return std::make_unique<uint128>(value);
		}
		return nullptr;
	};

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
		{
			std::map<pubkey_t, uint32_t> farmer_block_count;
			farmer_block_map.scan([&farmer_block_count](const pubkey_t& key, const farmed_block_info_t& info) -> bool {
				farmer_block_count[key]++;
				return true;
			});
			for(const auto& entry : farmer_block_count) {
				farmer_ranking.push_back(entry);
			}
			update_farmer_ranking();
		}
		if(height) {
			log(INFO) << "Loaded DB at height " << (height - 1) << ", " << balance_count << " balances, " << mmx_address_count << " addresses, "
					<< farmer_ranking.size() << " farmers, took " << (vnx::get_wall_time_millis() - time_begin) / 1e3 << " sec";
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
							tx_index.insert(tx->id, tx->get_tx_index(params, block, tx_offsets[i]));
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
							if(now - last_time >= 5000) {
								log(INFO) << "DB replay at height " << block->height << " ...";
								last_time = now;
							}
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
			root = get_header_at(height - std::min(params->commit_delay, height));
			if(!root) {
				throw std::logic_error("failed to load root");
			}

			// fetch fork line
			for(uint32_t i = root->height + 1; i <= height; ++i) {
				if(auto block = get_block_at(i)) {
					auto fork = std::make_shared<fork_t>();
					fork->block = block;
					fork->is_vdf_verified = true;
					add_fork(fork);
				}
			}

			// load alternate forks
			for(auto file : vnx::Directory(storage_path + "forks").files()) {
				try {
					file->open("wb+");
					int64_t end = 0;
					uint32_t depth = 0;
					std::shared_ptr<const Block> peak;
					while(auto block = std::dynamic_pointer_cast<const Block>(vnx::read(file->in))) {
						if(block->height > height) {
							break;
						}
						if(block->height == root->height) {
							auto alt = std::make_shared<alt_root_t>();
							alt->file = file;
							alt->block = block;
							alt_roots[block->hash] = alt;
						} else if(block->height > root->height) {
							auto fork = std::make_shared<fork_t>();
							fork->block = block;
							fork->is_vdf_verified = true;
							add_fork(fork);
						}
						end = file->get_input_pos();
						peak = block;
						depth++;
					}
					file->seek_to(end);

					if(peak && peak >= root->height) {
						log(INFO) << "Loaded alternate fork at height " << peak->height << " with depth " << depth;
					} else {
						file->remove();
						log(WARN) << "Deleted obsolete fork: " << file->get_path();
					}
				}
				catch(const std::exception& ex) {
					log(WARN) << "Failed to load alternate fork: " << ex.what() << " (" << file->get_path() << ")";
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
		block->time_stamp = params->initial_time_stamp;
		block->time_diff = params->initial_time_diff * params->time_diff_divider;
		block->space_diff = params->initial_space_diff;
		block->vdf_output = hash_t("MMX/" + params->network + "/vdf/0");
		block->challenge = hash_t("MMX/" + params->network + "/challenge/0");
		block->tx_list.push_back(vnx::read_from_file<Transaction>("data/tx_plot_binary.dat"));
		block->tx_list.push_back(vnx::read_from_file<Transaction>("data/tx_offer_binary.dat"));
		block->tx_list.push_back(vnx::read_from_file<Transaction>("data/tx_swap_binary.dat"));
		block->tx_list.push_back(vnx::read_from_file<Transaction>("data/tx_token_binary.dat"));
		block->tx_list.push_back(vnx::read_from_file<Transaction>("data/tx_plot_nft_binary.dat"));
		block->tx_list.push_back(vnx::read_from_file<Transaction>("data/tx_nft_binary.dat"));
		block->tx_list.push_back(vnx::read_from_file<Transaction>("data/tx_template_binary.dat"));
		block->tx_list.push_back(vnx::read_from_file<Transaction>("data/tx_escrow_binary.dat"));
		block->tx_list.push_back(vnx::read_from_file<Transaction>("data/tx_time_lock_binary.dat"));
		block->tx_list.push_back(vnx::read_from_file<Transaction>("data/tx_relay_binary.dat"));

		if(auto tx = vnx::read_from_file<Transaction>("data/tx_project_relay.dat")) {
			auto exec = std::dynamic_pointer_cast<const contract::Executable>(tx->deploy);
			if(!tx->is_valid(params) || !tx->sender || tx->expires != 0xFFFFFFFF || !exec || exec->binary != params->relay_binary) {
				throw std::logic_error("invalid tx_project_relay");
			}
			block->project_addr = tx->id;
		} else {
			throw std::logic_error("failed to load tx_project_relay");
		}
		// TODO: testnet rewards

		for(auto tx : block->tx_list) {
			if(!tx) {
				throw std::logic_error("failed to load genesis transaction");
			}
			if(tx->network != params->network) {
				throw std::logic_error("invalid genesis transaction");
			}
		}
		block->finalize();
		block->content_hash = block->calc_content_hash();

		if(!block->is_valid()) {
			throw std::logic_error("invalid genesis block");
		}
		apply(block, nullptr);
		commit(block);
	}

	if(vdf_slave_mode) {
		subscribe(input_vdf_points, max_queue_ms);
	} else {
		subscribe(input_vdfs, max_queue_ms);
	}
	subscribe(input_proof, max_queue_ms);
	subscribe(input_votes, max_queue_ms);
	subscribe(input_blocks, max_queue_ms);
	subscribe(input_transactions, max_queue_ms);
	subscribe(input_timelord_vdfs, max_queue_ms);
	subscribe(input_harvester_proof, max_queue_ms);
	subscribe(output_votes, max_queue_ms);

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
	const auto root = get_root();
	if(block->height <= root->height) {
		return;
	}
	try {
		if(!block->is_valid()) {
			throw std::logic_error("invalid block");
		}
		// need to verify farmer_sig before adding to fork tree
		block->validate();
	}
	catch(const std::exception& ex) {
		log(WARN) << "Pre-validation failed for a block at height " << block->height << ": " << ex.what();
		return;
	}
	auto fork = std::make_shared<fork_t>();
	fork->block = block;
	add_fork(fork);

	if(is_synced) {
		const auto prev_height = block->height - 1;
		if(!fork_tree.count(block->prev) && prev_height > 0) {
			// fetch missed previous
			router->get_blocks_at(prev_height,
				[this](const std::vector<std::shared_ptr<const Block>>& blocks) {
					for(auto block : blocks) {
						add_block(block);
					}
				});
			log(WARN) << "Fetched missing block at height " << prev_height << " (" << block->prev.to_string() << ")";
		}
		trigger_update();
	}
}

void Node::add_fork(std::shared_ptr<fork_t> fork)
{
	const auto root = get_root();
	const auto block = fork->block;
	if(block->height <= root->height) {
		return;
	}
	if(!fork->recv_time) {
		fork->recv_time = vnx::get_wall_time_millis();
	}
	fork->proof_score_224 = block->proof_hash.to_uint<uint256_t>(true) >> 32;

	if(fork_tree.emplace(block->hash, fork).second) {
		fork_index.emplace(block->height, fork);
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
	if(tx_queue.size() < max_tx_queue) {
		// Note: tx->is_valid() already checked by Router
		tx_queue[tx->content_hash] = tx;
	}
	if(!vnx_sample) {
		publish(tx, output_transactions);
	}
}

void Node::handle(std::shared_ptr<const Block> block)
{
	add_block(block);
}

void Node::handle(std::shared_ptr<const Transaction> tx)
{
	if(is_synced) {
		add_transaction(tx);
	}
}

void Node::handle(std::shared_ptr<const ProofOfTime> value)
{
	vdf_queue.emplace_back(value, vnx::get_wall_time_millis());
	trigger_update();
}

void Node::handle(std::shared_ptr<const VDF_Point> value)
{
	if(value->vdf_height > get_vdf_height()) {
		log(INFO) << "-------------------------------------------------------------------------------";
	}
	if(find_vdf_point(value->input, value->output)) {
		return;		// duplicate
	}
	const auto vdf_iters = value->start + value->num_iters;
	vdf_index.emplace(vdf_iters, value);
	vdf_tree.emplace(value->output, value);

	log(INFO) << "\U0001F552 Received VDF point for height " << value->vdf_height;

	trigger_update();
}

void Node::handle(std::shared_ptr<const ProofResponse> value)
{
	proof_queue.emplace_back(value, vnx::get_wall_time_millis());
}

void Node::handle(std::shared_ptr<const ValidatorVote> value)
{
	if(!value->is_valid()) {
		return;
	}
	vote_queue.emplace_back(value, vnx::get_wall_time_millis());
	verify_votes();
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
	{
		std::ofstream file(storage_path + "timelord_trust.txt", std::ios::trunc);
		for(const auto& entry : timelord_trust) {
			file << entry.first << "\t" << entry.second << std::endl;
		}
	}
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
	{
		std::unique_lock lock(db_mutex);
		is_synced = false;
	}
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
		return;		// wait for VDF checks
	}
	if(get_height() + max_sync_ahead < sync_pos) {
		return;		// limit blocks in memory during sync
	}
	const size_t max_pending = sync_retry ? 2 : std::max(std::min<int>(max_sync_pending, max_sync_jobs), 4);

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
		add_block(block);
		total_size += block->static_cost;
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
		if((height % max_sync_jobs == 0 || sync_pending.empty()) && !sync_retry) {
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

std::shared_ptr<const BlockHeader> Node::fork_to(std::shared_ptr<fork_t> peak)
{
	const auto root = get_root();
	const auto prev_state = state_hash;
	const auto fork_line = get_fork_line(peak);

	bool did_fork = false;
	std::shared_ptr<const BlockHeader> forked_at;

	const auto alt = find_value(alt_roots, fork_line[0]->block->prev, nullptr);
	if(alt) {
		// TODO
	} else {
		forked_at = root;
		const auto old_fork = get_fork_line();
		for(size_t i = 0; i < fork_line.size() && i < old_fork.size(); ++i) {
			if(fork_line[i] != old_fork[i]) {
				did_fork = true;
				break;
			}
			forked_at = old_fork[i]->block;
		}
		if(did_fork) {
			revert(forked_at->height + 1);
		}
	}

	// check for competing forks
	for(const auto& fork : fork_line)
	{
		if(fork_index.count(fork->block->height) > 1) {
			fork->ahead_count = 0;
		} else if(auto prev = fork->prev.lock()) {
			fork->ahead_count = prev->ahead_count + 1;
		} else {
			fork->ahead_count = 0;
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
				fork->context = validate(block);

				if(!fork->is_vdf_verified) {
					if(fork->ahead_count > 0) {
						check_vdf(fork);
					} else {
						fork->is_vdf_verified = true;
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
				for(auto point : fork->vdf_points) {
					publish(point->proof, output_verified_vdfs);
				}
				publish(block, output_verified_blocks);
			}
		}
		apply(block, fork->context);
	}
	return did_fork ? forked_at : nullptr;
}

std::shared_ptr<Node::fork_t> Node::find_best_fork() const
{
	const auto root = get_root();

	std::shared_ptr<fork_t> best;
	for(auto iter = fork_index.upper_bound(root->height); iter != fork_index.end(); ++iter)
	{
		const auto& fork = iter->second;
		const auto& block = fork->block;
		const auto  prev = fork->prev.lock();

		if(block->prev == root->hash || alt_roots.count(block->prev)) {
			fork->total_votes = fork->votes;
			fork->proof_score_sum = fork->proof_score_224;
			fork->is_all_proof_verified = fork->is_proof_verified;
		} else if(prev) {
			fork->is_invalid = prev->is_invalid || fork->is_invalid;
			fork->total_votes = prev->total_votes + fork->votes;
			fork->proof_score_sum = prev->proof_score_sum + fork->proof_score_224;
			fork->is_all_proof_verified = prev->is_all_proof_verified && fork->is_proof_verified;
		} else {
			fork->is_all_proof_verified = false;
		}

		if(fork->is_all_proof_verified && !fork->is_invalid)
		{
			const auto peak = best ? best->block : nullptr;
			const int cond_a = (!peak || block->total_weight > peak->total_weight) ? 1 : (block->total_weight == peak->total_weight ? 0 : -1);
			const int cond_b = (!best || fork->total_votes > best->total_votes) ? 1 : (fork->total_votes == best->total_votes ? 0 : -1);
			const int cond_c = (!best || fork->proof_score_sum < best->proof_score_sum) ? 1 : (fork->proof_score_sum == best->proof_score_sum ? 0 : -1);
			const int cond_d = (!peak || block->hash < peak->hash) ? 1 : 0;

			if(cond_a > 0
				|| (cond_a == 0 && cond_b > 0)
				|| (cond_a == 0 && cond_b == 0 && cond_c > 0)
				|| (cond_a == 0 && cond_b == 0 && cond_c == 0 && cond_d > 0))
			{
				best = fork;
			}
		}
	}
	return best;
}

std::vector<std::shared_ptr<Node::fork_t>> Node::get_fork_line(std::shared_ptr<fork_t> peak) const
{
	const auto root = get_root();
	std::vector<std::shared_ptr<fork_t>> line;
	auto fork = peak ? peak : find_fork(state_hash);
	if(!fork) {
		return {};
	}
	while(fork) {
		line.push_back(fork);
		if(fork->block->prev == root->hash || alt_roots.count(fork->block->prev)) {
			std::reverse(line.begin(), line.end());
			return line;
		}
		fork = fork->prev.lock();
	}
	throw std::logic_error("disconnected fork");
}

void Node::update_farmer_ranking()
{
	std::sort(farmer_ranking.begin(), farmer_ranking.end(),
		[](const std::pair<pubkey_t, uint32_t>& L, const std::pair<pubkey_t, uint32_t>& R) -> bool {
			return L.second > R.second;
		});
}

void Node::commit(std::shared_ptr<const Block> block)
{
	const auto height = block->height;
	if(height) {
		const auto root = get_root();
		if(block->prev != root->hash) {
			throw std::logic_error("cannot commit height " + std::to_string(height) + " after " + std::to_string(root->height));
		}
	}
	const auto fork = find_fork(block->hash);
	{
		const auto begin = challenge_map.begin();
		const auto end = challenge_map.upper_bound(block->vdf_height);
		for(auto iter = begin; iter != end; ++iter) {
			proof_map.erase(iter->second);
		}
		challenge_map.erase(begin, end);
	}
	{
		const auto begin = vdf_index.begin();
		const auto end = vdf_index.upper_bound(block->vdf_iters);
		for(auto iter = begin; iter != end; ++iter) {
			vdf_tree.erase(iter->second->output);
		}
		vdf_index.erase(begin, end);
	}
	fork_tree.erase(block->hash);

	const auto range = fork_index.equal_range(height);
	for(auto iter = range.first; iter != range.second; ++iter)
	{
		const auto& fork = iter->second;
		const auto& block = fork->block;
		if(!fork_tree.erase(block->hash)) {
			continue;
		}
		auto alt = find_value(alt_roots, block->prev, nullptr);
		if(alt) {
			// existing fork
			alt_roots.erase(block->prev);
		}
		else if(root && block->prev == root->hash) {
			// new alternate fork
			alt = std::make_shared<alt_root_t>();
			alt->file = std::make_shared<vnx::File>(
					storage_path + "forks/" + std::to_string(height) + "_" + block->hash.to_string());
			alt->file->open("wb+");
			log(WARN) << "New alternate fork at height " << height << ", block " << block->hash;
		}
		if(alt) {
			auto& out = alt->file->out;
			vnx::write(out, block);
			const auto end = out.get_output_pos();
			vnx::write(out, nullptr);	// temporary end
			alt->file->seek_to(end);
			alt->file->flush();
			alt->block = block;
			alt_roots[block->hash] = alt;
		}
	}
	fork_index.erase(height);

	root = block;	// update root at end

	history[block->hash] = block->get_header();
	history_log.emplace(height, block->hash);

	// purge history
	for(auto iter = history_log.begin(); iter != history_log.end() && iter->first + max_history < height;) {
		history.erase(iter->second);
		iter = history_log.erase(iter);
	}

	if(is_synced) {
		std::string ksize = "N/A";
		std::string score = "N/A";
		if(height) {
			if(auto proof = std::dynamic_pointer_cast<const ProofOfSpaceOG>(block->proof[0])) {
				ksize = std::to_string(proof->ksize);
				score = std::to_string(proof->score);
			} else if(auto proof = std::dynamic_pointer_cast<const ProofOfSpaceNFT>(block->proof[0])) {
				ksize = std::to_string(proof->ksize);
				score = std::to_string(proof->score);
			}
		}
		Node::log(INFO)
				<< "Committed height " << height << " with: ntx = " << block->tx_list.size()
				<< ", k = " << ksize << ", score = " << score
				<< ", votes = " << (fork ? fork->votes : 0) << " of " << (fork ? fork->validators.size() : 0)
				<< ", tdiff = " << block->time_diff << ", sdiff = " << block->space_diff;
	}
	publish(block, output_committed_blocks, is_synced ? 0 : BLOCKING);
}

size_t Node::prefetch_balances(const std::set<std::pair<addr_t, addr_t>>& keys) const
{
	if(keys.size() < 64) {
		return 0;
	}
	std::atomic<size_t> total {0};
	for(const auto& key : keys) {
		threads->add_task([this, &key, &total]() {
			total += balance_table.count(key);
		});
	}
	threads->sync();
	return total;
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

		const auto block_inputs = block->get_inputs(params);
		const auto block_outputs = block->get_outputs(params);
		{
			std::set<std::pair<addr_t, addr_t>> keys;
			for(const auto& io : block_inputs) {
				keys.emplace(io.address, io.contract);
			}
			for(const auto& io : block_outputs) {
				keys.emplace(io.address, io.contract);
			}
			const auto count = prefetch_balances(keys);
			log(DEBUG) << "apply(): pre-fetched " << count << " / " << keys.size() << " balance entries at height " << block->height;
		}
		for(const auto& out : block_outputs)
		{
			if(out.memo) {
				const auto key = hash_t(out.address + (*out.memo));
				memo_log.insert(std::make_tuple(key, block->height, counter), out.address);
			}
			txio_log.insert(std::make_tuple(out.address, block->height, counter), out);
			balance_cache.get(out.address, out.contract) += out.amount;
			counter++;
		}
		for(const auto& in : block_inputs)
		{
			if(auto balance = balance_cache.find(in.address, in.contract)) {
				clamped_sub_assign(*balance, in.amount);
			}
			if(in.memo) {
				const auto key = hash_t(in.address + (*in.memo));
				memo_log.insert(std::make_tuple(key, block->height, counter), in.address);
			}
			txio_log.insert(std::make_tuple(in.address, block->height, counter), in);
			counter++;
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
		if(block->height) {
			farmed_block_info_t info;
			info.height = block->height;
			info.reward = block->reward_amount;
			info.reward_addr = (block->reward_addr ? *block->reward_addr : addr_t());

			const auto& farmer_key = block->get_farmer_key();
			farmer_block_map.insert(farmer_key, info);

			bool found = false;
			for(auto& entry : farmer_ranking) {
				if(entry.first == farmer_key) {
					entry.second++;
					found = true;
					break;
				}
			}
			if(!found) {
				farmer_ranking.emplace_back(farmer_key, 1);
			}
			update_farmer_ranking();
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
				const auto bid_currency = exec->get_arg(1).to<addr_t>();
				const auto ask_currency = exec->get_arg(2).to<addr_t>();
				offer_index.insert(std::make_tuple(hash_t(ask_currency + "ANY"), block->height, ticket), tx->id);
				offer_index.insert(std::make_tuple(hash_t("ANY" + bid_currency), block->height, ticket), tx->id);
				offer_index.insert(std::make_tuple(hash_t(ask_currency + bid_currency), block->height, ticket), tx->id);
			}
			if(exec->binary == params->swap_binary) {
				const auto token = exec->get_arg(0).to<addr_t>();
				const auto currency = exec->get_arg(1).to<addr_t>();
				swap_index.insert(std::make_tuple(hash_t(token + "ANY"), block->height, ticket), tx->id);
				swap_index.insert(std::make_tuple(hash_t("ANY" + currency), block->height, ticket), tx->id);
				swap_index.insert(std::make_tuple(hash_t(token + currency), block->height, ticket), tx->id);
			}
			int owner_index = -1;

			if(exec->binary == params->plot_binary
				|| exec->binary == params->plot_nft_binary
				|| exec->binary == params->offer_binary
				|| exec->binary == params->time_lock_binary)
			{
				owner_index = 0;
			}
			if(exec->binary == params->escrow_binary) {
				owner_index = 2;
			}
			if(owner_index >= 0) {
				owner_map.insert(std::make_tuple(exec->get_arg(owner_index).to<addr_t>(), block->height, ticket), std::make_pair(tx->id, type_hash));
			}
			contract_log.insert(std::make_tuple(exec->binary, block->height, ticket), tx->id);
		}
		if(tx->sender) {
			deploy_map.insert(std::make_tuple(*tx->sender, block->height, ticket), std::make_pair(tx->id, type_hash));
		}
		if(auto owner = contract->get_owner()) {
			owner_map.insert(std::make_tuple(*owner, block->height, ticket), std::make_pair(tx->id, type_hash));
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
			entry.time_stamp = block->time_stamp;
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
						trade_log_t log;
						log.time_stamp = block->time_stamp;
						log.txid = tx->id;
						log.address = address;
						log.ask_amount = deposit->amount;
						exec->get_arg(1).to(log.inv_price);
						trade_log.insert(std::make_pair(block->height, ticket), log);
						try {
							const auto ask_currency = deposit->currency;
							const auto bid_currency = to_addr(read_storage_field(address, "bid_currency").first);
							trade_index.insert(std::make_tuple(hash_t(ask_currency + "ANY"), block->height, ticket), true);
							trade_index.insert(std::make_tuple(hash_t("ANY" + bid_currency), block->height, ticket), true);
							trade_index.insert(std::make_tuple(hash_t(ask_currency + bid_currency), block->height, ticket), true);
						} catch(...) {
							// ignore
						}
					}
				}
			}
		}
	}
}

void Node::revert(const uint32_t height)
{
	const auto time_begin = vnx::get_wall_time_millis();
	const auto peak = get_peak();

	for(auto block = peak; block && block->height >= height; block = find_prev_header(block))
	{
		// add removed tx back to pool
		if(auto full = std::dynamic_pointer_cast<const Block>(block)) {
			for(const auto& tx : full->tx_list) {
				tx_pool_t entry;
				auto copy = vnx::clone(tx);
				copy->reset(params);
				entry.tx = copy;
				entry.fee = tx->exec_result->total_fee;
				entry.cost = tx->exec_result->total_cost;
				entry.is_valid = true;
				tx_pool_update(entry, true);
			}
		}
		// revert farmer_ranking
		if(block->height) {
			const auto& farmer_key = block->get_farmer_key();
			for(auto& entry : farmer_ranking) {
				if(entry.first == farmer_key) {
					if(entry.second) {
						entry.second--;
					}
					break;
				}
			}
		}
		// store reverted blocks on-disk for evidence
		if(is_synced && block->height < peak->height)
		{
			const auto file_name = storage_path + "blocks/"
					+ std::to_string(block->height) + "_" + block->hash.to_string() + ".dat";
			try {
				vnx::write_to_file(file_name, block);
			} catch(const std::exception& ex) {
				log(WARN) << "Failed to write block: " << ex.what() << " (" << file_name << ")";
			}
		}
	}
	update_farmer_ranking();

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
	if(!root) {
		throw std::logic_error("have no root");
	}
	return root;
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

bool Node::find_challenge(std::shared_ptr<const BlockHeader> block, const uint32_t offset, hash_t& challenge, uint64_t& space_diff) const
{
	if(offset > params->challenge_delay)
	{
		const auto advance = offset - params->challenge_delay;
		if(advance > params->max_vdf_count) {
			return false;
		}
		hash_t tmp = block->challenge;
		for(uint32_t i = 0; i < advance; ++i) {
			tmp = hash_t(std::string("next_challenge") + tmp);
		}
		challenge = tmp;
		space_diff = block->space_diff;
		return true;
	}
	const uint32_t target = params->challenge_delay - offset;

	std::vector<std::shared_ptr<const BlockHeader>> chain = {block};
	{
		std::shared_ptr<const BlockHeader> iter = block;
		for(uint32_t i = 0; i < target; ++i) {
			if(auto block = find_prev_header(iter)) {
				chain.push_back(block);
				iter = block;
			} else if(iter->height) {
				return false;
			} else {
				break;
			}
		}
	}
	std::reverse(chain.begin(), chain.end());

	std::vector<std::pair<hash_t, uint64_t>> list;
	std::shared_ptr<const BlockHeader> prev;
	for(auto block : chain) {
		if(prev) {
			auto tmp = prev->challenge;
			for(uint32_t i = 1; i < block->vdf_count; ++i) {
				tmp = hash_t(std::string("next_challenge") + tmp);
				list.emplace_back(tmp, prev->space_diff);
			}
		}
		list.emplace_back(block->challenge, block->space_diff);
		prev = block;
	}
	std::reverse(list.begin(), list.end());

	while(target >= list.size()) {
		// generate challenges for "before" genesis
		const auto prev = list.back();
		list.emplace_back(hash_t(std::string("prev_challenge") + prev.first), prev.second);
	}
	const auto& out = list[target];
	challenge = out.first;
	space_diff = out.second;
	return true;
}

bool Node::find_challenge(const uint32_t vdf_height, hash_t& challenge, uint64_t& space_diff) const
{
	auto block = get_peak();
	if(block) {
		if(vdf_height > block->vdf_height) {
			if(vdf_height - block->vdf_height > params->max_vdf_count) {
				return false;
			}
		} else if(block->vdf_height - vdf_height > max_history) {
			return false;
		}
	}
	while(block && block->vdf_height > vdf_height) {
		block = find_prev_header(block);
	}
	if(!block) {
		return false;
	}
	return find_challenge(block, vdf_height - block->vdf_height, challenge, space_diff);
}

hash_t Node::get_challenge(std::shared_ptr<const BlockHeader> block, const uint32_t offset, uint64_t& space_diff) const
{
	hash_t challenge;
	if(!find_challenge(block, offset, challenge, space_diff)) {
		throw std::logic_error("cannot find challenge");
	}
	return challenge;
}

uint64_t Node::get_time_diff(std::shared_ptr<const BlockHeader> infused) const
{
	if(auto prev = find_prev_header(infused, params->commit_delay, true)) {
		return prev->time_diff;
	}
	throw std::logic_error("cannot get time difficulty");
}

bool Node::find_infusion(std::shared_ptr<const BlockHeader> block, const uint32_t offset, hash_t& value, uint64_t& num_iters) const
{
	if(offset < params->infuse_delay)
	{
		const uint32_t delta = params->infuse_delay - offset;
		const uint32_t target = block->vdf_height > delta ? block->vdf_height - delta : 0;

		while(block && block->vdf_height > target) {
			block = find_prev_header(block);
		}
	}
	if(block) {
		num_iters = get_block_iters(params, get_time_diff(block));
		value = block->hash;
		return true;
	}
	return false;
}

hash_t Node::get_infusion(std::shared_ptr<const BlockHeader> block, const uint32_t offset, uint64_t& num_iters) const
{
	hash_t value;
	if(!find_infusion(block, offset, value, num_iters)) {
		throw std::logic_error("cannot find infusion");
	}
	return value;
}

std::pair<uint32_t, hash_t> Node::get_vdf_peak_ex() const
{
	if(auto peak = get_peak()) {
		const auto points = find_next_vdf_points(peak);
		if(points.empty()) {
			return std::make_pair(peak->vdf_height, peak->vdf_output);
		} else {
			return std::make_pair(peak->vdf_height + points.size(), points.back()->output);
		}
	}
	return std::make_pair(0, hash_t());		// should never happen
}

std::shared_ptr<const VDF_Point>
Node::find_vdf_point(const hash_t& input, const hash_t& output) const
{
	const auto range = vdf_tree.equal_range(output);
	for(auto iter = range.first; iter != range.second; ++iter) {
		const auto& point = iter->second;
		if(point->input == input && point->output == output) {
			return point;
		}
	}
	return nullptr;
}

std::vector<std::shared_ptr<const VDF_Point>>
Node::find_vdf_points(std::shared_ptr<const BlockHeader> block) const
{
	const auto prev = find_prev_header(block);
	if(!prev) {
		return {};
	}
	std::vector<std::shared_ptr<const VDF_Point>> out;

	auto output = block->vdf_output;
	auto vdf_iters = prev->vdf_iters;
	while(out.size() < block->vdf_count) {
		hash_t infuse;
		uint64_t num_iters = 0;
		const auto offset = block->vdf_count - out.size() - 1;
		if(!find_infusion(prev, offset, infuse, num_iters)) {
			return {};
		}
		bool found = false;
		const auto range = vdf_tree.equal_range(output);
		for(auto iter = range.first; iter != range.second; ++iter)
		{
			const auto& point = iter->second;
			if(point->output == output
				&& point->prev == infuse
				&& point->num_iters == num_iters)
			{
				output = point->input;
				vdf_iters += num_iters;
				out.push_back(point);
				found = true;
				break;
			}
		}
		if(!found) {
			return {};
		}
	}
	if(output != prev->vdf_output) {
		return {};
	}
	std::reverse(out.begin(), out.end());
	return out;
}

std::vector<std::shared_ptr<const VDF_Point>> Node::find_next_vdf_points(std::shared_ptr<const BlockHeader> block) const
{
	std::vector<std::shared_ptr<const VDF_Point>> peaks;
	std::unordered_map<hash_t, std::shared_ptr<const VDF_Point>> fork_map;
	fork_map[block->vdf_output] = nullptr;

	auto vdf_iters = block->vdf_iters;
	for(uint32_t i = 0; i < params->max_vdf_count; ++i)
	{
		uint64_t num_iters = 0;
		const auto infuse = get_infusion(block, i, num_iters);

		vdf_iters += num_iters;
		std::vector<std::shared_ptr<const VDF_Point>> new_peaks;

		const auto range = vdf_index.equal_range(vdf_iters);
		for(auto iter = range.first; iter != range.second; ++iter)
		{
			const auto& point = iter->second;
			if(fork_map.count(point->input)
				&& point->prev == infuse
				&& point->num_iters == num_iters)
			{
				fork_map[point->output] = point;
				new_peaks.push_back(point);
			}
		}
		if(new_peaks.size()) {
			peaks = std::move(new_peaks);
		} else {
			break;
		}
	}
	if(peaks.empty()) {
		return {};
	}
	std::sort(peaks.begin(), peaks.end(),
		[]( const std::shared_ptr<const VDF_Point>& L, const std::shared_ptr<const VDF_Point>& R) -> bool {
			return L->recv_time < R->recv_time;
		});

	std::vector<std::shared_ptr<const VDF_Point>> out;
	for(auto point = peaks[0]; point; point = fork_map[point->input]) {
		out.push_back(point);
	}
	std::reverse(out.begin(), out.end());
	return out;
}

std::set<pubkey_t> Node::get_validators(std::shared_ptr<const BlockHeader> block) const
{
	block = find_prev_header(block, params->commit_delay);

	std::set<pubkey_t> set;
	for(uint32_t k = 0; block && k < max_history && set.size() < params->max_validators; ++k)
	{
		for(size_t i = 1; i < block->proof.size() && set.size() < params->max_validators; ++i) {
			set.insert(block->proof[i]->farmer_key);
		}
		block = find_prev_header(block);
	}
	return set;
}

vnx::optional<Node::proof_data_t> Node::find_best_proof(const hash_t& challenge) const
{
	const auto iter = proof_map.find(challenge);
	if(iter != proof_map.end()) {
		const auto& list = iter->second;
		if(list.size()) {
			return list.front();
		}
	}
	return nullptr;
}

uint64_t Node::calc_block_reward(std::shared_ptr<const BlockHeader> block, const uint64_t total_fees) const
{
	if(block->proof.empty()) {
		return 0;
	}
	if(block->height < params->reward_activation) {
		return 0;
	}
	uint64_t base_reward = 0;
	uint64_t reward_deduction = 0;
	if(auto prev = find_prev_header(block)) {
		base_reward = prev->base_reward;
		reward_deduction = calc_min_reward_deduction(params, prev->txfee_buffer);
	}
	uint64_t reward = base_reward;
	if(params->min_reward > reward_deduction) {
		reward += params->min_reward - reward_deduction;
	}
	return mmx::calc_final_block_reward(params, reward, total_fees);
}

vnx::optional<addr_t> Node::get_vdf_reward_winner(std::shared_ptr<const BlockHeader> block) const
{
	std::map<addr_t, uint32_t> win_map;
	for(uint32_t i = 0; i < params->vdf_reward_interval; ++i) {
		if(auto prev = find_prev_header(block)) {
			for(const auto& addr : prev->vdf_reward_addr) {
				win_map[addr]++;
			}
			block = prev;
		} else {
			break;
		}
	}
	hash_t max_hash;
	uint32_t max_count = 0;
	vnx::optional<addr_t> out;

	for(const auto& entry : win_map) {
		const auto& address = entry.first;
		const auto& count = entry.second;
		const hash_t hash(address + block->proof_hash);
		if(count > max_count || (count == max_count && hash < max_hash)) {
			out = address;
			max_count = count;
			max_hash = hash;
		}
	}
	return out;
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
		tx_list.emplace_back(tx->id, tx->get_tx_index(params, block, out.get_output_pos()));
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
