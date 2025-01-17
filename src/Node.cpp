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
#include <mmx/operation/Execute.hxx>
#include <mmx/operation/Deposit.hxx>
#include <mmx/FarmerClient.hxx>
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

		struct device_t {
			int index = 0;
			std::string name;
			std::string platform;
			cl_device_id device_id;
			cl_platform_id platform_id;
		};
		std::vector<device_t> list;
		int listsel = -1;

		for(const auto idp : platforms) {
			const auto devices = automy::basic_opencl::get_devices(idp, CL_DEVICE_TYPE_GPU);
			const auto platform = automy::basic_opencl::get_platform_name(idp);
			for(const auto idd : devices) {
				int device_index = 0;
				const auto name = automy::basic_opencl::get_device_name(idd);
				for(const auto& device : list) {
					if(device.name == name) {
						device_index++;
					}
				}
				list.push_back({device_index, name, platform, idd, idp});
				log(INFO) << "Found OpenCL GPU device '" << name << "' [" << device_index << "] (" << platform << ")";
			}
		}

		std::vector<std::pair<std::string, int>> device_list;
		for(const auto& device : list) {
			device_list.emplace_back(device.name, device.index);
		}
		vnx::write_config("Node.opencl_device_list", device_list);

		if(opencl_device >= 0) {
			if(list.size()) {
				if(!opencl_device_name.empty()) {
					for(size_t i = 0; i < list.size() && listsel < 0; ++i) {
						if(opencl_device_name == list[i].name && opencl_device == list[i].index) {
							listsel = i;
						}
					}
				}
				else {
					if(platform_name.empty()) {
						platform_name = list[0].platform;
					}
					int devidx = -1;
					for(size_t i = 0; i < list.size() && listsel < 0; ++i) {
						if(platform_name == list[i].platform) {
							devidx++;
						}
						if(devidx >= opencl_device) {
							listsel = i;
						}
					}
				}
				if(listsel >= 0) {
					const auto& device = list[listsel];
					opencl_context = automy::basic_opencl::create_context(device.platform_id, {device.device_id});

					// TODO: optimize vdf_verify_max_pending according to GPU size
					for(uint32_t i = 0; i < max_vdf_verify_pending; ++i) {
						opencl_vdf.push_back(std::make_shared<OCL_VDF>(opencl_context, device.device_id));
					}
					log(INFO) << "Using OpenCL GPU device '" << device.name << "' [" << device.index << "] (" << device.platform << ")";
				}
				else {
					log(WARN) << "No such OpenCL GPU device '" << opencl_device_name << "' [" << opencl_device << "]";
				}
			}
			else {
				log(INFO) << "No OpenCL devices found";
			}
		}
		else {
			log(INFO) << "No OpenCL device used (disabled)";
		}
		vnx::write_config("Node.opencl_device_select", listsel);
	}
	catch(const std::exception& ex) {
		log(WARN) << "Failed to create OpenCL GPU context: " << ex.what();
	}
