/*
 * Node_update.cpp
 *
 *  Created on: Jan 20, 2022
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/FarmerClient.hxx>
#include <mmx/Challenge.hxx>
#include <mmx/IntervalRequest.hxx>
#include <mmx/ProofOfSpaceOG.hxx>
#include <mmx/ProofOfSpaceNFT.hxx>
#include <mmx/contract/VirtualPlot.hxx>
#include <mmx/operation/Execute.hxx>
#include <mmx/operation/Deposit.hxx>
#include <mmx/utils.h>
#include <mmx/helpers.h>

#include <vnx/vnx.h>
#include <random>


namespace mmx {

void Node::verify_vdfs()
{
	if(vdf_queue.empty() || vdf_verify_pending.size() >= vdf_verify_max_pending) {
		return;
	}
	const auto time_now = vnx::get_wall_time_millis();
	const auto vdf_timeout = 10 * params->block_interval_ms;
	const auto root = get_root();

	struct vdf_fork_t {
		uint32_t height = 0;
		int64_t trust = 0;
		int64_t recv_time = 0;
		std::shared_ptr<const ProofOfTime> proof;
	};
	std::vector<std::shared_ptr<vdf_fork_t>> try_now;
	std::vector<std::pair<std::shared_ptr<const ProofOfTime>, int64_t>> try_again;

	std::unordered_map<hash_t, uint32_t> height_map;	// [output => vdf_height] based on verified proofs and VDFs only
	for(const auto& entry : fork_index) {
		const auto& fork = entry.second;
		if(fork->is_all_proof_verified) {
			height_map[fork->block->vdf_output] = fork->block->vdf_height;
		}
	}
	height_map[root->vdf_output] = root->vdf_height;

	for(const auto& entry : vdf_index) {
		const auto& point = entry.second;
		if(auto height = find_value(height_map, point->input)) {
			height_map[point->output] = (*height) + 1;
		}
	}

	for(const auto& entry : vdf_queue)
	{
		const auto& proof = entry.first;
		if(proof->start < root->vdf_iters) {
			continue;		// too old
		}
		if(time_now - entry.second > vdf_timeout) {
			continue;		// timeout
		}
		if(find_vdf_point(proof->input, proof->get_output())) {
			continue;		// already verified
		}
		if(!get_header(proof->prev) || !height_map.count(proof->input)) {
			try_again.push_back(entry);	// wait for previous block / VDF
			continue;
		}
		auto out = std::make_shared<vdf_fork_t>();
		out->height = find_value(height_map, proof->input, 0);
		out->trust = find_value(timelord_trust, proof->timelord_key, 0);
		out->recv_time = entry.second;
		out->proof = proof;
		try_now.emplace_back(out);
	}

	std::sort(try_now.begin(), try_now.end(),
		[]( const std::shared_ptr<vdf_fork_t>& L, const std::shared_ptr<vdf_fork_t>& R) -> bool {
			return std::make_pair(L->height, L->trust) > std::make_pair(R->height, R->trust);
		});

	for(const auto& fork : try_now)
	{
		const auto& proof = fork->proof;
		if(vdf_verify_pending.count(proof->hash)) {
			continue;		// duplicate
		}
		if(vdf_verify_pending.size() >= vdf_verify_max_pending) {
			try_again.emplace_back(proof, fork->recv_time);
			continue;
		}
		try {
			verify_vdf(proof, fork->recv_time);
		} catch(const std::exception& ex) {
			log(WARN) << "VDF static verification for height " << proof->vdf_height << " failed with: " << ex.what();
		}
	}
	vdf_queue = std::move(try_again);
}

void Node::verify_votes()
{
	const auto time_now = vnx::get_wall_time_millis();
	const auto vote_timeout = 3 * params->block_interval_ms;

	std::vector<std::pair<std::shared_ptr<const ValidatorVote>, int64_t>> try_again;

	for(const auto& entry : vote_queue) {
		if(time_now - entry.second > vote_timeout) {
			continue;		// timeout
		}
		const auto& vote = entry.first;
		// Note: is_valid() already checked in handle()
		try {
			if(auto fork = find_fork(vote->hash)) {
				if(fork->is_proof_verified) {
					auto iter = fork->validators.find(vote->farmer_key);
					if(iter != fork->validators.end()) {
						if(!iter->second) {
							iter->second = true;
							fork->votes++;
							publish(vote, output_verified_votes);

							const auto delay_ms = entry.second - fork->recv_time;
							log(DEBUG) << "Received vote for block at height " << fork->block->height
									<< ", total is now " << fork->votes << ", delay " << delay_ms / 1e3 << " sec";
						}
						continue;
					} else {
						throw std::logic_error("farmer is not a validator for this block");
					}
				}
			}
			try_again.push_back(entry);
		}
		catch(const std::exception& ex) {
			log(DEBUG) << "Got invalid vote for block " << vote->hash << ": " << ex.what();
		}
	}
	vote_queue = std::move(try_again);
}

void Node::verify_proofs()
{
	const auto time_now = vnx::get_wall_time_millis();
	const auto proof_timeout = 10 * params->block_interval_ms;
	const auto vdf_height = get_vdf_height();

	std::mutex mutex;
	std::vector<std::pair<std::shared_ptr<const ProofResponse>, int64_t>> try_again;

	for(const auto& entry : proof_queue) {
		const auto& res = entry.first;
		if(time_now - entry.second > proof_timeout) {
			continue;		// timeout
		}
		if(res->vdf_height >= vdf_height + params->challenge_delay) {
			try_again.push_back(entry);		// wait for challenge to confirm
			continue;
		}
		threads->add_task([this, res, &mutex, &try_again]() {
			try {
				verify(res);
				std::lock_guard<std::mutex> lock(mutex);
				add_proof(res->proof, res->vdf_height, res->farmer_addr);
				log(DEBUG) << "Got proof for VDF height " << res->vdf_height << " with score " << res->proof->score;
			}
			catch(const std::exception& ex) {
				log(WARN) << "Got invalid proof for VDF height " << res->vdf_height << ": " << ex.what();
			}
		});
	}
	threads->sync();

	proof_queue = std::move(try_again);
}

void Node::verify_block_proofs()
{
	std::mutex mutex;
	const auto root = get_root();

	for(const auto& entry : fork_index)
	{
		const auto& fork = entry.second;
		const auto& block = fork->block;
		if(!fork->prev.lock()) {
			fork->prev = find_fork(block->prev);
		}
		if(auto prev = fork->prev.lock()) {
			if(prev->is_invalid) {
				fork->is_invalid = true;
			}
			fork->is_connected = prev->is_connected;
		} else if(block->prev == root->hash) {
			fork->is_connected = true;
		}
		if(!fork->is_connected || fork->is_invalid || fork->is_proof_verified) {
			continue;
		}
		if(!fork->is_vdf_verified) {
			const auto vdf_points = find_vdf_points(block);
			if(vdf_points.size()) {
				fork->vdf_points = vdf_points;
				fork->is_vdf_verified = true;
			}
		}
		// we don't verify VDFs here if still syncing
		// instead it's done later when needed (competing forks)
		if(fork->is_vdf_verified || !is_synced)
		{
			threads->add_task([this, fork, &mutex]() {
				const auto& block = fork->block;
				try {
					verify_proof(fork);

					if(auto proof = block->proof[0]) {
						std::lock_guard<std::mutex> lock(mutex);
						add_proof(proof, block->vdf_height, vnx::Hash64());
					}
					for(auto key : get_validators(block)) {
						fork->validators[key] = false;
					}
				} catch(const std::exception& ex) {
					fork->is_invalid = true;
					log(WARN) << "Proof verification failed for a block at height " << block->height << ": " << ex.what();
				}
			});
		}
	}
	threads->sync();
}

void Node::update()
{
	std::unique_lock lock(db_mutex);

	const auto time_begin = vnx::get_wall_time_millis();
	update_pending = false;

	verify_proofs();

	verify_block_proofs();

	verify_votes();		// after block proofs

	const auto prev_peak = get_peak();
	std::shared_ptr<const BlockHeader> forked_at;

	// choose best fork
	while(vnx::do_run())
	{
		forked_at = nullptr;
		const auto best_fork = find_best_fork();

		if(!best_fork || best_fork->block->hash == state_hash) {
			break;	// no change
		}

		// verify and apply new fork
		try {
			forked_at = fork_to(best_fork);
		}
		catch(const std::exception& ex) {
			best_fork->is_invalid = true;
			log(WARN) << "Forking to height " << best_fork->block->height << " failed with: " << ex.what();
			continue;	// try again
		}
		break;
	}

	verify_vdfs();	// after peak update

	const auto peak = get_peak();
	if(!peak) {
		log(WARN) << "Have no peak!";
		return;
	}
	const auto fork = find_fork(peak->hash);
	{
		// commit to disk
		const auto fork_line = get_fork_line();

		for(size_t i = 0; i + params->commit_delay < fork_line.size(); ++i)
		{
			const auto& fork = fork_line[i];
			const auto& block = fork->block;
			if(!fork->is_vdf_verified) {
				break;	// wait for VDF verify
			}
			commit(block);
		}
	}
	const auto root = get_root();
	const auto now_ms = vnx::get_wall_time_millis();
	const auto elapsed = (now_ms - time_begin) / 1e3;

	if(!prev_peak || peak->hash != prev_peak->hash)
	{
		if(fork) {
			std::stringstream msg;
			msg << u8"\U0001F4BE New peak at height " << peak->height << " / " << peak->vdf_height << " with score ";
			if(peak->proof.size()) {
				msg << peak->proof[0]->score;
			} else {
				msg << "N/A";
			}
			if(is_synced) {
				if(forked_at) {
					msg << ", forked at " << forked_at->height;
				}
				if(fork->vdf_points.size()) {
					msg << ", delay " << (fork->recv_time - fork->vdf_points.back()->recv_time) / 1e3 << " sec";
				}
			} else {
				msg << ", " << sync_pending.size() << " pending";
			}
			msg << ", took " << elapsed << " sec";
			log(INFO) << msg.str();
		}
		if(forked_at && prev_peak) {
			const auto depth = prev_peak->height - forked_at->height;
			if(depth > 1) {
				log(WARN) << "Forked " << depth << " blocks deep at height " << peak->height;
			}
		}
		stuck_timer->reset();
	}

	if(!is_synced && sync_peak && sync_pending.empty())
	{
		if(sync_retry < num_sync_retries) {
			if(now_ms - sync_finish_ms > params->block_interval_ms / 2) {
				sync_pos = *sync_peak - 1;
				sync_peak = nullptr;
				sync_finish_ms = now_ms;
				sync_retry++;
				log(INFO) << "Reached sync peak at height " << sync_pos;
			}
		} else {
			is_synced = true;
			on_sync_done(peak->height);
		}
	}
	if(!is_synced) {
		sync_more();
		update_timer->reset();
		return;
	}

	{
		const auto vdf_points = find_next_vdf_points(peak);
		const auto vdf_output = vdf_points.empty() ? peak->vdf_output : vdf_points.back()->output;

		const auto vdf_advance = std::min<uint32_t>(
				vdf_points.size() + params->infuse_delay, params->max_vdf_count);

		// request next VDF proofs
		auto vdf_iters = peak->vdf_iters;
		for(uint32_t i = 0; i <= vdf_advance; ++i)
		{
			uint64_t num_iters = 0;
			const auto infuse = get_infusion(peak, i, num_iters);

			auto req = IntervalRequest::create();
			req->vdf_height = peak->vdf_height + i + 1;
			req->start = vdf_iters;
			req->end = vdf_iters + num_iters;
			req->infuse = infuse;

			if(i == vdf_points.size()) {
				req->input = vdf_output;	// for start or restart
			}
			publish(req, output_interval_request);

			vdf_iters += num_iters;
		}

		const auto challenge_advance = std::min<uint32_t>(
				vdf_points.size() + params->challenge_delay - 1, params->max_vdf_count);

		// publish challenges
		for(uint32_t i = 1; i <= challenge_advance; ++i)
		{
			uint64_t space_diff = 0;
			const auto challenge = get_challenge(peak, i, space_diff);

			auto value = Challenge::create();
			value->vdf_height = peak->vdf_height + i;
			value->challenge = challenge;
			value->difficulty = space_diff;

			publish(value, output_challenges);
		}
	}

	// vote for new peak
	if(fork && peak->height && voted_blocks.count(peak->prev) == 0)
	{
		// make sure peak is from best proof seen
		const auto proof = find_best_proof(peak->proof[0]->challenge);
		if(proof && peak->proof_hash == proof->hash)
		{
			for(const auto& entry : fork->validators) {
				auto iter = farmer_keys.find(entry.first);
				if(iter != farmer_keys.end()) {
					auto vote = ValidatorVote::create();
					vote->hash = peak->hash;
					vote->farmer_key = iter->first;
					try {
						FarmerClient farmer(iter->second);
						vote->farmer_sig = farmer.sign_vote(vote);
						vote->content_hash = vote->calc_content_hash();
						publish(vote, output_votes);
						publish(vote, output_verified_votes);
						log(INFO) << "Voted for block at height " << peak->height << ": " << vote->hash;
					}
					catch(const std::exception& ex) {
						log(WARN) << "Failed to sign vote for height " << peak->height << ": " << ex.what();
					}
					voted_blocks[peak->prev] = vote->hash;
				}
			}
		}
	}

	// try to replace current peak
	// in case another farmer made a block when they shouldn't
	if(peak->height) {
		const auto challenge = peak->proof[0]->challenge;

		if(auto proof = find_best_proof(challenge))
		{
			if(vnx::get_pipe(proof->farmer_mac))
			{
				if(!created_blocks.count(proof->hash))
				{
					if(auto prev = find_prev(peak))
					{
						const auto vdf_points = find_next_vdf_points(prev);
						if(vdf_points.size()) {
							try {
								if(auto block = make_block(prev, vdf_points, challenge)) {
									add_block(block);
								}
							} catch(const std::exception& ex) {
								log(WARN) << "Failed to create block at height " << peak->height << ": " << ex.what();
							}
							fork_to(peak->hash);
						}
					}
				}
			}
		}
	}

	// try to extend peak
	{
		const auto vdf_points = find_next_vdf_points(peak);
		if(vdf_points.size())
		{
			hash_t challenge;
			uint64_t space_diff = 0;
			if(find_challenge(peak, vdf_points.size(), challenge, space_diff))
			{
				if(auto proof = find_best_proof(challenge))
				{
					if(vnx::get_pipe(proof->farmer_mac))
					{
						if(!created_blocks.count(proof->hash)) {
							try {
								if(auto block = make_block(peak, vdf_points, challenge)) {
									add_block(block);
								}
							} catch(const std::exception& ex) {
								log(WARN) << "Failed to create block at height " << peak->height + 1 << ": " << ex.what();
							}
							fork_to(peak->hash);
						}
					}
				}
			}
		}
	}
}

void Node::on_sync_done(const uint32_t height)
{
	log(INFO) << "Finished sync at height " << height;
	synced_since = height;
	update_control();
}

bool Node::tx_pool_update(const tx_pool_t& entry, const bool force_add)
{
	if(entry.is_skipped) {
		throw std::logic_error("tx_pool_update(): entry is_skipped");
	}
	if(const auto& tx = entry.tx) {
		if(tx->sender) {
			const auto& sender = *tx->sender;
			const auto iter = tx_pool.find(tx->id);
			const auto fees = tx_pool_fees.find(sender);
			auto new_total = (fees != tx_pool_fees.end()) ? fees->second : 0;
			if(iter != tx_pool.end()) {
				new_total -= iter->second.fee;
			}
			new_total += entry.fee;

			if(!force_add && new_total > get_balance(sender, addr_t())) {
				return false;
			}
			if(fees != tx_pool_fees.end()) {
				fees->second = new_total;
			} else {
				tx_pool_fees[sender] = new_total;
			}
			if(iter != tx_pool.end()) {
				iter->second = entry;
			} else {
				std::lock_guard<std::mutex> lock(mutex);

				for(const auto& in : tx->get_inputs()) {
					tx_pool_index.emplace(std::make_pair(in.address, tx->id), tx);
				}
				for(const auto& out : tx->get_outputs()) {
					tx_pool_index.emplace(std::make_pair(out.address, tx->id), tx);
					if(out.memo) {
						tx_pool_index.emplace(std::make_pair(hash_t(out.address + (*out.memo)), tx->id), tx);
					}
				}
				tx_pool[tx->id] = entry;
			}
			return true;
		}
	}
	return false;
}

void Node::tx_pool_erase(const hash_t& txid)
{
	const auto iter = tx_pool.find(txid);
	if(iter != tx_pool.end()) {
		if(const auto& tx = iter->second.tx) {
			if(const auto& sender = tx->sender) {
				const auto iter2 = tx_pool_fees.find(*sender);
				if(iter2 != tx_pool_fees.end()) {
					if((iter2->second -= iter->second.fee) == 0) {
						tx_pool_fees.erase(iter2);
					}
				}
			}
			std::lock_guard<std::mutex> lock(mutex);

			for(const auto& in : tx->get_inputs()) {
				tx_pool_index.erase(std::make_pair(in.address, tx->id));
			}
			for(const auto& out : tx->get_outputs()) {
				tx_pool_index.erase(std::make_pair(out.address, tx->id));
				if(out.memo) {
					tx_pool_index.erase(std::make_pair(hash_t(out.address + (*out.memo)), tx->id));
				}
			}
		}
		tx_pool.erase(iter);
	}
}

void Node::purge_tx_pool()
{
	const auto time_begin = vnx::get_wall_time_millis();

	std::vector<tx_pool_t> all_tx;
	all_tx.reserve(tx_pool.size());
	for(const auto& entry : tx_pool) {
		all_tx.push_back(entry.second);
	}

	// sort transactions by fee ratio
	std::sort(all_tx.begin(), all_tx.end(),
		[](const tx_pool_t& lhs, const tx_pool_t& rhs) -> bool {
			const auto L = lhs.tx->fee_ratio;
			const auto R = rhs.tx->fee_ratio;
			return (L == R) ? lhs.luck < rhs.luck : L > R;
		});

	size_t num_purged = 0;
	uint64_t total_pool_size = 0;
	std::unordered_map<addr_t, std::pair<uint64_t, uint64_t>> sender_map;	// [sender => [balance, total fee]]

	const uint64_t max_pool_size = uint64_t(max_tx_pool) * params->max_block_size;

	// purge transactions from pool if overflowing
	for(const auto& entry : all_tx) {
		const auto& tx = entry.tx;
		bool fee_overspend = false;
		if(tx->sender) {
			const auto sender = *tx->sender;
			const auto iter = sender_map.emplace(sender, std::make_pair(0, 0));
			auto& balance = iter.first->second.first;
			auto& total_fee = iter.first->second.second;
			if(iter.second) {
				balance = get_balance(sender, addr_t());
			}
			total_fee += entry.fee;
			fee_overspend = total_fee > balance;
		}
		if(!fee_overspend) {
			total_pool_size += tx->static_cost;
		}
		if(total_pool_size > max_pool_size || fee_overspend) {
			tx_pool_erase(tx->id);
			num_purged++;
		} else {
			min_pool_fee_ratio = tx->fee_ratio;
		}
	}
	if(total_pool_size < 9 * max_pool_size / 10) {
		min_pool_fee_ratio = 0;
	}
	if(total_pool_size || num_purged) {
		log(INFO) << uint64_t((total_pool_size * 10000) / max_pool_size) / 100. << " % mem pool, "
				<< min_pool_fee_ratio / 1024. << " min fee ratio, " << num_purged << " purged, took "
				<< (vnx::get_wall_time_millis() - time_begin) / 1e3 << " sec";
	}
}

void Node::validate_new()
{
	const auto peak = get_peak();
	if(!peak || !is_synced) {
		return;
	}
	std::default_random_engine luck_gen(vnx::rand64());

	// select non-overlapping set
	std::vector<tx_pool_t> tx_list;
	std::unordered_set<hash_t> tx_set;
	tx_list.reserve(max_tx_queue);
	tx_set.reserve(max_tx_queue);
	for(const auto& entry : tx_queue) {
		if(const auto& tx = entry.second) {
			if(tx_set.insert(tx->id).second) {
				tx_pool_t tmp;
				tmp.tx = tx;
				tmp.luck = luck_gen();
				tx_list.push_back(tmp);
			}
		}
	}
	auto context = new_exec_context(peak->height + 1);

	// prepare synchronization
	for(const auto& entry : tx_list) {
		prepare_context(context, entry.tx);
	}
	const auto deadline_ms = vnx::get_wall_time_millis() + validate_interval_ms;

	// verify transactions in parallel
	for(auto& entry : tx_list) {
		threads->add_task([this, &entry, context, deadline_ms]() {
			if(vnx::get_wall_time_millis() > deadline_ms) {
				entry.is_skipped = true;
				return;
			}
			entry.is_valid = false;

			auto& tx = entry.tx;
			if(tx->exec_result) {
				auto tmp = vnx::clone(tx);
				tmp->reset(params);
				tx = tmp;
			}
			context->wait(tx->id);
			try {
				if(auto result = validate(tx, context)) {
					entry.cost = result->total_cost;
					entry.fee = result->total_fee;
					entry.is_valid = true;
				}
			} catch(const std::exception& ex) {
				if(show_warnings) {
					log(WARN) << "TX validation failed with: " << ex.what() << " (" << tx->id << ")";
				}
			} catch(...) {
				// ignore
			}
			context->signal(tx->id);
		});
	}
	threads->sync();

	// update tx pool
	for(const auto& entry : tx_list) {
		if(!entry.is_skipped) {
			const auto& tx = entry.tx;
			if(entry.is_valid) {
				if(tx_pool_update(entry)) {
					publish(tx, output_verified_transactions);
				}
			}
			tx_queue.erase(tx->content_hash);
		}
	}
}

std::vector<Node::tx_pool_t> Node::validate_for_block(const int64_t deadline_ms)
{
	const auto peak = get_peak();
	const auto context = new_exec_context(peak->height + 1);

	std::vector<tx_pool_t> all_tx;
	all_tx.reserve(tx_pool.size());
	for(const auto& entry : tx_pool) {
		const auto& pool = entry.second;
		if(check_tx_inclusion(pool.tx->id, context->height)) {
			all_tx.push_back(pool);
		}
	}

	// sort transactions by fee ratio
	std::sort(all_tx.begin(), all_tx.end(),
		[](const tx_pool_t& lhs, const tx_pool_t& rhs) -> bool {
			return lhs.tx->fee_ratio > rhs.tx->fee_ratio;
		});

	std::vector<tx_pool_t> tx_list;
	uint64_t total_verify_cost = 0;

	// select transactions to verify
	for(const auto& entry : all_tx) {
		if(total_verify_cost + entry.cost <= params->max_block_cost) {
			tx_list.push_back(entry);
			total_verify_cost += entry.cost;
		}
	}

	// prepare synchronization
	for(const auto& entry : tx_list) {
		prepare_context(context, entry.tx);
	}

	// verify transactions in parallel
	for(auto& entry : tx_list) {
		threads->add_task([this, &entry, context, deadline_ms]() {
			if(vnx::get_wall_time_millis() > deadline_ms) {
				entry.is_skipped = true;
				return;
			}
			entry.is_valid = false;

			auto& tx = entry.tx;
			if(tx->exec_result) {
				auto tmp = vnx::clone(tx);
				tmp->reset(params);
				tx = tmp;
			}
			context->wait(tx->id);
			try {
				auto result = validate(tx, context);
				if(!result) {
					throw std::logic_error("!result");
				}
				auto tmp = vnx::clone(tx);
				tmp->update(*result, params);
				tx = tmp;
				entry.cost = result->total_cost;
				entry.fee = result->total_fee;
				entry.is_valid = true;
			} catch(const std::exception& ex) {
				if(show_warnings) {
					log(WARN) << "TX validation failed with: " << ex.what() << " (" << tx->id << ")";
				}
			} catch(...) {
				// ignore
			}
			context->signal(tx->id);
		});
	}
	threads->sync();

	uint32_t num_skipped = 0;
	uint64_t total_cost = 0;
	uint64_t static_cost = 0;
	std::vector<tx_pool_t> result;
	balance_cache_t balance_cache(&balance_table);

	// select final set of transactions
	for(auto& entry : tx_list)
	{
		if(entry.is_skipped) {
			num_skipped++;
			continue;
		}
		if(!entry.is_valid) {
			tx_pool_erase(entry.tx->id);
			continue;
		}
		const auto tx = entry.tx;

		if(static_cost + tx->static_cost > params->max_block_size || total_cost + entry.cost > params->max_block_cost) {
			continue;
		}
		bool passed = true;
		balance_cache_t tmp_cache(&balance_cache);
		{
			const auto balance = tmp_cache.find(*tx->sender, addr_t());
			if(balance && entry.fee <= *balance) {
				*balance -= entry.fee;
			} else {
				passed = false;
			}
		}
		if(!tx->exec_result->did_fail) {
			for(const auto& in : tx->inputs) {
				const auto balance = tmp_cache.find(in.address, in.contract);
				if(balance && in.amount <= *balance) {
					*balance -= in.amount;
				} else {
					passed = false;
				}
			}
		}
		if(!passed) {
			continue;
		}
		balance_cache.apply(tmp_cache);

		total_cost += entry.cost;
		static_cost += tx->static_cost;
		result.push_back(entry);
	}
	if(num_skipped) {
		log(WARN) << "Skipped " << num_skipped << " transactions due to block creation deadline";
	}

	const uint32_t N = params->min_fee_ratio.size();
	if(N == 0) {
		return result;
	}
	std::vector<uint64_t> band_avail(N);
	for(uint64_t i = 0; i < N; ++i) {
		band_avail[i] = ((i + 1) * params->max_block_size) / N - (i * params->max_block_size) / N;
	}
	uint32_t i = N - 1;
	std::vector<tx_pool_t> out;
	for(const auto& entry : result) {
		const auto& tx = entry.tx;
		while(i && (!band_avail[i] || tx->fee_ratio < params->min_fee_ratio[i] * 1024)) {
			i--;
		}
		if(tx->static_cost > band_avail[i]) {
			if(i) {
				// we assume band size is always >= max_tx_cost (so band_avail never goes negative here)
				band_avail[i - 1] -= tx->static_cost - band_avail[i];
				band_avail[i] = 0;
			} else {
				continue;	// no room left for this tx
			}
		} else {
			band_avail[i] -= tx->static_cost;
		}
		out.push_back(entry);
	}
	return out;
}

std::shared_ptr<const Block> Node::make_block(
		std::shared_ptr<const BlockHeader> prev, std::vector<std::shared_ptr<const VDF_Point>> vdf_points, const hash_t& challenge)
{
	if(vdf_points.empty()) {
		return nullptr;
	}
	const auto proof = proof_map[challenge];
	if(proof.empty()) {
		return nullptr;
	}
	const auto time_begin = vnx::get_wall_time_millis();

	// reset state to previous block
	fork_to(prev->hash);

	auto block = Block::create();
	block->prev = prev->hash;
	block->height = prev->height + 1;
	block->time_diff = prev->time_diff;
	block->vdf_count = vdf_points.size();
	block->vdf_height = prev->vdf_height + block->vdf_count;
	block->reward_vote = reward_vote;
	block->project_addr = prev->project_addr;
	block->txfee_buffer = calc_new_txfee_buffer(params, prev);

	block->vdf_iters = prev->vdf_iters;
	for(auto point : vdf_points) {
		block->vdf_iters += point->num_iters;
		block->vdf_output = point->output;
		block->vdf_reward_addr.push_back(point->reward_addr);
	}

	for(const auto& entry : proof) {
		block->proof.push_back(entry.proof);
	}
	block->proof_hash = proof[0].hash;

	block->challenge = calc_next_challenge(params, prev->challenge, block->vdf_count, block->proof_hash, block->is_space_fork);

	if(block->height % params->vdf_reward_interval == 0) {
		block->vdf_reward_payout = get_vdf_reward_winner(block);
	}

	if(auto nft = std::dynamic_pointer_cast<const ProofOfSpaceNFT>(block->proof[0])) {
		block->reward_contract = nft->contract;
	}

	if(auto contract = block->reward_contract) {
		const addr_t any = hash_t::random();
		const auto address = get_plot_nft_target(*contract, any);
		if(address != any) {
			block->reward_addr = address;
		}
	}

	if(auto ref = find_prev(prev, 100))
	{
		// set new time diff
		const auto delta_ms = prev->time_stamp - ref->time_stamp;
		const auto delta_blocks = prev->height - ref->height;
		const auto factor = double(params->block_interval_ms * delta_blocks) / delta_ms;

		const auto delay = params->commit_delay + params->infuse_delay;
		const auto begin = find_prev(ref, delay, true);
		const auto end =   find_prev(prev, delay, true);

		if(begin && end) {
			const auto avg_diff = (begin->time_diff + end->time_diff) / 2;
			block->time_diff = std::max<int64_t>(avg_diff * factor + 0.5, params->time_diff_divider);
		}

		// limit time diff update
		const auto max_update = std::max<uint64_t>(prev->time_diff >> params->max_diff_adjust, 1);
		if(prev->time_diff > max_update) {
			block->time_diff = std::max(block->time_diff, prev->time_diff - max_update);
		}
		block->time_diff = std::min(block->time_diff, prev->time_diff + max_update);
	}
	{
		// set time stamp
		auto delta_ms = time_begin - prev->time_stamp;
		delta_ms = std::min(delta_ms, block->vdf_count * params->block_interval_ms * 2);
		delta_ms = std::max(delta_ms, block->vdf_count * params->block_interval_ms / 10);
		block->time_stamp = prev->time_stamp + delta_ms;
	}

	uint64_t total_fees = 0;
	if(block->height >= params->transaction_activation)
	{
		const auto deadline = vnx::get_wall_time_millis() + params->block_interval_ms / 2;
		const auto tx_list = validate_for_block(deadline);
		// select transactions
		for(const auto& entry : tx_list) {
			block->tx_list.push_back(entry.tx);
			total_fees += entry.fee;
		}
	}

	block->weight = calc_block_weight(params, block, prev);
	block->total_weight = prev->total_weight + block->weight;
	block->reward_amount = calc_block_reward(block, total_fees);
	block->set_space_diff(params, prev);
	block->set_base_reward(params, prev);
	block->finalize();

	FarmerClient farmer(proof[0].farmer_mac);

	const auto result = farmer.sign_block(block);
	if(!result) {
		log(WARN) << "Farmer refused to sign block at height " << block->height;
		return nullptr;
	}
	block->BlockHeader::operator=(*result);

	created_blocks[block->proof_hash] = block->hash;

	const auto elapsed = (vnx::get_wall_time_millis() - time_begin) / 1e3;
	log(INFO) << u8"\U0001F911 Created block at height " << block->height << " with: ntx = " << block->tx_count
			<< ", score = " << block->proof[0]->score << ", reward = " << to_value(block->reward_amount, params) << " MMX"
			<< ", fees = " << to_value(total_fees, params) << " MMX" << ", took " << elapsed << " sec";
	return block;
}


} // mmx
