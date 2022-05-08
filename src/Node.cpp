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
#include <mmx/contract/PubKey.hxx>
#include <mmx/operation/Revoke.hxx>
#include <mmx/operation/Mutate.hxx>
#include <mmx/operation/Deposit.hxx>
#include <mmx/utils.h>

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
	if(opencl_device >= 0) {
		const auto devices = automy::basic_opencl::get_devices();
		if(size_t(opencl_device) < devices.size()) {
			for(int i = 0; i < 2; ++i) {
				opencl_vdf[i] = std::make_shared<OCL_VDF>(opencl_device);
			}
			log(INFO) << "Using OpenCL GPU device " << opencl_device << " (total of " << devices.size() << " found)";
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
	{
		::rocksdb::Options options;
		options.max_open_files = 8;
		options.keep_log_file_num = 3;
		options.max_manifest_file_size = 64 * 1024 * 1024;
		options.OptimizeForSmallDb();

		addr_log.open(database_path + "addr_log", options);
		recv_log.open(database_path + "recv_log", options);
		spend_log.open(database_path + "spend_log", options);
		revoke_log.open(database_path + "revoke_log", options);
		revoke_map.open(database_path + "revoke_map", options);

		contract_cache.open(database_path + "contract_cache", options);
		mutate_log.open(database_path + "mutate_log", options);
		owner_map.open(database_path + "owner_map", options);

		tx_log.open(database_path + "tx_log", options);
		tx_index.open(database_path + "tx_index", options);
		hash_index.open(database_path + "hash_index", options);
		block_index.open(database_path + "block_index", options);
		balance_table.open(database_path + "balance_table", options);
	}
	block_chain = std::make_shared<vnx::File>(storage_path + "block_chain.dat");

	bool is_replay = true;
	if(block_chain->exists()) {
		const auto time_begin = vnx::get_time_millis();
		block_chain->open("rb+");
		uint32_t height = 0;
		std::pair<int64_t, hash_t> entry;
		while(block_index.find_last(height, entry)) {
			is_replay = false;
			block_chain->seek_to(entry.first);
			try {
				int64_t offset = 0;
				auto block = read_block(*block_chain, true, &offset);
				if(height <= replay_height) {
					state_hash = block->hash;
					revert(height + 1, nullptr);
					break;
				}
			} catch(const std::exception& ex) {
				log(WARN) << "Failed to read block " << height << ": " << ex.what();
			}
			revert(height, nullptr);
		}
		if(is_replay) {
			log(INFO) << "Creating DB (this may take a while) ...";
			int64_t offset = 0;
			while(auto header = read_block(*block_chain, true, &offset)) {
				if(auto block = std::dynamic_pointer_cast<const Block>(header)) {
					apply(block, &offset);
					commit(block);
					if(block->height % 1000 == 999) {
						log(INFO) << "Height " << block->height << " ...";
					}
				}
			}
			if(auto peak = get_peak()) {
				log(INFO) << "Loaded " << peak->height + 1 << " blocks from disk, took " << (vnx::get_wall_time_millis() - time_begin) / 1e3 << " sec";
			}
		} else {
			for(uint32_t i = 0; i < max_history; ++i) {
				if(i <= height) {
					if(auto header = get_header_at(height - i)) {
						history[header->height] = header;
					}
				}
			}
			balance_table.scan([this, height](const std::pair<addr_t, addr_t>& key, const std::pair<uint128, uint32_t>& value) {
				balance_map.emplace(key, value.first);
			});
			log(INFO) << "Loaded height " << height << ", took " << (vnx::get_time_millis() - time_begin) / 1e3 << " sec";
		}
	} else {
		block_chain->open("wb");
		block_chain->open("rb+");
	}
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

	subscribe(input_vdfs, max_queue_ms);
	subscribe(input_proof, max_queue_ms);
	subscribe(input_blocks, max_queue_ms);
	subscribe(input_transactions, max_queue_ms);
	subscribe(input_timelord_vdfs, max_queue_ms);
	subscribe(input_harvester_proof, max_queue_ms);

	set_timer_millis(60 * 1000, std::bind(&Node::print_stats, this));
	set_timer_millis(validate_interval_ms, std::bind(&Node::validate_pool, this));

	update_timer = set_timer_millis(update_interval_ms, std::bind(&Node::update, this));
	stuck_timer = set_timer_millis(sync_loss_delay * 1000, std::bind(&Node::on_stuck_timeout, this));

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
			info->is_synced = is_synced;
			info->height = peak->height;
			info->time_diff = peak->time_diff;
			info->space_diff = peak->space_diff;
			info->block_reward = mmx::calc_block_reward(params, peak->space_diff);
			info->total_space = calc_total_netspace(params, peak->space_diff);
			info->address_count = balance_map.size();
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
	return std::dynamic_pointer_cast<const Block>(get_block_ex(hash, true));
}

std::shared_ptr<const BlockHeader> Node::get_block_ex(const hash_t& hash, bool full_block) const
{
	auto iter = fork_tree.find(hash);
	if(iter != fork_tree.end()) {
		auto block = iter->second->block;
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
	if(auto tx = get_transaction(id, true))
	{
		if(tx->parent) {
			tx = tx->get_combined();
		}
		tx_info_t info;
		info.id = id;
		if(auto height = get_tx_height(id)) {
			info.height = *height;
			info.block = get_block_hash(*height);
		}
		info.note = tx->note;
		info.cost = tx->calc_cost(params);
		info.inputs = tx->inputs;
		info.outputs = tx->get_outputs();
		info.operations = tx->execute;
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
		auto iter = info.input_amounts.find(addr_t());
		auto iter2 = info.output_amounts.find(addr_t());
		info.fee = int64_t(iter != info.input_amounts.end() ? iter->second.lower() : 0)
						- (iter2 != info.output_amounts.end() ? iter2->second.lower() : 0);
		return info;
	}
	return nullptr;
}

std::shared_ptr<const Transaction> Node::get_transaction(const hash_t& id, const bool& include_pending) const
{
	// THREAD SAFE (for concurrent reads)
	if(include_pending) {
		auto iter = tx_pool.find(id);
		if(iter != tx_pool.end()) {
			return iter->second.tx;
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
		uint128_t recv = uint128_0;
		uint128_t spent = uint128_0;
	};
	std::map<std::tuple<addr_t, hash_t, addr_t>, entry_t> delta_map;

	for(const auto& address : std::unordered_set<addr_t>(addresses.begin(), addresses.end())) {
		{
			std::vector<txout_entry_t> entries;
			recv_log.find_range(std::make_pair(address, min_height), std::make_pair(address, -1), entries);
			for(const auto& entry : entries) {
				auto& delta = delta_map[std::make_tuple(entry.address, entry.key.txid, entry.contract)];
				delta.height = entry.height;
				delta.recv += entry.amount;
			}
		}
		{
			std::vector<txio_entry_t> entries;
			spend_log.find_range(std::make_pair(address, min_height), std::make_pair(address, -1), entries);
			for(const auto& entry : entries) {
				auto& delta = delta_map[std::make_tuple(entry.address, entry.key.txid, entry.contract)];
				delta.height = entry.height;
				delta.spent += entry.amount;
			}
		}
	}
	std::multimap<uint32_t, tx_entry_t> list;
	for(const auto& entry : delta_map) {
		const auto& delta = entry.second;
		tx_entry_t out;
		out.height = delta.height;
		out.txid = std::get<1>(entry.first);
		out.address = std::get<0>(entry.first);
		out.contract = std::get<2>(entry.first);
		if(delta.recv > delta.spent) {
			tx_entry_t out;
			out.type = tx_type_e::RECEIVE;
			out.amount = delta.recv - delta.spent;
		}
		if(delta.recv < delta.spent) {
			out.type = tx_type_e::SPEND;
			out.amount = delta.spent - delta.recv;
		}
		if(out.amount) {
			list.emplace(out.height, out);
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
	std::shared_ptr<const Contract> contract;
	if(!contract_cache.find(address, contract)) {
		if(auto tx = get_transaction(address)) {
			contract = tx->deploy;
		}
	}
	return contract;
}

std::shared_ptr<const Contract> Node::get_contract_for(const addr_t& address) const
{
	std::shared_ptr<const Contract> out;
	if(auto contract = get_contract(address)) {
		out = contract;
	} else {
		auto pubkey = contract::PubKey::create();
		pubkey->address = address;
		out = pubkey;
	}
	return out;
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
	return res;
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
	auto fork = std::make_shared<fork_t>();
	fork->recv_time = vnx::get_wall_time_micros();
	fork->block = block;
	add_fork(fork);
}

void Node::add_transaction(std::shared_ptr<const Transaction> tx, const vnx::bool_t& pre_validate)
{
	if(!tx->is_valid()) {
		throw std::logic_error("invalid tx");
	}
	if(tx_pool.count(tx->id)) {
		return;
	}
	if(pre_validate) {
		validate(tx);
	}
	tx_pool[tx->id].tx = tx;

	if(!vnx_sample) {
		publish(tx, output_transactions);
	}
}

uint128 Node::get_balance(const addr_t& address, const addr_t& currency, const uint32_t& min_confirm) const
{
	return get_total_balance({address}, currency, min_confirm);
}

uint128 Node::get_total_balance(const std::vector<addr_t>& addresses, const addr_t& currency, const uint32_t& min_confirm) const
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

std::map<addr_t, uint128> Node::get_balances(const addr_t& address, const uint32_t& min_confirm) const
{
	return get_total_balances({address}, min_confirm);
}

std::map<addr_t, uint128> Node::get_total_balances(const std::vector<addr_t>& addresses, const uint32_t& min_confirm) const
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

std::map<std::pair<addr_t, addr_t>, uint128> Node::get_all_balances(const std::vector<addr_t>& addresses, const uint32_t& min_confirm) const
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
				info.num_spend++;
				info.total_spend[entry.contract] += entry.amount;
				info.last_spend_height = std::max(info.last_spend_height, entry.height);
		}
	}
	return info;
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

bool Node::recv_height(const uint32_t& height)
{
	if(auto root = get_root()) {
		if(height < root->height) {
			return false;
		}
	}
	if(auto peak = get_peak()) {
		if(height > peak->height && height - peak->height > 2 * params->commit_delay) {
			return false;
		}
	}
	return true;
}

void Node::handle(std::shared_ptr<const Block> block)
{
	if(!block->proof) {
		return;
	}
	if(!recv_height(block->height)) {
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
	if(!is_synced || !value->is_valid()) {
		return;
	}
	const auto root = get_root();
	const auto request = value->request;
	if(request->height < root->height) {
		return;
	}
	try {
		// TODO: race condition with receive of block
		const auto vdf_block = get_header(request->vdf_block);
		if(!vdf_block) {
			throw std::logic_error("no such vdf_block");
		}
		if(request->height != vdf_block->height + params->challenge_delay) {
			throw std::logic_error("invalid height");
		}
		const auto diff_block = find_diff_header(vdf_block, params->challenge_delay);
		if(!diff_block) {
			throw std::logic_error("missing difficulty block");
		}
		const auto challenge = hash_t(diff_block->hash + vdf_block->vdf_output[1]);
		if(request->challenge != challenge) {
			throw std::logic_error("invalid challenge");
		}
		if(request->space_diff != diff_block->space_diff) {
			throw std::logic_error("invalid space_diff");
		}
		verify_proof(value->proof, challenge, diff_block);

		if(value->proof->score >= params->score_threshold) {
			throw std::logic_error("invalid score");
		}
		auto iter = proof_map.find(challenge);
		if(iter == proof_map.end() || value->proof->score <= iter->second->proof->score)
		{
			if(iter == proof_map.end()) {
				challenge_map.emplace(request->height, challenge);
			}
			else if(value->proof->score < iter->second->proof->score) {
				proof_map.erase(challenge);
			}
			proof_map.emplace(challenge, value);

			log(DEBUG) << "Got new best proof for height " << request->height << " with score " << value->proof->score;
		}
		publish(value, output_verified_proof);
	}
	catch(const std::exception& ex) {
		log(WARN) << "Got invalid proof: " << ex.what();
	}
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
	log(INFO) << tx_pool.size() << " tx pool, " << balance_map.size() << " addrs, " << fork_tree.size() << " blocks";
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
	const size_t max_pending = !sync_retry ? std::max(std::min<int>(max_sync_pending, max_sync_jobs), 2) : 1;

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
			total_cost += block->calc_cost(params);
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
		if(!sync_retry && height % 64 == 0) {
			add_task(std::bind(&Node::update, this));
		}
		sync_more();
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
				validate(block);

				if(!fork->is_vdf_verified) {
					if(auto prev = find_prev_header(block)) {
						if(auto infuse = find_prev_header(block, params->infuse_delay + 1, true)) {
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
					publish(point->proof, output_verified_vdfs);
				}
				publish(block, output_verified_blocks);
			}
		}
		apply(block);
	}
	return did_fork ? forked_at : nullptr;
}

std::shared_ptr<Node::fork_t> Node::find_best_fork() const
{
	uint128_t max_weight = 0;
	std::shared_ptr<fork_t> best_fork;
	const auto root = get_root();
	const auto begin = fork_index.upper_bound(root->height);
	for(auto iter = begin; iter != fork_index.end(); ++iter)
	{
		const auto& fork = iter->second;
		const auto& block = fork->block;
		const auto prev = fork->prev.lock();
		if(prev && prev->is_invalid) {
			fork->is_invalid = true;
		}
		if(!fork->is_proof_verified || fork->is_invalid) {
			continue;
		}
		if(!best_fork
			|| block->total_weight > max_weight
			|| (block->total_weight == max_weight && block->hash < best_fork->block->hash))
		{
			best_fork = fork;
			max_weight = block->total_weight;
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

std::unordered_set<addr_t> Node::get_revokations(const hash_t& txid) const
{
	std::unordered_set<addr_t> addrs;
	std::vector<std::pair<addr_t, hash_t>> entries;
	revoke_map.find_range(std::make_pair(txid, 0), std::make_pair(txid, -1), entries);
	for(const auto& entry : entries) {
		addrs.insert(entry.first);
	}
	return addrs;
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

void Node::commit(std::shared_ptr<const Block> block) noexcept
{
	if(!history.empty() && block->prev != get_root()->hash) {
		return;
	}
	history[block->height] = block->get_header();
	{
		const auto range = challenge_map.equal_range(block->height);
		for(auto iter = range.first; iter != range.second; ++iter) {
			proof_map.erase(iter->second);
		}
		challenge_map.erase(range.first, range.second);
	}
	while(history.size() > max_history) {
		history.erase(history.begin());
	}
	fork_tree.erase(block->hash);
	pending_vdfs.erase(pending_vdfs.begin(), pending_vdfs.upper_bound(block->height));
	verified_vdfs.erase(verified_vdfs.begin(), verified_vdfs.upper_bound(block->height));

	if(is_synced) {
		Node::log(INFO)
				<< "Committed height " << block->height << " with: ntx = " << block->tx_list.size()
				<< ", score = " << (block->proof ? block->proof->score : params->score_threshold)
				<< ", tdiff = " << block->time_diff << ", sdiff = " << block->space_diff;
	}
	publish(block, output_committed_blocks, is_synced ? 0 : BLOCKING);
}

void Node::apply(std::shared_ptr<const Block> block, int64_t* file_offset) noexcept
{
	if(block->prev != state_hash) {
		return;
	}
	std::unordered_set<addr_t> addr_set;
	std::unordered_set<hash_t> revoke_set;
	std::set<std::pair<addr_t, addr_t>> balance_set;

	for(const auto& tx : block->get_all_transactions()) {
		apply(block, tx, addr_set, revoke_set, balance_set);
	}
	addr_log.insert(block->height, std::vector<addr_t>(addr_set.begin(), addr_set.end()));
	revoke_log.insert(block->height, std::vector<hash_t>(revoke_set.begin(), revoke_set.end()));

	for(const auto& key : balance_set) {
		balance_table.insert(key, std::make_pair(balance_map[key], block->height));
	}
	write_block(block, file_offset);
	state_hash = block->hash;
}

void Node::apply(	std::shared_ptr<const Block> block,
					std::shared_ptr<const Transaction> tx,
					std::unordered_set<addr_t>& addr_set,
					std::unordered_set<hash_t>& revoke_set,
					std::set<std::pair<addr_t, addr_t>>& balance_set) noexcept
{
	if(tx->parent) {
		apply(block, tx->parent, addr_set, revoke_set, balance_set);
	}
	const auto outputs = tx->get_outputs();
	for(size_t i = 0; i < outputs.size(); ++i)
	{
		const auto& out = outputs[i];
		addr_set.insert(out.address);
		recv_log.insert(std::make_pair(out.address, block->height),
				txout_entry_t::create_ex(txio_key_t::create_ex(tx->id, i), block->height, out));

		const auto key = std::make_pair(out.address, out.contract);
		balance_set.insert(key);
		balance_map[key] += out.amount;
	}
	for(size_t i = 0; i < tx->inputs.size(); ++i)
	{
		const auto& in = tx->inputs[i];
		addr_set.insert(in.address);
		spend_log.insert(std::make_pair(in.address, block->height),
				txio_entry_t::create_ex(txio_key_t::create_ex(tx->id, i), block->height, in));

		const auto key = std::make_pair(in.address, in.contract);
		balance_set.insert(key);
		balance_map[key] -= in.amount;
	}
	for(const auto& op : tx->execute)
	{
		if(auto revoke = std::dynamic_pointer_cast<const operation::Revoke>(op))
		{
			revoke_set.insert(revoke->txid);
			revoke_map.insert(std::make_pair(revoke->txid, block->height), std::make_pair(revoke->address, tx->id));
		}
		if(auto mutate = std::dynamic_pointer_cast<const operation::Mutate>(op))
		{
			std::shared_ptr<const Contract> contract;
			if(!contract_cache.find(op->address, contract)) {
				if(auto tx = get_transaction(op->address)) {
					contract = tx->deploy;
				}
			}
			if(contract) {
				try {
					auto copy = vnx::clone(contract);
					copy->vnx_call(vnx::clone(mutate->method));

					addr_set.insert(op->address);
					mutate_log.insert(std::make_pair(op->address, block->height), mutate->method);
					contract_cache.insert(op->address, copy);
					// TODO: handle owner change
				}
				catch(const std::exception& ex) {
					Node::log(ERROR) << "apply(): mutate " << op->address << " failed with: " << ex.what();
				}
			}
		}
	}
	if(auto contract = tx->deploy) {
		if(auto owner = contract->get_owner()) {
			owner_map.insert(*owner, tx->id);
		}
	}
	tx_pool.erase(tx->id);
}

bool Node::revert() noexcept
{
	if(state_hash == get_root()->hash) {
		return false;
	}
	if(auto block = get_block(state_hash)) {
		revert(block->height, block);
		return true;
	}
	return false;
}

void Node::revert(const uint32_t height, std::shared_ptr<const Block> block) noexcept
{
	std::vector<std::vector<addr_t>> addr_list;
	addr_log.find_greater_equal(height, addr_list);

	std::unordered_set<addr_t> addr_set;
	for(const auto& list : addr_list) {
		addr_set.insert(list.begin(), list.end());
	}

	std::set<std::pair<addr_t, addr_t>> balance_set;
	for(const auto& address : addr_set) {
		const auto log_key = std::make_pair(address, height);
		{
			std::vector<txio_entry_t> entries;
			spend_log.find(log_key, entries, vnx::rocksdb::GREATER_EQUAL);
			for(const auto& entry : entries) {
				const auto key = std::make_pair(entry.address, entry.contract);
				balance_set.insert(key);
				balance_map[key] += entry.amount;
			}
			spend_log.erase_all(log_key, vnx::rocksdb::GREATER_EQUAL);
		}
		{
			std::vector<txout_entry_t> entries;
			recv_log.find(log_key, entries);
			for(const auto& entry : entries) {
				const auto key = std::make_pair(entry.address, entry.contract);
				balance_set.insert(key);
				balance_map[key] -= entry.amount;
			}
			recv_log.erase_all(log_key, vnx::rocksdb::GREATER_EQUAL);
		}
	}
	{
		std::set<addr_t> affected;
		for(const auto& address : addr_set) {
			if(mutate_log.erase_range(std::make_pair(address, height), std::make_pair(address, -1))) {
				affected.insert(address);
			}
		}
		for(const auto& address : affected) {
			if(auto tx = get_transaction(address)) {
				if(auto contract = tx->deploy) {
					auto copy = vnx::clone(contract);
					std::vector<vnx::Object> mutations;
					mutate_log.find_range(std::make_pair(address, 0), std::make_pair(address, height), mutations);
					for(const auto& method : mutations) {
						copy->vnx_call(vnx::clone(method));
					}
					if(mutations.empty()) {
						contract_cache.erase(address);
					} else {
						contract_cache.insert(address, copy);
					}
					// TODO: handle owner change
				}
			}
		}
		if(affected.size()) {
			log(INFO) << "Reverted " << affected.size() << " contracts";
		}
	}
	addr_log.erase_greater_equal(height);

	for(const auto& key : balance_set) {
		balance_table.insert(key, std::make_pair(balance_map[key], height - 1));
	}
	{
		std::vector<std::vector<hash_t>> all_keys;
		tx_log.find_greater_equal(height, all_keys);
		for(const auto& keys : all_keys) {
			tx_index.erase_many(keys);
		}
		tx_log.erase_greater_equal(height);
	}
	{
		std::vector<std::vector<hash_t>> all_keys;
		revoke_log.find_greater_equal(height, all_keys);
		for(const auto& keys : all_keys) {
			for(const auto& txid : keys) {
				revoke_map.erase_all(std::make_pair(txid, height));
			}
		}
		revoke_log.erase_greater_equal(height);
	}
	{
		std::pair<int64_t, hash_t> entry;
		if(block_index.find(height, entry)) {
			hash_index.erase(entry.second);
			block_chain->seek_to(entry.first);
		}
	}
	block_index.erase(height);

	if(block) {
		for(const auto& tx : block->tx_list) {
			auto& entry = tx_pool[tx->id];
			entry.did_validate = true;
			entry.tx = tx;
		}
		state_hash = block->prev;
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

std::vector<std::shared_ptr<const ProofResponse>> Node::find_proof(const hash_t& challenge) const
{
	std::vector<std::shared_ptr<const ProofResponse>> res;
	const auto range = proof_map.equal_range(challenge);
	for(auto iter = range.first; iter != range.second; ++iter) {
		res.push_back(iter->second);
	}
	return res;
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

std::shared_ptr<const BlockHeader> Node::read_block(vnx::File& file, bool full_block, int64_t* file_offset) const
{
	// THREAD SAFE (for concurrent reads)
	auto& in = file.in;
	const auto offset = in.get_input_pos();
	if(file_offset) {
		*file_offset = offset;
	}
	try {
		if(auto header = std::dynamic_pointer_cast<BlockHeader>(vnx::read(in))) {
			if(full_block) {
				auto block = Block::create();
				block->BlockHeader::operator=(*header);
				while(true) {
					if(auto value = vnx::read(in)) {
						if(auto tx = std::dynamic_pointer_cast<Transaction>(value)) {
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

void Node::write_block(std::shared_ptr<const Block> block, int64_t* file_offset)
{
	auto& out = block_chain->out;
	const auto offset = file_offset ? *file_offset : out.get_output_pos();

	std::vector<hash_t> tx_ids;
	if(auto tx = block->tx_base) {
		tx_ids.push_back(tx->id);
		tx_index.insert(tx->id, std::make_pair(offset, block->height));
	}
	if(!file_offset) {
		vnx::write(out, block->get_header());
	}

	for(auto tx : block->tx_list) {
		const auto offset = out.get_output_pos();
		while(tx) {
			tx_ids.push_back(tx->id);
			tx_index.insert(tx->id, std::make_pair(offset, block->height));
			tx = tx->parent;
		}
		if(!file_offset) {
			vnx::write(out, tx);
		}
	}
	tx_log.insert(block->height, tx_ids);

	if(!file_offset) {
		vnx::write(out, nullptr);
		block_chain->flush();
	}
	block_index.insert(block->height, std::make_pair(offset, block->hash));
	hash_index.insert(block->hash, block->height);
}


} // mmx