#endif

	if(opencl_vdf.size()) {
		opencl_vdf_enable = true;
	} else {
		max_vdf_verify_pending = 1;
	}
	threads = std::make_shared<vnx::ThreadPool>(num_threads);
	api_threads = std::make_shared<vnx::ThreadPool>(num_api_threads);
	vdf_threads = std::make_shared<vnx::ThreadPool>(max_vdf_verify_pending);
	fetch_threads = std::make_shared<vnx::ThreadPool>(2);

	router = std::make_shared<RouterAsyncClient>(router_name);
	timelord = std::make_shared<TimeLordAsyncClient>(timelord_name);
	http = std::make_shared<vnx::addons::HttpInterface<Node>>(this, vnx_name);
	add_async_client(router);
	add_async_client(timelord);
	add_async_client(http);

	vnx::Directory(storage_path).create();
	vnx::Directory(database_path).create();

	const auto time_begin = get_time_ms();
	{
		db = std::make_shared<DataBase>(num_db_threads);

		db->open_async(txio_log, database_path + "txio_log");
		db->open_async(exec_log, database_path + "exec_log");
		db->open_async(memo_log, database_path + "memo_log");

		db->open_async(contract_map, database_path + "contract_map");
		db->open_async(contract_log, database_path + "contract_log");
		db->open_async(contract_depends, database_path + "contract_depends");
		db->open_async(deploy_map, database_path + "deploy_map");
		db->open_async(owner_map, database_path + "owner_map");
		db->open_async(swap_index, database_path + "swap_index");
		db->open_async(offer_index, database_path + "offer_index");
		db->open_async(trade_log, database_path + "trade_log");
		db->open_async(trade_index, database_path + "trade_index");
		db->open_async(swap_liquid_map, database_path + "swap_liquid_map");

		db->open_async(tx_log, database_path + "tx_log");
		db->open_async(tx_index, database_path + "tx_index");
		db->open_async(height_map, database_path + "height_map");
		db->open_async(balance_table, database_path + "balance_table");
		db->open_async(farmer_block_map, database_path + "farmer_block_map");
		db->open_async(total_supply_map, database_path + "total_supply_map");

		db->sync();
		db->recover();
	}
	{
		db_blocks = std::make_shared<DataBase>(2);

		db_blocks->open_async(block_index, database_path + "block_index");
		db_blocks->open_async(height_index, database_path + "height_index");

		db_blocks->sync();
		db_blocks->recover();
	}
	storage = std::make_shared<vm::StorageDB>(database_path, db);

	storage->read_balance = [this](const addr_t& address, const addr_t& currency) -> std::unique_ptr<uint128> {
		uint128 value = 0;
		if(balance_table.find(std::make_pair(address, currency), value)) {
			return std::make_unique<uint128>(value);
		}
		return nullptr;
	};

	blocks = std::make_shared<vnx::File>(database_path + "blocks.dat");
	{
		const auto height = std::min(db->min_version(), revert_height);
		revert(height);
		reset();

		log(INFO) << "Loaded DB at height " << get_height() << ", " << mmx_address_count << " addresses, "
				<< farmer_ranking.size() << " farmers, took " << (get_time_ms() - time_begin) / 1e3 << " sec";
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

	set_timer_millis(60 * 1000, std::bind(&Node::print_stats, this));
	set_timer_millis(30 * 1000, std::bind(&Node::purge_tx_pool, this));
	set_timer_millis(3600 * 1000, std::bind(&Node::update_control, this));
	set_timer_millis(validate_interval_ms, std::bind(&Node::validate_new, this));

	update_timer = set_timer_millis(update_interval_ms, std::bind(&Node::update, this));
	stuck_timer = set_timer_millis(sync_loss_delay * 1000, std::bind(&Node::on_stuck_timeout, this));

	if(run_tests) {
		is_synced = true;
		const auto state = state_hash;
		const auto version = db_blocks->version();
		bool failed = false;
		try {
			test_all();
		} catch(const std::exception& ex) {
			failed = true;
			log(WARN) << "Test failed with: " << ex.what();
		}
		db_blocks->revert(version);
		fork_to(state);
		if(failed) {
			return;
		}
		reset();
		is_synced = false;
	}

	vnx::Handle<mmx::Router> router = new mmx::Router("Router");
	router->node_server = vnx_name;
	router->storage_path = storage_path;
	router.start();

	if(do_sync) {
		start_sync(true);
	} else {
		is_synced = true;
	}
	update();

	Super::main();

	blocks->close();
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

void Node::init_chain()
{
	auto block = Block::create();
	block->nonce = params->port;
	block->time_stamp = params->initial_time_stamp;
	block->time_diff = params->initial_time_diff * params->time_diff_divider;
	block->space_diff = params->initial_space_diff;
	block->vdf_output = hash_t("MMX/" + params->network + "/vdf/0");
	block->challenge = hash_t("MMX/" + params->network + "/challenge/0");

	std::shared_ptr<const Transaction> tx_rewards;
	vnx::from_string(read_file("data/tx_testnet_rewards.json"), tx_rewards);
	if(!tx_rewards) {
		throw std::logic_error("failed to read testnet rewards");
	}
	block->tx_list.push_back(tx_rewards);

	uint64_t total_rewards = 0;
	for(const auto& out : tx_rewards->outputs) {
		total_rewards += out.amount;
	}
	log(INFO) << "Total testnet rewards: " << total_rewards / 1000000 << " MMX";

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

	log(INFO) << "Initialized chain with hash " << block->hash;
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
	if(!fork->recv_time_ms) {
		fork->recv_time_ms = get_time_ms();
	}
	fork->prev = find_fork(block->prev);

	if(fork_tree.emplace(block->hash, fork).second) {
		fork_index.emplace(block->height, fork);
	}
}

void Node::add_transaction(std::shared_ptr<const Transaction> tx, const vnx::bool_t& pre_validate)
{
	if(!is_synced) {
		throw std::logic_error("not synced");
	}
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
	if(is_synced || sync_peak) {
		add_block(block);
	}
}

void Node::handle(std::shared_ptr<const Transaction> tx)
{
	if(is_synced) {
		add_transaction(tx);
	}
}

void Node::handle(std::shared_ptr<const ProofOfTime> value)
{
	vdf_queue.emplace_back(value, get_time_ms());
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
	proof_queue.emplace_back(value, get_time_ms());
}

void Node::handle(std::shared_ptr<const ValidatorVote> value)
{
	if(!value->is_valid()) {
		return;
	}
	vote_queue.emplace_back(value, get_time_ms());
	// verify immediately to speed up relay
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
			<< tx_pool.size() << " tx pool, " << tx_pool_fees.size() << " tx senders";
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

	timelord->stop_vdf(
		[this]() {
			log(INFO) << "Stopped TimeLord";
		});

	sync_more();
}

void Node::revert_sync(const uint32_t& height)
{
	if(height <= get_root()->height) {
		log(WARN) << "Reverting to height " << height << " ...";
		revert(height);
		reset();
	}
	start_sync(true);
}

void Node::sync_more()
{
	if(is_synced) {
		return;
	}
	const auto root = get_root();

	if(!sync_pos) {
		sync_pos = root->height + 1;
		log(INFO) << "Starting sync at height " << sync_pos;
	}
	if(sync_pos > root->height && sync_pos - root->height > params->commit_delay + max_sync_ahead) {
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

void Node::sync_result(const uint32_t& height, const std::vector<std::shared_ptr<const Block>>& result)
{
	sync_pending.erase(height);

	// filter out blocks too far into the future
	// prevent extension attack with invalid VDFs during sync
	const auto max_time_stamp = get_time_ms() + max_future_sync * params->block_interval_ms;

	std::vector<std::shared_ptr<const Block>> blocks;
	for(auto block : result) {
		if(block->time_stamp < max_time_stamp) {
			blocks.push_back(block);
		} else {
			log(WARN) << "Block at height " << block->height
					<< " is too far in the future: " << vnx::get_date_string(false, block->time_stamp);
		}
	}

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
	const auto prev_state = state_hash;
	const auto fork_line = get_fork_line(peak);

	if(fork_line.empty()) {
		return nullptr;
	}
	bool did_fork = false;
	std::shared_ptr<const BlockHeader> forked_at;

	const auto root_hash = fork_line[0]->block->prev;
	const auto alt = find_value(alt_roots, root_hash, nullptr);
	if(alt) {
		did_fork = true;
		log(WARN) << "Performing deep fork ...";
		const auto time_begin = get_time_ms();
		try {
			forked_at = alt;
			std::vector<hash_t> list;
			while(forked_at) {
				hash_t hash;
				if(height_map.find(forked_at->height, hash) && hash == forked_at->hash) {
					break;
				}
				list.push_back(forked_at->hash);
				forked_at = find_prev(forked_at);
			}
			if(!forked_at) {
				throw std::logic_error("cannot find fork point");
			}
			std::reverse(list.begin(), list.end());

			log(WARN) << "Reverting to height " << forked_at->height << " ...";

			revert(forked_at->height + 1);

			for(const auto& hash : list) {
				if(auto block = get_block(hash)) {
					try {
						apply(block, validate(block));
					}
					catch(const std::exception& ex) {
						log(WARN) << "Block validation failed for height " << block->height << " with: " << ex.what();
						throw std::logic_error("validation failed");
					}
				} else {
					throw std::logic_error("failed to read block");
				}
			}
			alt_roots.erase(root_hash);
			alt_roots[root->hash] = root;
			root = alt;

			log(INFO) << "Deep fork to new root at height " << root->height << " took "
					<< (get_time_ms() - time_begin) / 1e3 << " sec";
		}
		catch(const std::exception& ex) {
			log(WARN) << "Failed to apply alternate fork: " << ex.what();

			// restore old peak
			std::vector<std::shared_ptr<const Block>> blocks;
			hash_t hash = prev_state;
			while(auto block = get_block(hash)) {
				blocks.push_back(block);
				if(block->prev == state_hash) {
					break;
				}
				hash = block->prev;
			}
			std::reverse(blocks.begin(), blocks.end());

			for(auto block : blocks) {
				apply(block, validate(block));
			}
			reset();

			// don't try this fork again
			for(auto fork : fork_line) {
				if(auto new_fork = find_fork(fork->block->hash)) {
					new_fork->is_invalid = true;
				}
			}
			if(auto peak = get_peak()) {
				log(INFO) << "Restored old peak at height " << peak->height;
			}
			throw;
		}
	} else {
		// normal in-memory fork
		std::set<hash_t> main_set;
		for(const auto& fork : get_fork_line()) {
			main_set.insert(fork->block->hash);
		}

		forked_at = root;
		for(const auto& fork : fork_line) {
			const auto& block = fork->block;
			if(main_set.count(block->hash)) {
				forked_at = block;
			} else {
				break;
			}
		}
		if(forked_at->hash != state_hash) {
			did_fork = true;
			revert(forked_at->height + 1);
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
		if(!fork->is_validated) {
			try {
				fork->context = validate(block);
				fork->is_validated = true;
			}
			catch(const std::exception& ex) {
				log(WARN) << "Block validation failed for height " << block->height << " with: " << ex.what();
				fork->is_invalid = true;
				fork_to(prev_state);
				throw std::logic_error("validation failed");
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
			fork->root = find_prev(block);
			fork->total_votes = fork->votes;
			fork->is_all_proof_verified = fork->is_proof_verified;
		} else if(prev) {
			fork->root = prev->root;
			fork->is_invalid = prev->is_invalid || fork->is_invalid;
			fork->total_votes = prev->total_votes + fork->votes;
			fork->is_all_proof_verified = prev->is_all_proof_verified && fork->is_proof_verified;
		} else {
			fork->is_all_proof_verified = false;
		}

		if(fork->is_all_proof_verified && fork->root && !fork->is_invalid)
		{
			if(!best) {
				best = fork;
			} else {
				const auto is_deep_fork = fork->root->total_weight >  best->root->total_weight;
				const auto is_same_root = fork->root->total_weight >= best->root->total_weight;
				const auto cond_weight =  block->total_weight >= best->block->total_weight;
				const auto cond_height =  block->height > best->block->height   ? 1 : (block->height == best->block->height ? 0 : -1);
				const auto cond_votes =   fork->total_votes > best->total_votes ? 1 : (fork->total_votes == best->total_votes ? 0 : -1);

				if(cond_weight && is_deep_fork) {
					best = fork;	// higher peak and root weight (long range attack recovery)
				} else if(cond_weight || is_same_root) {
					// heavier peak or equal root
					if(cond_votes > 0) {
						best = fork;	// more total votes
					} else if(cond_votes == 0) {
						// same total votes
						if(cond_height > 0) {
							best = fork;	// longer chain (new block without votes)
						} else if(cond_height == 0) {
							// same peak height
							if(block->hash < best->block->hash) {
								best = fork;	// race condition (multiple blocks at same time, votes will make better proof win)
							}
						}
					}
				}
			}
		}
	}
	return best;
}

std::vector<std::shared_ptr<Node::fork_t>> Node::get_fork_line(std::shared_ptr<fork_t> peak) const
{
	const auto root = get_root();
	if(!peak && state_hash == root->hash) {
		return {};
	}
	std::vector<std::shared_ptr<fork_t>> line;
	auto fork = peak ? peak : find_fork(state_hash);
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

void Node::vote_for_block(std::shared_ptr<fork_t> fork)
{
	const auto& block = fork->block;
	for(const auto& entry : fork->validators)
	{
		const auto& farmer_key = entry.first;
		if(auto farmer_mac = find_value(farmer_keys, farmer_key)) {
			auto vote = ValidatorVote::create();
			vote->hash = block->hash;
			vote->farmer_key = farmer_key;
			try {
				FarmerClient farmer(*farmer_mac);
				vote->farmer_sig = farmer.sign_vote(vote);
				vote->content_hash = vote->calc_content_hash();
				publish(vote, output_votes);
				publish(vote, output_verified_votes);

				if(voted_blocks.count(block->prev)) {
					log(INFO) << "Voted again for block at height " << block->height << ": " << vote->hash;
				} else {
					log(INFO) << "Voted for block at height " << block->height << ": " << vote->hash;
				}
			} catch(const std::exception& ex) {
				log(WARN) << "Failed to sign vote for height " << block->height << ": " << ex.what();
			}
			voted_blocks[block->prev] = std::make_pair(vote->hash, get_time_ms());
		}
	}
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

	for(auto iter = fork_index.rbegin(); iter != fork_index.rend(); ++iter)
	{
		const auto& fork = iter->second;
		if(auto prev = fork->prev.lock()) {
			prev->fork_length = std::max(prev->fork_length, fork->fork_length + 1);
		}
	}
	const auto old_roots = std::move(alt_roots);
	alt_roots.clear();
	fork_tree.erase(block->hash);

	const auto range = fork_index.equal_range(height);
	for(auto iter = range.first; iter != range.second; ++iter)
	{
		const auto& fork = iter->second;
		const auto& block = fork->block;
		if(fork_tree.erase(block->hash) && fork->is_proof_verified) {
			if(fork->fork_length > 0 || old_roots.count(block->prev)) {
				write_block(block, false);
				alt_roots[block->hash] = block;
			}
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
					std::shared_ptr<const execution_context_t> context)
{
	if(block->prev != state_hash) {
		throw std::logic_error("apply(): prev != state_hash");
	}
	try {
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
			farmer_block_map.insert(std::make_pair(farmer_key, block->height), info);

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

		write_block(block);

		height_map.insert(block->height, block->hash);
		contract_cache.clear();

		state_hash = block->hash;

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
			if(exec->binary == params->plot_nft_binary
				|| exec->binary == params->offer_binary
				|| exec->binary == params->time_lock_binary) {
				owner_index = 0;
			} else if(exec->binary == params->escrow_binary) {
				owner_index = 2;
			}
			if(owner_index >= 0) {
				owner_map.insert(std::make_tuple(exec->get_arg(owner_index).to<addr_t>(), block->height, ticket), std::make_pair(tx->id, type_hash));
			}
			{
				std::set<addr_t> depends;
				for(const auto& entry : exec->depends) {
					depends.insert(entry.second);
				}
				if(depends.size()) {
					contract_depends.insert(tx->id, std::vector<addr_t>(depends.begin(), depends.end()));
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
	const auto time_begin = get_time_ms();

	const bool is_deep = !root || height <= root->height;

	for(auto block = get_peak(); !is_deep && block && block->height >= height; block = find_prev(block))
	{
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
		// add removed tx back to pool
		if(is_synced) {
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
		}
	}

	db->revert(height);

	uint32_t peak = 0;
	if(!height_map.find_last(peak, state_hash)) {
		state_hash = hash_t();
	}
	contract_cache.clear();

	if(is_deep || farmer_ranking.empty())
	{
		// reset farmer ranking
		std::map<pubkey_t, uint32_t> farmer_block_count;
		farmer_block_map.scan([&farmer_block_count](const std::pair<pubkey_t, uint32_t>& key, const farmed_block_info_t& info) -> bool {
			farmer_block_count[key.first]++;
			return true;
		});
		farmer_ranking.clear();
		for(const auto& entry : farmer_block_count) {
			farmer_ranking.push_back(entry);
		}
	}
	update_farmer_ranking();

	const auto elapsed = (get_time_ms() - time_begin) / 1e3;
	if(elapsed > 1) {
		log(WARN) << "Reverting to height " << peak << " took " << elapsed << " sec";
	}
}

void Node::reset()
{
	root = nullptr;
	alt_roots.clear();
	fork_tree.clear();
	fork_index.clear();
	history.clear();
	history_log.clear();
	contract_cache.clear();

	uint32_t height = 0;
	if(height_map.find_last(height, state_hash))
	{
		blocks->open("rb+");

		// check consistency
		while(true) {
			if(auto block = get_block_at(height)) {
				if(block->is_valid() && block->height == height) {
					break;
				} else {
					log(WARN) << "Corrupted block at height " << height << ", reverting ...";
				}
			} else {
				log(WARN) << "Missing block at height " << height << ", reverting ...";
			}
			if(height) {
				revert(height--);
			} else {
				break;
			}
		}

		// get root
		root = get_header_at(height - std::min(params->commit_delay, height));
		if(!root) {
			throw std::logic_error("failed to load root block");
		}

		// load fork tree
		std::vector<hash_t> list;
		height_index.find_range(root->height, height + 1, list);
		for(const auto& hash : list) {
			if(auto block = get_block(hash)) {
				if(block->height == root->height) {
					if(block->hash != root->hash) {
						alt_roots[hash] = block;
					}
				} else {
					auto fork = std::make_shared<fork_t>();
					fork->block = block;
					fork->is_vdf_verified = true;
					add_fork(fork);
				}
			}
		}

		// load history
		for(auto block = root; block && history.size() < max_history; block = find_prev(block)) {
			history[block->hash] = block;
			history_log.emplace(block->height, block->hash);
		}

		// update address count
		mmx_address_count = 0;
		balance_table.scan([this](const std::pair<addr_t, addr_t>& key, const uint128& value) -> bool {
			if(value >= 1000000 && key.second == addr_t()) {
				mmx_address_count++;
			}
			return true;
		});
	}
	else {
		blocks->open("wb");
		blocks->open("rb+");
		db_blocks->revert(0);
		init_chain();
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

std::shared_ptr<const BlockHeader> Node::find_prev(	std::shared_ptr<const BlockHeader> block,
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
			if(auto block = find_prev(iter)) {
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
		block = find_prev(block);
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
	if(auto prev = find_prev(infused, params->commit_delay, true)) {
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
			block = find_prev(block);
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
	const auto prev = find_prev(block);
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
	block = find_prev(block, params->commit_delay);

	std::set<pubkey_t> set;
	for(uint32_t k = 0; block && k < max_history && set.size() < params->max_validators; ++k)
	{
		if(block->proof.size()) {
			set.insert(block->proof[0]->farmer_key);
		}
		block = find_prev(block);
	}
	return set;
}

std::vector<addr_t> Node::get_all_depends(std::shared_ptr<const contract::Executable> exec) const
{
	std::set<addr_t> out;
	for(const auto& entry : exec->depends) {
		out.insert(entry.second);
	}
	if(out.size() > params->max_rcall_width) {
		throw std::logic_error("remote call width overflow");
	}
	for(const auto& address : std::vector<addr_t>(out.begin(), out.end())) {
		const auto tmp = get_all_depends(address, 2);
		out.insert(tmp.begin(), tmp.end());
	}
	if(out.size() > params->max_rcall_width) {
		throw std::logic_error("remote call width overflow");
	}
	return std::vector<addr_t>(out.begin(), out.end());
}

std::vector<addr_t> Node::get_all_depends(const addr_t& address, const uint32_t depth) const
{
	std::set<addr_t> out;
	std::vector<addr_t> list;
	if(contract_depends.find(address, list)) {
		for(const auto& address : list) {
			const auto tmp = get_all_depends(address, depth + 1);
			out.insert(tmp.begin(), tmp.end());
		}
		out.insert(list.begin(), list.end());
	}
	if(out.size() && depth > params->max_rcall_depth) {
		throw std::logic_error("remote call depth overflow");
	}
	if(out.size() > params->max_rcall_width) {
		throw std::logic_error("remote call width overflow");
	}
	return std::vector<addr_t>(out.begin(), out.end());
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
	if(auto prev = find_prev(block)) {
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
		if(auto prev = find_prev(block)) {
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
		vnx::File& file, bool full_block, std::vector<int64_t>* tx_offsets) const
{
	// THREAD SAFE (for concurrent reads)
	auto& in = file.in;
	if(tx_offsets) {
		tx_offsets->clear();
	}
	try {
		if(auto header = std::dynamic_pointer_cast<const BlockHeader>(vnx::read(in))) {
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
	return nullptr;
}

void Node::write_block(std::shared_ptr<const Block> block, const bool is_main)
{
	try {
		block_index_t index;
		if(block_index.find(block->hash, index))
		{
			blocks->seek_to(index.file_offset);

			std::vector<int64_t> tx_offsets;
			if(auto block = read_block(*blocks, true, &tx_offsets)) {
				if(!block->is_valid()) {
					throw std::logic_error("invalid block");
				}
			} else {
				throw std::logic_error("failed to read block");
			}
			if(tx_offsets.size() != block->tx_count) {
				throw std::logic_error("tx count mismatch");
			}
			if(is_main) {
				for(uint32_t i = 0; i < block->tx_count; ++i) {
					const auto& tx = block->tx_list[i];
					tx_index.insert(tx->id, tx->get_tx_index(params, block, tx_offsets[i]));
				}
			}
			return;
		}
	} catch(const std::exception& ex) {
		log(WARN) << "Stored block at height " << block->height << " is corrupted: " << ex.what();
	}
	blocks->seek_end();
	auto& out = blocks->out;
	const auto offset = out.get_output_pos();

	vnx::write(out, block->get_header());

	std::vector<std::pair<hash_t, tx_index_t>> tx_list;
	for(const auto& tx : block->tx_list) {
		if(is_main) {
			tx_list.emplace_back(tx->id, tx->get_tx_index(params, block, out.get_output_pos()));
		}
		vnx::write(out, tx);
	}
	for(const auto& entry : tx_list) {
		tx_index.insert(entry.first, entry.second);
	}
	vnx::write(out, nullptr);	// end of block
	blocks->flush();

	block_index.insert(block->hash, block->get_block_index(offset));
	{
		std::vector<hash_t> list;
		height_index.find(block->height, list);
		if(std::find(list.begin(), list.end(), block->hash) == list.end()) {
			height_index.insert(block->height, block->hash);
		}
	}
	db_blocks->commit(db_blocks->version() + 1);
}


} // mmx
