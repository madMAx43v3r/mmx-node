/*
 * Node.cpp
 *
 *  Created on: Dec 7, 2021
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/Challenge.hxx>
#include <mmx/FarmerClient.hxx>
#include <mmx/TimeInfusion.hxx>
#include <mmx/IntervalRequest.hxx>
#include <mmx/TimeLordClient.hxx>
#include <mmx/contract/PubKey.hxx>
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
	if(light_mode) {
		vnx::read_config("light_address_set", light_address_set);
		log(INFO) << "Got " << light_address_set.size() << " addresses for light mode.";
	} else {
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
		vdf_threads = std::make_shared<vnx::ThreadPool>(1);
	}

	router = std::make_shared<RouterAsyncClient>(router_name);
	http = std::make_shared<vnx::addons::HttpInterface<Node>>(this, vnx_name);
	add_async_client(router);
	add_async_client(http);

	block_chain = std::make_shared<vnx::File>(storage_path + "block_chain.dat");

	if(block_chain->exists()) {
		const auto time_begin = vnx::get_wall_time_millis();
		block_chain->open("rb+");
		int64_t last_pos = 0;
		while(auto block = read_block(true, &last_pos)) {
			if(block->height >= replay_height) {
				block_chain->seek_to(last_pos);
				break;
			}
			apply(block);
			commit(block);
		}
		if(auto block = find_header(state_hash)) {
			log(INFO) << "Loaded " << block->height + 1 << " blocks with " << tx_index.size()
					<< " transactions from disk, took " << (vnx::get_wall_time_millis() - time_begin) / 1e3 << " sec";
		}
	} else {
		block_chain->open("wb");
		block_chain->open("rb+");
	}
	is_replay = false;
	is_synced = !do_sync;

	if(state_hash == hash_t())
	{
		vdf_point_t point;
		point.output[0] = hash_t(params->vdf_seed);
		point.output[1] = hash_t(params->vdf_seed);
		point.recv_time = vnx::get_wall_time_micros();
		verified_vdfs[0] = point;

		auto genesis = Block::create();
		genesis->time_diff = params->initial_time_diff;
		genesis->space_diff = params->initial_space_diff;
		genesis->vdf_output = point.output;
		genesis->finalize();

		apply(genesis);
		commit(genesis);
	}

	if(auto block = find_header(state_hash)) {
		vdf_point_t point;
		point.height = block->height;
		point.iters = block->vdf_iters;
		point.output = block->vdf_output;
		point.recv_time = vnx::get_wall_time_micros();
		verified_vdfs[block->height] = point;
	}

	if(!light_mode) {
		subscribe(input_vdfs, max_queue_ms);
		subscribe(input_proof, max_queue_ms);
		subscribe(input_transactions, max_queue_ms);
		subscribe(input_timelord_vdfs, max_queue_ms);
		subscribe(input_harvester_proof, max_queue_ms);
	}
	subscribe(input_blocks, max_queue_ms);

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

std::shared_ptr<const ChainParams> Node::get_params() const
{
	return params;
}

uint32_t Node::get_height() const
{
	if(auto block = find_header(state_hash)) {
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
		auto iter = tx_index.find(id);
		if(iter != tx_index.end()) {
			return iter->second.second;
		}
	}
	return nullptr;
}

txo_info_t Node::get_txo_info(const txio_key_t& key) const
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
		auto iter = stxo_index.find(key);
		if(iter != stxo_index.end()) {
			txo_info_t info;
			info.output = iter->second.output;
			info.spent = iter->second.key;
			return info;
		}
	}
	for(const auto& log : change_log) {
		auto iter = log->utxo_removed.find(key);
		if(iter != log->utxo_removed.end()) {
			txo_info_t info;
			info.output = iter->second.output;
			info.spent = iter->second.key;
			return info;
		}
	}
	throw std::logic_error("no such txo entry");
}

std::shared_ptr<const Transaction> Node::get_transaction(const hash_t& id) const
{
	{
		auto iter = tx_pool.find(id);
		if(iter != tx_pool.end()) {
			return iter->second;
		}
	}
	{
		auto iter = tx_index.find(id);
		if(iter != tx_index.end()) {
			const auto& entry = iter->second;
			const auto last_pos = block_chain->get_output_pos();
			try {
				block_chain->seek_to(entry.first);
				auto value = vnx::read(block_chain->in);
				block_chain->seek_to(last_pos);
				if(auto tx = std::dynamic_pointer_cast<Transaction>(value)) {
					return tx;
				}
				if(auto header = std::dynamic_pointer_cast<BlockHeader>(value)) {
					if(auto tx = std::dynamic_pointer_cast<const Transaction>(header->tx_base)) {
						if(tx->id == id) {
							return tx;
						}
					}
				}
			}
			catch(...) {
				block_chain->seek_to(last_pos);
				throw;
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
	auto iter = contracts.find(address);
	if(iter != contracts.end()) {
		return iter->second;
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
	return false;
}

void Node::add_block(std::shared_ptr<const Block> block)
{
	if(fork_tree.count(block->hash)) {
		return;
	}
	auto root = get_root();
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
	fork_tree[block->hash] = fork;

	// add dummy block in case no proof found
	auto iter = verified_vdfs.find(block->height + 1);
	if(iter != verified_vdfs.end()) {
		auto next = Block::create();
		next->prev = block->hash;
		next->height = block->height + 1;
		next->time_diff = block->time_diff;
		next->space_diff = block->space_diff;
		next->vdf_iters = iter->second.iters;
		next->vdf_output = iter->second.output;
		next->finalize();
		add_block(next);
	}

	// fetch missing previous
	if(is_synced && !fork_tree.count(block->prev))
	{
		const auto height = block->height - 1;
		if(!sync_pending.count(height)) {
			router->get_blocks_at(height, std::bind(&Node::sync_result, this, height, std::placeholders::_1));
			sync_pending.insert(height);
			log(INFO) << "Fetching missed block at height " << height << " with hash " << block->prev;
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
	const auto tx_cost = tx->calc_min_fee(params);
	if(tx_cost > params->max_block_cost) {
		log(WARN) << "Rejected over-size TX: cost = " << tx_cost;
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
		const auto& out = entry.output;
		if(out.contract == contract) {
			total += out.amount;
		}
	}
	return total;
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
		const auto range = saddr_map.equal_range(addr);
		for(auto iter = range.first; iter != range.second; ++iter) {
			auto iter2 = stxo_index.find(iter->second);
			if(iter2 != stxo_index.end()) {
				res.push_back(stxo_entry_t::create_ex(iter->second, iter2->second.output, iter2->second.key));
			}
		}
	}
	const std::unordered_set<addr_t> addr_set(addresses.begin(), addresses.end());
	for(const auto& log : change_log) {
		for(const auto& entry : log->utxo_removed) {
			const auto& stxo = entry.second.output;
			if(addr_set.count(stxo.address)) {
				res.push_back(stxo_entry_t::create_ex(entry.first, stxo, entry.second.key));
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
	add_block(block);
}

void Node::handle(std::shared_ptr<const Transaction> tx)
{
	add_transaction(tx);
}

void Node::handle(std::shared_ptr<const ProofOfTime> proof)
{
	if(verified_vdfs.count(proof->height)) {
		return;
	}
	const auto height = get_height();
	if(proof->height < height || proof->height > height + params->commit_delay) {
		return;
	}
	if(proof->height == vdf_verify_pending) {
		pending_vdfs.emplace(proof->height, proof);
		return;
	}
	auto iter = verified_vdfs.find(proof->height - 1);
	if(iter != verified_vdfs.end())
	{
		try {
			vdf_verify_pending = proof->height;
			verify_vdf(proof, iter->second);
		}
		catch(const std::exception& ex) {
			if(is_synced) {
				log(WARN) << "VDF verification for height " << proof->height << " failed with: " << ex.what();
			}
			vdf_verify_pending = 0;
		}
	}
	else {
		pending_vdfs.emplace(proof->height, proof);
		log(INFO) << "Waiting on VDF for height " << proof->height - 1;
	}
}

void Node::handle(std::shared_ptr<const ProofResponse> value)
{
	if(!value->proof || !value->request) {
		return;
	}
	const auto root = get_root();
	const auto request = value->request;
	if(request->height <= root->height) {
		return;
	}
	const auto challenge = request->challenge;

	try {
		const auto diff_block = find_diff_header(root, request->height - root->height);
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
			if(proof->height != vdf_verify_pending) {
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

void Node::update()
{
	const auto time_begin = vnx::get_wall_time_micros();

	check_vdfs();

	// verify proof where possible
	std::vector<std::pair<std::shared_ptr<fork_t>, hash_t>> to_verify;
	{
		const auto root = get_root();
		for(const auto& entry : fork_tree)
		{
			const auto& fork = entry.second;
			const auto& block = fork->block;

			bool has_prev = true;
			if(!fork->prev.lock()) {
				if(auto prev = find_fork(block->prev)) {
					if(prev->is_invalid) {
						fork->is_invalid = true;
					}
					fork->prev = prev;
				} else {
					has_prev = false;
				}
			}
			if(!fork->diff_block) {
				fork->diff_block = find_diff_header(block);
			}
			if(!fork->is_invalid && !fork->is_proof_verified && fork->diff_block && (has_prev || block->prev == root->hash))
			{
				bool vdf_passed = light_mode;
				if(is_synced) {
					auto iter2 = verified_vdfs.find(block->height);
					if(iter2 != verified_vdfs.end()) {
						const auto& point = iter2->second;
						if(block->vdf_iters == point.iters && block->vdf_output == point.output) {
							vdf_passed = true;
						} else {
							fork->is_invalid = true;
							log(WARN) << "VDF verification failed for a block at height " << block->height;
						}
					}
				}
				if(!is_synced || vdf_passed)
				{
					hash_t vdf_challenge;
					if(find_vdf_challenge(block, vdf_challenge)) {
						to_verify.emplace_back(fork, vdf_challenge);
					}
				}
			}
		}
	}

#pragma omp parallel for
	for(size_t i = 0; i < to_verify.size(); ++i)
	{
		const auto& entry = to_verify[i];
		const auto& fork = entry.first;
		const auto& block = fork->block;
		try {
			verify_proof(fork, entry.second);
		}
		catch(const std::exception& ex) {
			fork->is_invalid = true;
			log(WARN) << "Proof verification failed for a block at height " << block->height << " with: " << ex.what();
		}
	}
	const auto prev_peak = find_header(state_hash);

	std::shared_ptr<const BlockHeader> forked_at;

	// choose best fork
	while(true) {
		forked_at = nullptr;

		// purge disconnected forks
		purge_tree();

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
		const auto fork_line = get_fork_line();

		// show finalized blocks
		for(const auto& fork : fork_line) {
			if(fork->block->height + params->finality_delay + 1 < fork_line.back()->block->height) {
				if(!fork->is_finalized) {
					fork->is_finalized = true;
					const auto block = fork->block;
					log(INFO) << "Finalized height " << block->height << " with: ntx = " << block->tx_list.size()
							<< ", score = " << fork->proof_score << ", k = " << (block->proof ? block->proof->ksize : 0)
							<< ", tdiff = " << block->time_diff << ", sdiff = " << block->space_diff << (fork->has_weak_proof ? ", weak proof" : "");
				}
			}
		}

		// commit to history
		for(size_t i = 0; i + params->commit_delay < fork_line.size(); ++i) {
			commit(fork_line[i]->block);
		}
		break;
	}

	const auto peak = find_header(state_hash);
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
			const auto prev = fork->prev.lock();
			log(INFO) << "New peak at height " << peak->height << " with score " << std::to_string(fork->proof_score)
					<< (forked_at ? ", forked at " + std::to_string(forked_at->height) : "")
					<< ", " << (light_mode ? "delta " : "delay ") << (fork->recv_time - (light_mode ? (prev ? prev->recv_time : 0) : verified_vdfs[peak->height].recv_time)) / 1e6 << " sec"
					<< ", took " << elapsed << " sec, " << fork_tree.size() << " blocks";
		}
	}

	if(!is_synced && sync_pos >= sync_peak && sync_pending.empty())
	{
		if(sync_retry < num_sync_retries) {
			log(INFO) << "Reached sync peak at height " << sync_peak - 1;
			sync_pos = sync_peak;
			sync_peak = -1;
			sync_retry++;
		} else if(peak->height + params->finality_delay < sync_peak - 1) {
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

	vdf_point_t vdf_point;
	{
		auto iter = verified_vdfs.find(prev->height + 1);
		if(iter != verified_vdfs.end()) {
			vdf_point = iter->second;
		} else {
			return false;
		}
	}
	// reset state to previous block
	fork_to(prev->hash);

	auto block = Block::create();
	block->prev = prev->hash;
	block->height = prev->height + 1;
	block->time_diff = prev->time_diff;
	block->space_diff = prev->space_diff;
	block->vdf_iters = vdf_point.iters;
	block->vdf_output = vdf_point.output;
	{
		// set new time difficulty
		auto iter = verified_vdfs.find(block->height - params->finality_delay);
		if(iter != verified_vdfs.end()) {
			const int64_t time_delta = (vdf_point.recv_time - iter->second.recv_time) / params->finality_delay;
			if(time_delta > 0) {
				const double gain = 0.1;
				double new_diff = params->block_time * ((block->vdf_iters - prev->vdf_iters) / params->time_diff_constant) / (time_delta * 1e-6);
				new_diff = prev->time_diff * (1 - gain) + new_diff * gain;
				block->time_diff = std::max<int64_t>(new_diff + 0.5, 1);
			}
		}
	}
	{
		// set new space difficulty
		double avg_score = response->score;
		{
			uint32_t counter = 1;
			auto fork = find_fork(state_hash);
			for(uint32_t i = 1; i < params->finality_delay && fork; ++i) {
				avg_score += fork->proof_score;
				counter++;
				fork = fork->prev.lock();
			}
			avg_score /= counter;
		}
		double delta = prev->space_diff * (params->target_score - avg_score);
		delta /= params->target_score;
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

	std::unordered_set<hash_t> invalid;
	std::unordered_set<hash_t> postpone;
	std::unordered_set<txio_key_t> spent;
	std::vector<std::shared_ptr<const Transaction>> tx_list;

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
			tx_list.push_back(tx);
		}
		catch(const std::exception& ex) {
			invalid.insert(entry.first);
			log(WARN) << "TX validation failed with: " << ex.what();
		}
	}
	std::vector<uint64_t> tx_fees(tx_list.size());
	std::vector<uint64_t> tx_cost(tx_list.size());

#pragma omp parallel for
	for(size_t i = 0; i < tx_list.size(); ++i)
	{
		const auto& tx = tx_list[i];
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
			tx_fees[i] = validate(tx);
			tx_cost[i] = tx->calc_min_fee(params);
		}
		catch(const std::exception& ex) {
#pragma omp critical
			invalid.insert(tx->id);
			log(WARN) << "TX validation failed with: " << ex.what();
		}
	}

	uint64_t total_fees = 0;
	uint64_t total_cost = 0;
	for(size_t i = 0; i < tx_list.size(); ++i)
	{
		const auto& tx = tx_list[i];
		if(!invalid.count(tx->id) && !postpone.count(tx->id))
		{
			if(total_cost + tx_cost[i] < params->max_block_cost)
			{
				block->tx_list.push_back(tx);
				total_fees += tx_fees[i];
				total_cost += tx_cost[i];
			}
		}
	}
	for(const auto& id : invalid) {
		tx_pool.erase(id);
	}
	block->finalize();

	FarmerClient farmer(response->farmer_addr);
	const auto block_reward = calc_block_reward(block);
	const auto final_reward = std::max(std::max(block_reward, params->min_reward), uint64_t(total_fees));
	const auto result = farmer.sign_block(block, final_reward);

	if(!result) {
		throw std::logic_error("farmer refused");
	}
	block->tx_base = result->tx_base;
	block->pool_sig = result->pool_sig;
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
	sync_peak = -1;
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
		sync_update = sync_pos;
		log(INFO) << "Starting sync at height " << sync_pos;
	}
	const auto max_pending = !sync_retry ? max_sync_jobs : 1;
	while(sync_pending.size() < max_pending)
	{
		if(sync_pos >= sync_peak) {
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
			if(height < sync_peak) {
				sync_peak = height;
			}
		}
		if(sync_pos - sync_update >= 32) {
			sync_update = sync_pos;
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
				fork->is_verified = true;
			}
			catch(const std::exception& ex) {
				log(WARN) << "Block verification failed for height " << block->height << " with: " << ex.what();
				fork->is_invalid = true;
				fork_to(prev_state);
				throw;
			}
			if(is_synced) {
				publish(block, output_verified_blocks);
			}
			else if(!light_mode) {
				vdf_point_t point;
				point.height = block->height;
				point.iters = block->vdf_iters;
				point.output = block->vdf_output;
				point.recv_time = vnx::get_wall_time_micros();
				verified_vdfs[block->height] = point;
			}
		}
		apply(block);
	}
	return did_fork ? forked_at : nullptr;
}

std::shared_ptr<Node::fork_t> Node::find_best_fork(std::shared_ptr<const BlockHeader> root, const uint32_t* at_height) const
{
	int64_t max_weight = 0;
	std::shared_ptr<fork_t> best_fork;

	if(!root) {
		root = get_root();
	}
	for(const auto& entry : fork_tree)
	{
		const auto& fork = entry.second;
		const auto& block = fork->block;
		if(fork->is_invalid || block->height <= root->height) {
			continue;
		}
		if(at_height && block->height != *at_height) {
			continue;
		}
		int64_t weight = 0;
		if(calc_fork_weight(root, fork, weight))
		{
			if(!best_fork || weight > max_weight || (weight == max_weight && block->hash < best_fork->block->hash))
			{
				best_fork = fork;
				max_weight = weight;
			}
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

bool Node::calc_fork_weight(std::shared_ptr<const BlockHeader> root, std::shared_ptr<fork_t> fork, int64_t& total_weight) const
{
	while(fork) {
		const auto& block = fork->block;
		if(fork->is_invalid || !fork->is_proof_verified || fork->proof_score > params->score_threshold) {
			return false;
		}
		if(fork->has_weak_proof) {
			total_weight -= params->score_threshold;	// count as negative dummy
		} else {
			total_weight += 2 * params->score_threshold - fork->proof_score;
		}
		if(block->prev == root->hash) {
			return true;
		}
		fork = fork->prev.lock();
	}
	return false;
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
		if(!block->pool_sig || !block->pool_sig->verify(proof->pool_key, block->hash)) {
			throw std::logic_error("invalid pool signature");
		}
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
	uint64_t base_spent = 0;
	if(auto tx = std::dynamic_pointer_cast<const Transaction>(block->tx_base)) {
		base_spent = validate(tx, block);
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
				total_fees += validate(tx);
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
	const auto base_allowed = std::max(std::max(base_reward, params->min_reward), uint64_t(total_fees));
	if(base_spent > base_allowed) {
		throw std::logic_error("coin base over-spend");
	}
}

uint64_t Node::validate(std::shared_ptr<const Transaction> tx, std::shared_ptr<const Block> block) const
{
	if(tx->id != tx->calc_hash()) {
		throw std::logic_error("invalid tx id");
	}
	if(block) {
		if(!tx->execute.empty()) {
			throw std::logic_error("coin base cannot have operations");
		}
		if(tx->outputs.size() > params->max_tx_base_out) {
			throw std::logic_error("coin base has too many outputs");
		}
		if(tx->inputs.size() != 1) {
			throw std::logic_error("coin base must have one input");
		}
		const auto& in = tx->inputs[0];
		if(in.prev.txid != hash_t(block->prev) || in.prev.index != 0) {
			throw std::logic_error("invalid coin base input");
		}
	} else {
		if(tx->inputs.empty()) {
			throw std::logic_error("tx without input");
		}
	}
	uint64_t base_amount = 0;
	std::unordered_map<hash_t, uint64_t> amounts;

	if(!block) {
		for(const auto& in : tx->inputs)
		{
			auto iter = utxo_map.find(in.prev);
			if(iter == utxo_map.end()) {
				throw std::logic_error("utxo not found");
			}
			const auto& out = iter->second;

			// verify signature
			const auto solution = tx->get_solution(in.solution);
			if(!solution) {
				throw std::logic_error("missing solution");
			}
			{
				auto iter = contracts.find(out.address);
				if(iter != contracts.end()) {
					if(!iter->second->validate(nullptr, solution, tx->id)) {
						throw std::logic_error("invalid solution");
					}
				}
				else {
					contract::PubKey simple;
					simple.address = out.address;
					if(!simple.validate(nullptr, solution, tx->id)) {
						throw std::logic_error("invalid solution");
					}
				}
			}
			amounts[out.contract] += out.amount;
		}
		for(const auto& op : tx->execute)
		{
			// TODO
		}
	}
	for(const auto& out : tx->outputs)
	{
		if(out.amount == 0) {
			throw std::logic_error("zero tx output");
		}
		if(block) {
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
	if(block) {
		return base_amount;
	}
	const auto fee_amount = amounts[hash_t()];
	const auto fee_needed = tx->calc_min_fee(params);
	if(fee_amount < fee_needed) {
		throw std::logic_error("insufficient fee: " + std::to_string(fee_amount) + " < " + std::to_string(fee_needed));
	}
	return fee_amount;
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
	const auto root = get_root();
	if(root && block->prev != root->hash) {
		return;
	}
	if(change_log.empty()) {
		return;
	}
	const auto log = change_log.front();

	for(const auto& entry : log->utxo_removed) {
		const auto& stxo = entry.second.output;
		stxo_index[entry.first] = entry.second;
		saddr_map.emplace(stxo.address, entry.first);
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

	// purge history
	while(history.size() > max_history) {
		history.erase(history.begin());
	}
	if(!history.empty()) {
		const auto begin = history.begin()->first;
		pending_vdfs.erase(pending_vdfs.begin(), pending_vdfs.lower_bound(begin));
		verified_vdfs.erase(verified_vdfs.begin(), verified_vdfs.lower_bound(begin));
	}
	if(!is_replay) {
		write_block(block);
	}
	fork_tree.erase(block->hash);
	purge_tree();

	publish(block, output_committed_blocks, is_replay ? BLOCKING : 0);
}

void Node::purge_tree()
{
	const auto root = get_root();
	bool repeat = true;
	std::unordered_set<hash_t> purged;
	do {
		repeat = false;
		for(auto iter = fork_tree.begin(); iter != fork_tree.end();)
		{
			const auto& fork = iter->second;
			const auto& block = fork->block;
			if(block->height <= root->height || purged.count(block->prev)
				|| (is_synced && block->height > root->height + 2 * params->commit_delay))
			{
				purged.insert(block->hash);
				iter = fork_tree.erase(iter);
				repeat = true;
			} else {
				iter++;
			}
		}
	} while(repeat);
}

void Node::verify_proof(std::shared_ptr<fork_t> fork, const hash_t& vdf_challenge) const
{
	const auto block = fork->block;
	const auto prev = find_prev_header(block);
	if(!prev) {
		throw std::logic_error("invalid prev");
	}
	const auto diff_block = find_diff_header(block);
	if(!diff_block) {
		throw std::logic_error("cannot verify");
	}
	if(block->vdf_iters != prev->vdf_iters + diff_block->time_diff * params->time_diff_constant) {
		throw std::logic_error("invalid vdf_iters");
	}
	// Note: vdf_output already verified to match vdf_iters

	if(!block->proof) {
		fork->proof_score = params->score_threshold;
		fork->is_proof_verified = true;
		return;
	}
	const auto challenge = get_challenge(block, vdf_challenge);

	fork->proof_score = verify_proof(block->proof, challenge, diff_block->space_diff);
	fork->is_proof_verified = true;

	// check if block has a weak proof
	const auto iter = proof_map.find(challenge);
	if(iter != proof_map.end()) {
		if(fork->proof_score > iter->second->score) {
			fork->has_weak_proof = true;
		}
	}
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

void Node::verify_vdf(std::shared_ptr<const ProofOfTime> proof, const vdf_point_t& prev) const
{
	// check number of segments
	if(proof->segments.size() < params->min_vdf_segments) {
		throw std::logic_error("not enough segments: " + std::to_string(proof->segments.size()));
	}
	if(proof->segments.size() > params->max_vdf_segments) {
		throw std::logic_error("too many segments: " + std::to_string(proof->segments.size()));
	}

	// check delta iters
	if(proof->start != prev.iters) {
		throw std::logic_error("invalid start: " + std::to_string(proof->start) + " != " + std::to_string(prev.iters));
	}
	if(proof->height != prev.height + 1) {
		throw std::logic_error("invalid height: " + std::to_string(proof->height) + " != " + std::to_string(prev.height + 1));
	}
	if(auto root = get_root()) {
		if(auto diff_block = find_diff_header(root, proof->height - root->height)) {
			const auto num_iters = proof->get_num_iters();
			const auto expected = diff_block->time_diff * params->time_diff_constant;
			if(num_iters != expected) {
				throw std::logic_error("wrong delta iters: " + std::to_string(num_iters) + " != " + std::to_string(expected));
			}
		} else {
			throw std::logic_error("cannot verify: missing diff block");
		}
	} else {
		throw std::logic_error("cannot verify: missing root");
	}

	// check proper infusions
	if(proof->start > 0) {
		if(proof->infuse[0].size() != 1) {
			throw std::logic_error("missing infusion on chain 0");
		}
		const auto infused = *proof->infuse[0].begin();
		if(infused.first != proof->start) {
			throw std::logic_error("invalid infusion point on chain 0: must be at start");
		}
		const auto infused_block = find_header(infused.second);
		if(!infused_block) {
			throw std::logic_error("invalid infusion value on chain 0");
		}
		if(auto block = get_header_at(infused_block->height)) {
			if(infused_block->hash != block->hash) {
				throw std::logic_error("invalid block infused on chain 0");
			}
		} else {
			throw std::logic_error("cannot verify");
		}
		if(infused_block->height + std::min(params->finality_delay + 1, proof->height) != proof->height) {
			throw std::logic_error("invalid block height infused on chain 0");
		}
		const bool need_second = infused_block->height >= params->challenge_interval
				&& infused_block->height % params->challenge_interval == 0;

		if(proof->infuse[1].size() != (need_second ? 1 : 0)) {
			throw std::logic_error("wrong number of infusions on chain 1: " + std::to_string(proof->infuse[1].size()));
		}
		if(need_second) {
			if(*proof->infuse[1].begin() != infused) {
				throw std::logic_error("invalid infusion on chain 1");
			}
		}
	}
	vdf_threads->add_task(std::bind(&Node::verify_vdf_task, this, proof, prev));
}

void Node::verify_vdf(std::shared_ptr<const ProofOfTime> proof, const uint32_t chain, const hash_t& begin) const
{
	const auto& segments = proof->segments;
	bool is_valid = !segments.empty();
	size_t invalid_segment = -1;

	std::vector<uint64_t> start_iters(segments.size());
	for(size_t i = 0; i < segments.size(); ++i) {
		if(i == 0) {
			start_iters[i] = proof->start;
		} else {
			start_iters[i] = start_iters[i - 1] + segments[i - 1].num_iters;
		}
	}

#pragma omp parallel for
	for(size_t i = 0; i < segments.size(); ++i)
	{
		if(!is_valid) {
			continue;
		}
		hash_t point;
		if(i > 0) {
			point = segments[i - 1].output[chain];
		} else {
			point = begin;
		}
		{
			auto iter = proof->infuse[chain].find(start_iters[i]);
			if(iter != proof->infuse[chain].end()) {
				point = hash_t(point + iter->second);
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

void Node::verify_vdf_success(std::shared_ptr<const ProofOfTime> proof, const vdf_point_t& prev, const vdf_point_t& point)
{
	verified_vdfs[proof->height] = point;
	vdf_verify_pending = 0;

	const auto elapsed = (vnx::get_wall_time_micros() - point.recv_time) / 1e6;
	if(elapsed > params->block_time) {
		log(WARN) << "VDF verification took more than block interval, unable to keep sync!";
	}
	else if(elapsed > params->block_time - 3) {
		log(WARN) << "VDF verification took longer than recommended: " << elapsed << " sec";
	}
	log(INFO) << "-------------------------------------------------------------------------------";
	log(INFO) << "Verified VDF for height " << proof->height << ", delta = "
				<< (point.recv_time - prev.recv_time) / 1e6 << " sec, took " << elapsed << " sec";

	publish(proof, output_verified_vdfs);

	// add dummy blocks in case no proof is found
	{
		std::vector<std::shared_ptr<const BlockHeader>> prev_blocks;
		if(auto root = get_root()) {
			if(root->height + 1 == proof->height) {
				prev_blocks.push_back(root);
			}
		}
		for(const auto& entry : fork_tree) {
			if(auto block = entry.second->block) {
				if(block->height + 1 == proof->height) {
					prev_blocks.push_back(block);
				}
			}
		}
		for(auto prev : prev_blocks) {
			auto block = Block::create();
			block->prev = prev->hash;
			block->height = proof->height;
			block->time_diff = prev->time_diff;
			block->space_diff = prev->space_diff;
			block->vdf_iters = point.iters;
			block->vdf_output = point.output;
			block->finalize();
			add_block(block);
		}
	}
	update();
}

void Node::verify_vdf_failed(std::shared_ptr<const ProofOfTime> proof)
{
	vdf_verify_pending = 0;
	check_vdfs();
}

void Node::verify_vdf_task(std::shared_ptr<const ProofOfTime> proof, const vdf_point_t& prev) const noexcept
{
	std::lock_guard<std::mutex> lock(vdf_mutex);

	const auto time_begin = vnx::get_wall_time_micros();
	try {
		for(int i = 0; i < 2; ++i) {
			if(auto engine = opencl_vdf[i]) {
				engine->compute(proof, i, prev.output[i]);
			}
		}
		for(int i = 0; i < 2; ++i) {
			if(auto engine = opencl_vdf[i]) {
				engine->verify(proof, i);
			} else {
				verify_vdf(proof, i, prev.output[i]);
			}
		}
		vdf_point_t point;
		point.height = proof->height;
		point.iters = proof->start + proof->get_num_iters();
		point.output[0] = proof->get_output(0);
		point.output[1] = proof->get_output(1);
		point.recv_time = time_begin;

		add_task([this, proof, prev, point]() {
			((Node*)this)->verify_vdf_success(proof, prev, point);
		});
	}
	catch(const std::exception& ex) {
		add_task([this, proof]() {
			((Node*)this)->verify_vdf_failed(proof);
		});
		log(WARN) << "VDF verification for height " << proof->height << " failed with: " << ex.what();
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
		apply(block, tx, 0, *log);
	}
	for(size_t i = 0; i < block->tx_list.size(); ++i) {
		if(auto tx = std::dynamic_pointer_cast<const Transaction>(block->tx_list[i])) {
			apply(block, tx, 1 + i, *log);
		}
	}
	state_hash = block->hash;
	change_log.push_back(log);
}

void Node::apply(std::shared_ptr<const Block> block, std::shared_ptr<const Transaction> tx, size_t index, change_log_t& log) noexcept
{
	for(size_t i = 0; i < tx->inputs.size(); ++i)
	{
		auto iter = utxo_map.find(tx->inputs[i].prev);
		if(iter != utxo_map.end()) {
			const auto key = txio_key_t::create_ex(tx->id, i);
			const auto& stxo = iter->second;
			log.utxo_removed.emplace(iter->first, utxo_entry_t::create_ex(key, stxo));
			taddr_map[stxo.address].erase(iter->first);
			utxo_map.erase(iter);
		}
	}
	for(size_t i = 0; i < tx->outputs.size(); ++i)
	{
		const auto key = txio_key_t::create_ex(tx->id, i);
		const auto utxo = utxo_t::create_ex(tx->outputs[i], block->height);
		utxo_map[key] = utxo;
		taddr_map[utxo.address].insert(key);
		log.utxo_added.emplace(key, utxo);
	}
	tx_map[tx->id] = block->height;
	log.tx_added.push_back(tx->id);
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
		const auto& utxo = entry.second.output;
		utxo_map.emplace(entry.first, utxo);
		taddr_map[utxo.address].insert(entry.first);
	}
	for(const auto& txid : log->tx_added) {
		tx_map.erase(txid);
	}
	change_log.pop_back();
	state_hash = log->prev_state;
	return true;
}

std::shared_ptr<const BlockHeader> Node::get_root() const
{
	if(history.empty()) {
		return nullptr;
	}
	return (--history.end())->second;
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
	if(distance > 1 && (block->height >= distance || clamped))
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
	uint32_t height = block->height + offset;
	height -= (height % params->challenge_interval);
	if(auto prev = find_prev_header(block, (block->height + params->challenge_interval) - height, true)) {
		return prev;
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
				if(auto tx = std::dynamic_pointer_cast<const Transaction>(header->tx_base)) {
					tx_index[tx->id] = std::make_pair(offset, header->height);
				}
				block_index[header->height] = std::make_pair(offset, header->hash);
			}
			auto block = Block::create();
			block->BlockHeader::operator=(*header);
			while(true) {
				const auto offset = in.get_input_pos();
				if(auto value = vnx::read(in)) {
					if(auto tx = std::dynamic_pointer_cast<TransactionBase>(value)) {
						block->tx_list.push_back(tx);
						if(is_replay && std::dynamic_pointer_cast<Transaction>(value)) {
							tx_index[tx->id] = std::make_pair(offset, block->height);
						}
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
		tx_index[tx->id] = std::make_pair(offset, block->height);
	}
	block_index[block->height] = std::make_pair(offset, block->hash);
	vnx::write(out, block->get_header());

	for(const auto& tx : block->tx_list) {
		const auto offset = out.get_output_pos();
		if(std::dynamic_pointer_cast<const Transaction>(tx)) {
			tx_index[tx->id] = std::make_pair(offset, block->height);
		}
		vnx::write(out, tx);
	}
	vnx::write(out, nullptr);
	block_chain->flush();
}


} // mmx
