/*
 * Node.cpp
 *
 *  Created on: Dec 7, 2021
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/Context.hxx>
#include <mmx/Challenge.hxx>
#include <mmx/FarmerClient.hxx>
#include <mmx/TimeInfusion.hxx>
#include <mmx/IntervalRequest.hxx>
#include <mmx/TimeLordClient.hxx>
#include <mmx/contract/NFT.hxx>
#include <mmx/contract/PubKey.hxx>
#include <mmx/operation/Spend.hxx>
#include <mmx/utxo_entry_t.hpp>
#include <mmx/stxo_entry_t.hpp>
#include <mmx/chiapos.h>
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

	vnx::Directory(storage_path + "db").create();
	{
		::rocksdb::Options options;
		options.max_open_files = 16;
		options.keep_log_file_num = 3;
		options.max_manifest_file_size = 64 * 1024 * 1024;

		stxo_index.open(storage_path + "db/stxo_index", options);
		saddr_map.open(storage_path + "db/saddr_map", options);
		stxo_log.open(storage_path + "db/stxo_log", options);
		tx_index.open(storage_path + "db/tx_index", options);
		tx_log.open(storage_path + "db/tx_log", options);
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
				stxo_index.flush();
				log(INFO) << "Purged " << keys.size() << " STXO entries";
			}
			stxo_log.erase_all(next_height, vnx::rocksdb::GREATER_EQUAL);
			stxo_log.flush();
		}
		{
			std::vector<hash_t> keys;
			if(tx_log.find(next_height, keys, vnx::rocksdb::GREATER_EQUAL)) {
				for(const auto& key : keys) {
					tx_index.erase(key);
				}
				tx_index.flush();
				log(INFO) << "Purged " << keys.size() << " TX entries";
			}
			tx_log.erase_all(next_height, vnx::rocksdb::GREATER_EQUAL);
			tx_log.flush();
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

std::shared_ptr<const ChainParams> Node::get_params() const
{
	return params;
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
	std::vector<hash_t> list;
	if(auto block = get_block_at(height)) {
		for(const auto& tx : block->tx_list) {
			list.push_back(tx->id);
		}
	}
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
	// THREAD SAFE
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
		std::vector<stxo_entry_t> inputs;
	};
	std::unordered_map<hash_t, txio_t> txio_map;

	for(const auto& entry : get_utxo_list(addresses)) {
		if(entry.output.height >= min_height) {
			txio_map[entry.key.txid].outputs.push_back(entry.output);
		}
	}
	for(const auto& entry : get_stxo_list(addresses)) {
		if(entry.output.height >= min_height) {
			txio_map[entry.key.txid].outputs.push_back(entry.output);
		}
		txio_map[entry.spent.txid].inputs.push_back(entry);
	}
	std::multimap<uint32_t, tx_entry_t> list;

	for(const auto& iter : txio_map) {
		const auto& txio = iter.second;

		std::unordered_map<hash_t, int64_t> amount;
		for(const auto& utxo : txio.outputs) {
			amount[utxo.contract] += utxo.amount;
		}
		for(const auto& entry : txio.inputs) {
			const auto& utxo = entry.output;
			amount[utxo.contract] -= utxo.amount;
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
		if(!txio.inputs.empty()) {
			if(auto height = get_tx_height(iter.first)) {
				if(*height >= min_height) {
					if(auto tx = get_transaction(iter.first)) {
						for(const auto& utxo : tx->outputs) {
							if(amount[utxo.contract] < 0 && !addr_set.count(utxo.address)) {
								tx_entry_t entry;
								entry.height = *height;
								entry.txid = iter.first;
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
	if(auto tx = get_transaction(address)) {
		return tx->deploy;
	}
	return nullptr;
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
		for(const auto& addr : contract->get_parties()) {
			if(light_address_set.count(addr)) {
				return true;
			}
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

void Node::add_transaction(std::shared_ptr<const Transaction> tx)
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
	tx_pool[tx->id] = tx;

	if(!vnx_sample) {
		publish(tx, output_transactions);
	}
}

uint64_t Node::get_balance(const addr_t& address, const addr_t& contract) const
{
	return get_total_balance({address}, contract);
}

uint64_t Node::get_total_balance(const std::vector<addr_t>& addresses, const addr_t& contract) const
{
	uint64_t total = 0;
	for(const auto& entry : get_utxo_list(addresses)) {
		const auto& utxo = entry.output;
		if(utxo.contract == contract) {
			total += utxo.amount;
		}
	}
	return total;
}

std::map<addr_t, uint64_t> Node::get_total_balances(const std::vector<addr_t>& addresses) const
{
	std::map<addr_t, uint64_t> amounts;
	for(const auto& entry : get_utxo_list(addresses)) {
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

std::vector<utxo_entry_t> Node::get_utxo_list(const std::vector<addr_t>& addresses) const
{
	std::vector<utxo_entry_t> res;
	for(const auto& addr : addresses) {
		const auto begin = addr_map.lower_bound(std::make_pair(addr, txio_key_t()));
		const auto end   = addr_map.upper_bound(std::make_pair(addr, txio_key_t::create_ex(hash_t::ones(), -1)));
		for(auto iter = begin; iter != end; ++iter) {
			auto iter2 = utxo_map.find(iter->second);
			if(iter2 != utxo_map.end()) {
				res.push_back(utxo_entry_t::create_ex(iter2->first, iter2->second));
			}
		}
		auto iter = taddr_map.find(addr);
		if(iter != taddr_map.end()) {
			for(const auto& key : iter->second) {
				auto iter2 = utxo_map.find(key);
				if(iter2 != utxo_map.end()) {
					res.push_back(utxo_entry_t::create_ex(iter2->first, iter2->second));
				}
			}
		}
	}
	return res;
}

std::vector<stxo_entry_t> Node::get_stxo_list(const std::vector<addr_t>& addresses) const
{
	std::vector<stxo_entry_t> res;
	for(const auto& addr : addresses) {
		std::vector<txio_key_t> keys;
		saddr_map.find(addr, keys);
		for(const auto& key : std::unordered_set<txio_key_t>(keys.begin(), keys.end())) {
			stxo_t stxo;
			if(stxo_index.find(key, stxo)) {
				res.push_back(stxo_entry_t::create_ex(key, stxo));
			}
		}
	}
	const std::unordered_set<addr_t> addr_set(addresses.begin(), addresses.end());
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
	if(!is_synced && !sync_retry) {
		return;
	}
	if(block->proof) {
		add_block(block);
	}
}

void Node::handle(std::shared_ptr<const Transaction> tx)
{
	if(!is_synced && !sync_retry) {
		return;
	}
	add_transaction(tx);
}

void Node::handle(std::shared_ptr<const ProofOfTime> proof)
{
	if(!is_synced && !sync_retry) {
		return;
	}
	if(find_vdf_point(	proof->height, proof->start, proof->get_vdf_iters(),
						proof->input, {proof->get_output(0), proof->get_output(1)}))
	{
		return;		// already verified
	}
	const auto peak = get_peak();
	if(!peak || proof->height < peak->height) {
		return;
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
		log(WARN) << "VDF verification for height " << proof->height << " failed with: " << ex.what();
	}
}

void Node::handle(std::shared_ptr<const ProofResponse> value)
{
	if(!value->proof || !value->request) {
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
		if(is_synced) {
			log(WARN) << "Got invalid proof: " << ex.what();
		}
	}
}

void Node::check_vdfs()
{
	for(auto iter = pending_vdfs.begin(); iter != pending_vdfs.end();) {
		const auto& proof = iter->second;
		if(verified_vdfs.count(proof->height - 1)) {
			if(!vdf_verify_pending.count(proof->height)) {
				add_task([this, proof]() {
					handle(proof);
				});
				iter = pending_vdfs.erase(iter);
				continue;
			}
		}
		iter++;
	}
}

void Node::add_fork(std::shared_ptr<fork_t> fork)
{
	const auto& block = fork->block;
	if(fork_tree.emplace(block->hash, fork).second)
	{
		fork_index.emplace(block->height, fork);
		// add transactions to pool
		for(const auto& tx : block->tx_list) {
			add_block_tx(block, tx);
		}
		add_block_tx(block, block->tx_base);
	}
	if(add_dummy_block(block)) {
		fork->has_dummy_block = true;
	}
}

void Node::add_block_tx(std::shared_ptr<const BlockHeader> block, std::shared_ptr<const TransactionBase> base)
{
	if(auto tx = std::dynamic_pointer_cast<const Transaction>(base)) {
		tx_pool.emplace(tx->id, tx);
	}
}

bool Node::add_dummy_block(std::shared_ptr<const BlockHeader> prev)
{
	if(auto vdf_point = find_next_vdf_point(prev))
	{
		auto block = Block::create();
		block->prev = prev->hash;
		block->height = prev->height + 1;
		block->time_diff = prev->time_diff;
		block->space_diff = prev->space_diff;
		block->vdf_iters = vdf_point->vdf_iters;
		block->vdf_output = vdf_point->output;
		block->finalize();
		add_block(block);
		return true;
	}
	return false;
}

void Node::update()
{
	const auto time_begin = vnx::get_wall_time_micros();

	check_vdfs();

	// add dummy blocks
	{
		std::vector<std::shared_ptr<const Block>> blocks;
		for(const auto& entry : fork_index) {
			const auto& fork = entry.second;
			if(!fork->has_dummy_block) {
				blocks.push_back(fork->block);
			}
		}
		for(const auto& block : blocks) {
			add_dummy_block(block);
		}
	}

	// verify proof where possible
	std::vector<std::shared_ptr<fork_t>> to_verify;
	{
		const auto root = get_root();
		for(const auto& entry : fork_index)
		{
			const auto& fork = entry.second;
			if(fork->is_proof_verified) {
				continue;
			}
			const auto& block = fork->block;
			if(!fork->prev.lock()) {
				if(auto prev = find_fork(block->prev)) {
					if(prev->is_invalid) {
						fork->is_invalid = true;
					}
					fork->prev = prev;
				}
			}
			if(!fork->diff_block) {
				fork->diff_block = find_diff_header(block);
			}
			if(fork->is_invalid || !fork->diff_block) {
				continue;
			}
			const auto prev = find_prev_header(block);
			if(!prev) {
				continue;	// wait for previous block
			}
			bool vdf_passed = false;
			if(auto point = find_vdf_point(block->height, prev->vdf_iters, block->vdf_iters, prev->vdf_output, block->vdf_output)) {
				vdf_passed = true;
				fork->is_vdf_verified = true;
				fork->vdf_point = point;
			}
			if(vdf_passed || !is_synced) {
				to_verify.push_back(fork);
			}
		}
	}

#pragma omp parallel for
	for(size_t i = 0; i < to_verify.size(); ++i)
	{
		const auto& fork = to_verify[i];
		const auto& block = fork->block;
		try {
			hash_t vdf_challenge;
			if(find_vdf_challenge(block, vdf_challenge)) {
				verify_proof(fork, vdf_challenge);
			}
		}
		catch(const std::exception& ex) {
			fork->is_invalid = true;
			log(WARN) << "Proof verification failed for a block at height " << block->height << " with: " << ex.what();
		}
	}
	const auto prev_peak = get_peak();

	std::shared_ptr<const BlockHeader> forked_at;

	// choose best fork
	while(true) {
		forked_at = nullptr;

		const auto best_fork = find_best_fork();

		if(!best_fork || best_fork->block->hash == state_hash) {
			break;	// no change
		}

		// verify and apply new fork
		try {
			forked_at = fork_to(best_fork);
		}
		catch(...) {
			continue;	// try again
		}
		purge_tree();

		const auto peak = best_fork->block;
		const auto fork_line = get_fork_line();

		// show finalized blocks
		for(const auto& fork : fork_line) {
			const auto& block = fork->block;
			if(block->height + params->finality_delay + 1 < peak->height) {
				if(!fork->is_finalized) {
					fork->is_finalized = true;
					if(!do_sync || (sync_peak && block->height >= *sync_peak)) {
						log(INFO) << "Finalized height " << block->height << " with: ntx = " << block->tx_list.size()
								<< ", k = " << (block->proof ? block->proof->ksize : 0)
								<< ", score = " << fork->proof_score << ", buffer = " << fork->weight_buffer
								<< ", tdiff = " << block->time_diff << ", sdiff = " << block->space_diff
								<< (fork->has_weak_proof ? ", weak proof" : "");
					}
				}
			}
		}

		// commit to history
		for(size_t i = 0; i + params->commit_delay < fork_line.size(); ++i)
		{
			const auto& fork = fork_line[i];
			const auto& block = fork->block;
			if(!fork->is_vdf_verified) {
				break;	// wait for VDF verify
			}
			if(!is_synced && fork_line.size() < max_fork_length) {
				// check if there is a competing fork at this height
				const auto finalized_height = peak->height - std::min(params->finality_delay + 1, peak->height);
				if(std::distance(fork_index.lower_bound(block->height), fork_index.upper_bound(block->height)) > 1
					&& std::distance(fork_index.lower_bound(finalized_height), fork_index.upper_bound(finalized_height)) > 1)
				{
					break;
				}
			}
			commit(block);
		}
		break;
	}

	const auto peak = get_peak();
	if(!peak) {
		log(WARN) << "Have no peak!";
		return;
	}
	const auto root = get_root();
	const auto elapsed = (vnx::get_wall_time_micros() - time_begin) / 1e6;

	if(!prev_peak || peak->hash != prev_peak->hash)
	{
		stuck_timer->reset();
		if(auto fork = find_fork(peak->hash)) {
			auto vdf_point = fork->vdf_point;
			log(INFO) << "New peak at height " << peak->height << " with score " << std::to_string(fork->proof_score)
					<< (is_synced && forked_at ? ", forked at " + std::to_string(forked_at->height) : "")
					<< (is_synced && vdf_point ? ", delay " + std::to_string((fork->recv_time - vdf_point->recv_time) / 1e6) + " sec" : "")
					<< ", took " << elapsed << " sec";
		}
	}

	if(!is_synced && sync_peak && sync_pending.empty())
	{
		if(sync_retry < num_sync_retries) {
			log(INFO) << "Reached sync peak at height " << *sync_peak - 1;
			sync_pos = *sync_peak;
			sync_peak = nullptr;
			sync_retry++;
		} else if(peak->height + params->commit_delay + 1 < *sync_peak) {
			log(ERROR) << "Sync failed, it appears we have forked from the network a while ago.";
			const auto replay_height = peak->height - std::min<uint32_t>(1000, peak->height);
			vnx::write_config("Node.replay_height", replay_height);
			log(INFO) << "Restarting with --Node.replay_height " << replay_height;
			exit();
			return;
		} else {
			is_synced = true;
			log(INFO) << "Finished sync at height " << peak->height;
		}
	}
	if(!is_synced) {
		sync_more();
		update_timer->reset();
		return;
	}
	if(light_mode) {
		return;
	}
	{
		// publish time infusions for VDF 0
		auto infuse = TimeInfusion::create();
		infuse->chain = 0;
		auto vdf_iters = peak->vdf_iters;
		for(uint32_t i = 0; i <= params->finality_delay; ++i)
		{
			if(auto diff_block = find_diff_header(peak, i + 1))
			{
				if(auto prev = find_prev_header(peak, params->finality_delay - i, true))
				{
					if(vdf_iters > 0) {
						infuse->values[vdf_iters] = prev->hash;
					}
				}
				vdf_iters += diff_block->time_diff * params->time_diff_constant;
			}
		}
		publish(infuse, output_timelord_infuse);
	}
	{
		// publish next time infusion for VDF 1
		uint32_t height = peak->height;
		height -= (height % params->challenge_interval);
		if(auto prev = find_prev_header(peak, peak->height - height))
		{
			if(prev->height >= params->challenge_interval)
			{
				if(auto diff_block = find_prev_header(prev, params->challenge_interval, true))
				{
					auto infuse = TimeInfusion::create();
					infuse->chain = 1;
					const auto vdf_iters = prev->vdf_iters + diff_block->time_diff * params->time_diff_constant * params->finality_delay;
					infuse->values[vdf_iters] = prev->hash;
					publish(infuse, output_timelord_infuse);
				}
			}
		}
	}
	{
		// request next VDF proofs
		auto vdf_iters = peak->vdf_iters;
		for(uint32_t i = 0; i < params->finality_delay; ++i)
		{
			if(auto diff_block = find_diff_header(peak, i + 1))
			{
				auto request = IntervalRequest::create();
				request->begin = vdf_iters;
				if(i == 0) {
					request->has_start = true;
					request->start_values = peak->vdf_output;
				}
				vdf_iters += diff_block->time_diff * params->time_diff_constant;
				request->end = vdf_iters;
				request->height = peak->height + i + 1;
				request->num_segments = params->num_vdf_segments;
				publish(request, output_interval_request);
			}
		}
	}

	// try to make a block
	{
		auto prev = peak;
		bool made_block = false;
		for(uint32_t i = 0; prev && i <= 1; ++i)
		{
			if(prev->height < root->height) {
				break;
			}
			hash_t vdf_challenge;
			if(!find_vdf_challenge(prev, vdf_challenge, 1)) {
				break;
			}
			const auto challenge = get_challenge(prev, vdf_challenge, 1);

			auto iter = proof_map.find(challenge);
			if(iter != proof_map.end()) {
				const auto proof = iter->second;
				// check if it's our proof
				if(vnx::get_pipe(proof->farmer_addr))
				{
					const auto next_height = prev->height + 1;
					const auto best_fork = find_best_fork(prev, &next_height);
					// check if we have a better proof
					if(!best_fork || proof->score < best_fork->proof_score) {
						try {
							if(make_block(prev, proof)) {
								made_block = true;
							}
						}
						catch(const std::exception& ex) {
							log(WARN) << "Failed to create a block: " << ex.what();
						}
						// revert back to peak
						if(auto fork = find_fork(peak->hash)) {
							fork_to(fork);
						}
					}
				}
			}
			prev = find_prev_header(prev);
		}
		if(made_block) {
			// update again right away
			add_task(std::bind(&Node::update, this));
		}
	}

	// publish challenges
	for(uint32_t i = 0; i <= params->challenge_delay; ++i)
	{
		hash_t vdf_challenge;
		if(find_vdf_challenge(peak, vdf_challenge, i))
		{
			const auto challenge = get_challenge(peak, vdf_challenge, i);

			auto value = Challenge::create();
			value->height = peak->height + i;
			value->challenge = challenge;
			if(auto diff_block = find_diff_header(peak, i)) {
				value->space_diff = diff_block->space_diff;
				publish(value, output_challenges);
			}
		}
	}
}

bool Node::make_block(std::shared_ptr<const BlockHeader> prev, std::shared_ptr<const ProofResponse> response)
{
	const auto time_begin = vnx::get_wall_time_micros();

	const auto vdf_point = find_next_vdf_point(prev);
	if(!vdf_point) {
		return false;
	}
	const auto prev_fork = find_fork(prev->hash);

	// reset state to previous block
	fork_to(prev->hash);

	auto block = Block::create();
	block->prev = prev->hash;
	block->height = prev->height + 1;
	block->time_diff = prev->time_diff;
	block->space_diff = prev->space_diff;
	block->vdf_iters = vdf_point->vdf_iters;
	block->vdf_output = vdf_point->output;

	// set new time difficulty
	if(auto fork = find_prev_fork(prev_fork, params->finality_delay)) {
		if(auto point = fork->vdf_point) {
			const int64_t time_delta = (vdf_point->recv_time - point->recv_time) / (params->finality_delay + 1);
			if(time_delta > 0) {
				const double gain = 0.1;
				if(auto diff_block = fork->diff_block) {
					double new_diff = params->block_time * diff_block->time_diff / (time_delta * 1e-6);
					new_diff = prev->time_diff * (1 - gain) + new_diff * gain;
					block->time_diff = std::max<int64_t>(new_diff + 0.5, 1);
				}
			}
		}
	}
	{
		// set new space difficulty
		double avg_score = response->score;
		{
			uint32_t counter = 1;
			auto fork = prev_fork;
			for(uint32_t i = 1; i < params->finality_delay && fork; ++i) {
				avg_score += fork->proof_score;
				fork = fork->prev.lock();
				counter++;
			}
			avg_score /= counter;
		}
		double delta = prev->space_diff * (params->score_target - avg_score);
		delta /= params->score_target;
		delta /= (1 << params->max_diff_adjust);

		int64_t update = 0;
		if(delta > 0 && delta < 1) {
			update = 1;
		} else if(delta < 0 && delta > -1) {
			update = -1;
		} else {
			update = delta + 0.5;
		}
		const int64_t new_diff = prev->space_diff + update;
		block->space_diff = std::max<int64_t>(new_diff, 1);
	}
	{
		const auto max_update = std::max<uint64_t>(prev->time_diff >> params->max_diff_adjust, 1);
		block->time_diff = std::min(block->time_diff, prev->time_diff + max_update);
		block->time_diff = std::max(block->time_diff, prev->time_diff - max_update);
	}
	{
		const auto max_update = std::max<uint64_t>(prev->space_diff >> params->max_diff_adjust, 1);
		block->space_diff = std::min(block->space_diff, prev->space_diff + max_update);
		block->space_diff = std::max(block->space_diff, prev->space_diff - max_update);
	}
	block->proof = response->proof;

	struct tx_entry_t {
		uint64_t fees = 0;
		uint64_t cost = 0;
		double fee_ratio = 0;
		std::shared_ptr<const Transaction> tx;
	};

	std::vector<tx_entry_t> tx_list;
	std::unordered_set<hash_t> invalid;
	std::unordered_set<hash_t> postpone;
	std::unordered_set<txio_key_t> spent;

	auto context = Context::create();
	context->height = block->height;

	for(const auto& entry : tx_pool)
	{
		if(tx_map.count(entry.first)) {
			// already included in a previous block
			continue;
		}
		const auto& tx = entry.second;
		try {
			for(const auto& in : tx->inputs) {
				if(!spent.insert(in.prev).second) {
					throw std::logic_error("double spend");
				}
			}
			tx_entry_t entry;
			entry.tx = tx;
			tx_list.push_back(entry);
		}
		catch(const std::exception& ex) {
			invalid.insert(entry.first);
			log(WARN) << "TX validation failed with: " << ex.what();
		}
	}

#pragma omp parallel for
	for(size_t i = 0; i < tx_list.size(); ++i)
	{
		auto& entry = tx_list[i];
		auto& tx = entry.tx;
		// check if tx depends on another one which is not in a block yet
		bool depends = false;
		for(const auto& in : tx->inputs) {
			if(tx_pool.count(in.prev.txid) && !tx_map.count(in.prev.txid)) {
				depends = true;
			}
		}
		if(depends) {
#pragma omp critical
			postpone.insert(tx->id);
			continue;
		}
		try {
			const auto cost = tx->calc_min_fee(params);
			if(cost > params->max_block_cost) {
				throw std::logic_error("tx cost > max_block_cost");
			}
			if(!tx->exec_outputs.empty()) {
				auto copy = vnx::clone(tx);
				copy->exec_outputs.clear();
				tx = copy;
			}
			if(auto new_tx = validate(tx, context, nullptr, entry.fees)) {
				tx = new_tx;
			}
			entry.cost = cost;
			entry.fee_ratio = entry.fees / double(cost);
		}
		catch(const std::exception& ex) {
#pragma omp critical
			invalid.insert(tx->id);
			log(WARN) << "TX validation failed with: " << ex.what();
		}
	}

	// sort by fee ratio
	std::sort(tx_list.begin(), tx_list.end(),
		[](const tx_entry_t& lhs, const tx_entry_t& rhs) -> bool {
			return lhs.fee_ratio > rhs.fee_ratio;
		});

	uint64_t total_fees = 0;
	uint64_t total_cost = 0;
	for(size_t i = 0; i < tx_list.size(); ++i)
	{
		const auto& entry = tx_list[i];
		const auto& tx = entry.tx;
		if(!invalid.count(tx->id) && !postpone.count(tx->id))
		{
			if(total_cost + entry.cost < params->max_block_cost)
			{
				block->tx_list.push_back(tx);
				total_fees += entry.fees;
				total_cost += entry.cost;
			}
		}
	}
	for(const auto& id : invalid) {
		tx_pool.erase(id);
	}
	block->finalize();

	FarmerClient farmer(response->farmer_addr);
	const auto block_reward = calc_block_reward(block);
	const auto final_reward = calc_final_block_reward(params, block_reward, total_fees);
	const auto result = farmer.sign_block(block, final_reward);

	if(!result) {
		throw std::logic_error("farmer refused");
	}
	block->tx_base = result->tx_base;
	block->farmer_sig = result->farmer_sig;
	block->finalize();

	add_block(block);

	const auto elapsed = (vnx::get_wall_time_micros() - time_begin) / 1e6;
	log(INFO) << "Created block at height " << block->height << " with: ntx = " << block->tx_list.size()
			<< ", score = " << response->score << ", reward = " << final_reward / pow(10, params->decimals) << " MMX"
			<< ", nominal = " << block_reward / pow(10, params->decimals) << " MMX"
			<< ", fees = " << total_fees / pow(10, params->decimals) << " MMX"
			<< ", took " << elapsed << " sec";
	return true;
}

void Node::print_stats()
{
	 log(INFO) << tx_pool.size() << " tx pool, " << utxo_map.size() << " utxo, " << change_log.size() << " / " << fork_tree.size() << " blocks";
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
	uint128_t max_weight = 0;
	std::shared_ptr<fork_t> best_fork;
	const auto begin = at_height ? fork_index.lower_bound(*at_height) : fork_index.upper_bound(root->height);
	const auto end =   at_height ? fork_index.upper_bound(*at_height) : fork_index.end();
	for(auto iter = begin; iter != end; ++iter)
	{
		const auto& fork = iter->second;
		if(!fork->is_proof_verified || fork->is_invalid) {
			continue;
		}
		if(auto prev = fork->prev.lock()) {
			if(prev->is_invalid) {
				fork->is_invalid = true;
				continue;
			}
			fork->total_weight = prev->total_weight + fork->weight + std::min<int32_t>(prev->weight_buffer, params->score_threshold);
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

void Node::validate(std::shared_ptr<const Block> block) const
{
	const auto prev = find_prev_header(block);
	if(!prev) {
		throw std::logic_error("invalid prev");
	}
	if(prev->hash != state_hash) {
		throw std::logic_error("state mismatch");
	}
	if(block->height != prev->height + 1) {
		throw std::logic_error("invalid height");
	}
	if(block->time_diff == 0 || block->space_diff == 0) {
		throw std::logic_error("invalid difficulty");
	}
	if(auto proof = block->proof) {
		if(!block->farmer_sig || !block->farmer_sig->verify(proof->farmer_key, block->hash)) {
			throw std::logic_error("invalid farmer signature");
		}
		validate_diff_adjust(block->time_diff, prev->time_diff);
		validate_diff_adjust(block->space_diff, prev->space_diff);
	} else {
		if(block->tx_base || !block->tx_list.empty()) {
			throw std::logic_error("transactions not allowed");
		}
		if(block->time_diff != prev->time_diff || block->space_diff != prev->space_diff) {
			throw std::logic_error("invalid difficulty adjustment");
		}
	}
	auto context = Context::create();
	context->height = block->height;

	uint64_t base_spent = 0;
	if(auto tx = std::dynamic_pointer_cast<const Transaction>(block->tx_base)) {
		if(validate(tx, context, block, base_spent)) {
			throw std::logic_error("missing exec_outputs");
		}
	}
	{
		std::unordered_set<txio_key_t> inputs;
		for(const auto& base : block->tx_list) {
			if(auto tx = std::dynamic_pointer_cast<const Transaction>(base)) {
				for(const auto& in : tx->inputs) {
					if(!inputs.insert(in.prev).second) {
						throw std::logic_error("double spend");
					}
				}
			} else {
				throw std::logic_error("transaction missing");
			}
		}
	}
	std::exception_ptr failed_ex;
	std::atomic<uint64_t> total_fees {0};
	std::atomic<uint64_t> total_cost {0};

#pragma omp parallel for
	for(size_t i = 0; i < block->tx_list.size(); ++i)
	{
		const auto& base = block->tx_list[i];
		try {
			if(auto tx = std::dynamic_pointer_cast<const Transaction>(base)) {
				uint64_t fees = 0;
				if(validate(tx, context, nullptr, fees)) {
					throw std::logic_error("missing exec_outputs");
				}
				total_fees += fees;
				total_cost += tx->calc_min_fee(params);
			}
		} catch(...) {
#pragma omp critical
			failed_ex = std::current_exception();
		}
	}
	if(failed_ex) {
		std::rethrow_exception(failed_ex);
	}
	if(total_cost > params->max_block_cost) {
		throw std::logic_error("block cost too high: " + std::to_string(uint64_t(total_cost)));
	}
	const auto base_reward = calc_block_reward(block);
	const auto base_allowed = calc_final_block_reward(params, base_reward, total_fees);
	if(base_spent > base_allowed) {
		throw std::logic_error("coin base over-spend");
	}
}

std::shared_ptr<const Context> Node::create_context(std::shared_ptr<const Contract> contract,
													std::shared_ptr<const Context> base, std::shared_ptr<const Transaction> tx) const
{
	auto context = vnx::clone(base);
	context->txid = tx->id;
	for(const auto& addr : contract->get_dependency()) {
		if(auto contract = get_contract(addr)) {
			context->depends[addr] = contract;
		} else {
			auto pubkey = contract::PubKey::create();
			pubkey->address = addr;
			context->depends[addr] = pubkey;
		}
	}
	return context;
}

std::shared_ptr<const Transaction> Node::validate(	std::shared_ptr<const Transaction> tx, std::shared_ptr<const Context> context,
													std::shared_ptr<const Block> base, uint64_t& fee_amount) const
{
	if(tx->id != tx->calc_hash()) {
		throw std::logic_error("invalid tx id");
	}
	if(base) {
		if(tx->deploy) {
			throw std::logic_error("coin base cannot deploy");
		}
		if(!tx->execute.empty()) {
			throw std::logic_error("coin base cannot have operations");
		}
		if(!tx->exec_outputs.empty()) {
			throw std::logic_error("coin base cannot have execution outputs");
		}
		if(tx->outputs.size() > params->max_tx_base_out) {
			throw std::logic_error("coin base has too many outputs");
		}
		if(tx->inputs.size() != 1) {
			throw std::logic_error("coin base must have one input");
		}
		const auto& in = tx->inputs[0];
		if(in.prev.txid != hash_t(base->prev) || in.prev.index != 0) {
			throw std::logic_error("invalid coin base input");
		}
	} else {
		if(tx->inputs.empty()) {
			throw std::logic_error("tx without input");
		}
	}
	uint64_t base_amount = 0;
	std::vector<tx_out_t> exec_outputs;
	std::unordered_map<hash_t, uint64_t> amounts;

	if(!base) {
		for(const auto& in : tx->inputs)
		{
			auto iter = utxo_map.find(in.prev);
			if(iter == utxo_map.end()) {
				throw std::logic_error("utxo not found");
			}
			const auto& utxo = iter->second;

			const auto solution = tx->get_solution(in.solution);
			if(!solution) {
				throw std::logic_error("missing solution");
			}
			auto contract = get_contract(utxo.address);
			if(!contract) {
				auto pubkey = contract::PubKey::create();
				pubkey->address = utxo.address;
				contract = pubkey;
			}
			auto spend = operation::Spend::create();
			spend->address = utxo.address;
			spend->solution = solution;
			spend->key = in.prev;
			spend->utxo = utxo;

			const auto outputs = contract->validate(spend, create_context(contract, context, tx));
			exec_outputs.insert(exec_outputs.end(), outputs.begin(), outputs.end());

			amounts[utxo.contract] += utxo.amount;
		}
	}
	for(const auto& op : tx->execute)
	{
		if(auto contract = get_contract(op->address)) {
			const auto outputs = contract->validate(op, create_context(contract, context, tx));
			exec_outputs.insert(exec_outputs.end(), outputs.begin(), outputs.end());
		} else {
			throw std::logic_error("no such contract");
		}
	}
	for(const auto& out : tx->outputs)
	{
		if(out.amount == 0) {
			throw std::logic_error("zero tx output");
		}
		if(base) {
			if(out.contract != hash_t()) {
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
	if(tx->deploy) {
		if(auto nft = std::dynamic_pointer_cast<const contract::NFT>(tx->deploy)) {
			tx_out_t out;
			out.contract = tx->id;
			out.address = nft->creator;
			out.amount = 1;
			exec_outputs.push_back(out);
		}
	}
	if(base) {
		fee_amount = base_amount;
		return nullptr;
	}
	fee_amount = amounts[hash_t()];

	const auto fee_needed = tx->calc_min_fee(params);
	if(fee_amount < fee_needed) {
		throw std::logic_error("insufficient fee: " + std::to_string(fee_amount) + " < " + std::to_string(fee_needed));
	}
	if(tx->exec_outputs.empty()) {
		if(!exec_outputs.empty()) {
			auto copy = vnx::clone(tx);
			copy->exec_outputs = exec_outputs;
			return copy;
		}
	} else {
		if(tx->exec_outputs.size() != exec_outputs.size()) {
			throw std::logic_error("exec_outputs size mismatch");
		}
		for(size_t i = 0; i < exec_outputs.size(); ++i) {
			const auto& lhs = exec_outputs[i];
			const auto& rhs = tx->exec_outputs[i];
			if(lhs.contract != rhs.contract || lhs.address != rhs.address || lhs.amount != rhs.amount) {
				throw std::logic_error("exec_output mismatch at index " + std::to_string(i));
			}
		}
	}
	return nullptr;
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
		auto iter = tx_map.find(txid);
		if(iter != tx_map.end()) {
			tx_map.erase(iter);
		}
		tx_pool.erase(txid);
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

void Node::verify_proof(std::shared_ptr<fork_t> fork, const hash_t& vdf_challenge) const
{
	const auto block = fork->block;
	const auto prev = find_prev_header(block);
	const auto diff_block = fork->diff_block;
	if(!prev || !diff_block) {
		throw std::logic_error("cannot verify");
	}
	if(block->vdf_iters != prev->vdf_iters + diff_block->time_diff * params->time_diff_constant) {
		throw std::logic_error("invalid vdf_iters");
	}
	// Note: vdf_output already verified to match vdf_iters

	if(block->proof) {
		const auto challenge = get_challenge(block, vdf_challenge);
		fork->proof_score = verify_proof(block->proof, challenge, diff_block->space_diff);

		// check if block has a weak proof
		const auto iter = proof_map.find(challenge);
		if(iter != proof_map.end()) {
			if(fork->proof_score > iter->second->score) {
				fork->has_weak_proof = true;
				log(INFO) << "Got weak proof block for height " << block->height << " with score " << fork->proof_score << " > " << iter->second->score;
			}
		}
	} else {
		fork->proof_score = params->score_threshold;
	}
	fork->weight = 0;
	fork->buffer_delta = 0;
	if(fork->has_weak_proof) {
		fork->weight -= params->score_threshold;
	} else if(block->proof) {
		fork->weight += 1 + params->score_threshold - fork->proof_score;
		fork->buffer_delta += int32_t(2 * params->score_target) - fork->proof_score;
	} else {
		fork->weight += 1;
		fork->buffer_delta -= params->score_threshold;
	}
	fork->weight *= diff_block->space_diff;
	fork->weight *= diff_block->time_diff;

	// check some VDFs during sync
	if(!fork->is_proof_verified && !fork->is_vdf_verified) {
		if(vnx::rand64() % vdf_check_divider) {
			fork->is_vdf_verified = true;
		}
	}
	fork->is_proof_verified = true;
}

uint32_t Node::verify_proof(std::shared_ptr<const ProofOfSpace> proof, const hash_t& challenge, const uint64_t space_diff) const
{
	if(proof->ksize < params->min_ksize) {
		throw std::logic_error("ksize too small");
	}
	if(proof->ksize > params->max_ksize) {
		throw std::logic_error("ksize too big");
	}
	const bls_pubkey_t plot_key = proof->local_key.to_bls() + proof->farmer_key.to_bls();

	const uint32_t port = 11337;
	if(hash_t(hash_t(proof->pool_key + plot_key) + bytes_t<4>(&port, 4)) != proof->plot_id) {
		throw std::logic_error("invalid proof keys or port");
	}
	if(!proof->local_sig.verify(proof->local_key, proof->calc_hash())) {
		throw std::logic_error("invalid proof signature");
	}
	if(!check_plot_filter(params, challenge, proof->plot_id)) {
		throw std::logic_error("plot filter failed");
	}
	const auto quality = hash_t::from_bytes(chiapos::verify(
			proof->ksize, proof->plot_id.bytes, challenge.bytes, proof->proof_bytes.data(), proof->proof_bytes.size()));

	const auto score = calc_proof_score(params, proof->ksize, quality, space_diff);
	if(score >= params->score_threshold) {
		throw std::logic_error("invalid score");
	}
	return score;
}

void Node::verify_vdf(std::shared_ptr<const ProofOfTime> proof) const
{
	// check number of segments
	if(proof->segments.size() < params->min_vdf_segments) {
		throw std::logic_error("not enough segments: " + std::to_string(proof->segments.size()));
	}
	if(proof->segments.size() > params->max_vdf_segments) {
		throw std::logic_error("too many segments: " + std::to_string(proof->segments.size()));
	}

	// check proper infusions
	if(proof->start > 0) {
		if(!proof->infuse[0]) {
			throw std::logic_error("missing infusion on chain 0");
		}
		const auto infused_block = find_header(*proof->infuse[0]);
		if(!infused_block) {
			throw std::logic_error("unknown block infused on chain 0");
		}
		if(infused_block->height + std::min(params->finality_delay + 1, proof->height) != proof->height) {
			throw std::logic_error("invalid block height infused on chain 0");
		}
		if(infused_block->height >= params->challenge_interval
			&& infused_block->height % params->challenge_interval == 0)
		{
			if(!proof->infuse[1]) {
				throw std::logic_error("missing infusion on chain 1");
			}
			if(*proof->infuse[1] != *proof->infuse[0]) {
				throw std::logic_error("invalid infusion on chain 1");
			}
		}
	}
	vdf_threads->add_task(std::bind(&Node::verify_vdf_task, this, proof));
}

void Node::verify_vdf(std::shared_ptr<const ProofOfTime> proof, const uint32_t chain) const
{
	const auto& segments = proof->segments;
	bool is_valid = !segments.empty();
	size_t invalid_segment = -1;

#pragma omp parallel for
	for(size_t i = 0; i < segments.size(); ++i)
	{
		if(!is_valid) {
			continue;
		}
		hash_t point;
		if(i > 0) {
			point = segments[i-1].output[chain];
		} else {
			point = proof->input[chain];
			if(auto infuse = proof->infuse[chain]) {
				point = hash_t(point + *infuse);
			}
		}
		for(size_t k = 0; k < segments[i].num_iters; ++k) {
			point = hash_t(point.bytes);
		}
		if(point != segments[i].output[chain]) {
			is_valid = false;
			invalid_segment = i;
		}
	}
	if(!is_valid) {
		throw std::logic_error("invalid output at segment " + std::to_string(invalid_segment));
	}
}

void Node::verify_vdf_success(std::shared_ptr<const ProofOfTime> proof, std::shared_ptr<vdf_point_t> point)
{
	if(verified_vdfs.count(proof->height) == 0) {
		log(INFO) << "-------------------------------------------------------------------------------";
	}
	verified_vdfs.emplace(proof->height, point);
	vdf_verify_pending.erase(proof->height);

	const auto elapsed = (vnx::get_wall_time_micros() - point->recv_time) / 1e6;
	if(elapsed > params->block_time) {
		log(WARN) << "VDF verification took longer than block interval, unable to keep sync!";
	}
	else if(elapsed > 0.7 * params->block_time) {
		log(WARN) << "VDF verification took longer than recommended: " << elapsed << " sec";
	}

	std::shared_ptr<const vdf_point_t> prev;
	for(auto iter = verified_vdfs.lower_bound(proof->height - 1); iter != verified_vdfs.lower_bound(proof->height); ++iter) {
		if(iter->second->output == point->input) {
			prev = iter->second;
		}
	}
	log(INFO) << "Verified VDF for height " << proof->height <<
			(prev ? ", delta = " + std::to_string((point->recv_time - prev->recv_time) / 1e6) + " sec" : "") << ", took " << elapsed << " sec";

	// add dummy blocks in case no proof is found
	{
		std::vector<std::shared_ptr<const BlockHeader>> prev_blocks;
		if(auto root = get_root()) {
			if(root->height + 1 == proof->height) {
				prev_blocks.push_back(root);
			}
		}
		for(auto iter = fork_index.lower_bound(proof->height - 1); iter != fork_index.upper_bound(proof->height - 1); ++iter) {
			prev_blocks.push_back(iter->second->block);
		}
		const auto infused_hash = proof->infuse[0];
		for(auto prev : prev_blocks) {
			if(infused_hash) {
				if(auto infused = find_prev_header(prev, params->finality_delay, true)) {
					if(infused->hash != *infused_hash) {
						continue;
					}
				}
			}
			auto block = Block::create();
			block->prev = prev->hash;
			block->height = proof->height;
			block->time_diff = prev->time_diff;
			block->space_diff = prev->space_diff;
			block->vdf_iters = point->vdf_iters;
			block->vdf_output = point->output;
			block->finalize();
			add_block(block);
		}
	}
	update();
}

void Node::verify_vdf_failed(std::shared_ptr<const ProofOfTime> proof)
{
	vdf_verify_pending.erase(proof->height);
	check_vdfs();
}

void Node::verify_vdf_task(std::shared_ptr<const ProofOfTime> proof) const noexcept
{
	std::lock_guard<std::mutex> lock(vdf_mutex);

	const auto time_begin = vnx::get_wall_time_micros();
	try {
		for(int i = 0; i < 2; ++i) {
			if(auto engine = opencl_vdf[i]) {
				engine->compute(proof, i);
			}
		}
		for(int i = 0; i < 2; ++i) {
			if(auto engine = opencl_vdf[i]) {
				engine->verify(proof, i);
			} else {
				verify_vdf(proof, i);
			}
		}
		auto point = std::make_shared<vdf_point_t>();
		point->height = proof->height;
		point->vdf_start = proof->start;
		point->vdf_iters = proof->get_vdf_iters();
		point->input = proof->input;
		point->output[0] = proof->get_output(0);
		point->output[1] = proof->get_output(1);
		point->infused = proof->infuse[0];
		point->proof = proof;
		point->recv_time = time_begin;

		add_task([this, proof, point]() {
			((Node*)this)->verify_vdf_success(proof, point);
		});
	}
	catch(const std::exception& ex) {
		add_task([this, proof]() {
			((Node*)this)->verify_vdf_failed(proof);
		});
		log(WARN) << "VDF verification for height " << proof->height << " failed with: " << ex.what();
	}
}

void Node::check_vdf_task(std::shared_ptr<fork_t> fork, std::shared_ptr<const BlockHeader> prev, std::shared_ptr<const BlockHeader> infuse) const noexcept
{
	// MAY NOT ACCESS ANY NODE DATA EXCEPT "params"
	const auto time_begin = vnx::get_wall_time_micros();
	const auto& block = fork->block;

	auto point = prev->vdf_output;
	point[0] = hash_t(point[0] + infuse->hash);

	if(infuse->height >= params->challenge_interval && infuse->height % params->challenge_interval == 0) {
		point[1] = hash_t(point[1] + infuse->hash);
	}
	const auto num_iters = block->vdf_iters - prev->vdf_iters;

	for(uint64_t i = 0; i < num_iters; ++i) {
		for(int chain = 0; chain < 2; ++chain) {
			point[chain] = hash_t(point[chain].bytes);
		}
	}
	if(point == block->vdf_output) {
		fork->is_vdf_verified = true;
		const auto elapsed = (vnx::get_wall_time_micros() - time_begin) / 1e6;
		log(INFO) << "VDF check for height " << block->height << " passed, took " << elapsed << " sec";
	} else {
		fork->is_invalid = true;
		log(WARN) << "VDF check for height " << block->height << " failed!";
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
	if(light_mode && tx->deploy) {
		light_address_set.insert(tx->id);
		log.deployed.push_back(tx->id);
	}
	tx_map[tx->id] = block->height;
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
	if(light_mode) {
		for(const auto& addr : log->deployed) {
			light_address_set.erase(addr);
		}
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
		if(auto value = vnx::read(in)) {
			auto header = std::dynamic_pointer_cast<BlockHeader>(value);
			if(!header) {
				return nullptr;
			}
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
	vnx::write(out, block->get_header());

	for(const auto& tx : block->tx_list) {
		const auto offset = out.get_output_pos();
		if(std::dynamic_pointer_cast<const Transaction>(tx)) {
			tx_log.insert(block->height, tx->id);
			tx_index.insert(tx->id, std::make_pair(offset, block->height));
		}
		vnx::write(out, tx);
	}
	vnx::write(out, nullptr);
	block_chain->flush();
}


} // mmx
