/*
 * TimeLord.cpp
 *
 *  Created on: Dec 6, 2021
 *      Author: mad
 */

#include <mmx/TimeLord.h>
#include <mmx/ProofOfTime.hxx>
#include <mmx/WalletClient.hxx>

#include <vnx/vnx.h>


namespace mmx {

TimeLord::TimeLord(const std::string& _vnx_name)
	:	TimeLordBase(_vnx_name)
{
}

void TimeLord::init()
{
	subscribe(input_infuse, 1000);
	subscribe(input_request, 1000);

	vnx::open_pipe(vnx_name, this, 1000);
}

void TimeLord::main()
{
	try {
		if(!reward_addr) {
			WalletClient wallet(wallet_server);
			reward_addr = wallet.get_address(0, 0);
		}
	}
	catch(const std::exception& ex) {
		log(WARN) << "Failed to get address from wallet: " << ex.what();
	}
	if(reward_addr) {
		log(INFO) << "Reward address: " << reward_addr->to_string();
	} else {
		log(WARN) << "No reward address set!";
	}
	{
		vnx::File file(storage_path + "timelord_sk.dat");
		if(file.exists()) {
			try {
				file.open("rb");
				vnx::read_generic(file.in, timelord_sk);
				file.close();
			}
			catch(const std::exception& ex) {
				log(WARN) << "Failed to read key from file: " << ex.what();
			}
		}
		if(timelord_sk == skey_t()) {
			timelord_sk = hash_t::random();
			try {
				file.open("wb");
				vnx::write_generic(file.out, timelord_sk);
				file.close();
			}
			catch(const std::exception& ex) {
				log(WARN) << "Failed to write key to file: " << ex.what();
			}
		}
		timelord_key = pubkey_t::from_skey(timelord_sk);
		log(INFO) << "Our Timelord Key: " << timelord_key;
	}

	set_timer_millis(10000, std::bind(&TimeLord::print_info, this));

	Super::main();

	stop_vdf();
}

void TimeLord::start_vdf(vdf_point_t begin)
{
	if(!is_running) {
		is_running = true;
		latest_point = nullptr;
		last_restart = vnx::get_time_micros();
		log(INFO) << "Started VDF at " << begin.num_iters;
		vdf_thread = std::thread(&TimeLord::vdf_loop, this, begin);
	}
}

void TimeLord::stop_vdf()
{
	{
		std::lock_guard<std::recursive_mutex> lock(mutex);
		is_running = false;
	}
	if(vdf_thread.joinable()) {
		vdf_thread.join();
	}
	clear_history();
}

void TimeLord::clear_history()
{
	history.clear();
	infuse_history[0].clear();
	infuse_history[1].clear();
}

void TimeLord::handle(std::shared_ptr<const TimeInfusion> value)
{
	if(value->chain > 1) {
		return;
	}
	std::lock_guard<std::recursive_mutex> lock(mutex);

	auto& map = infuse[value->chain];
	for(const auto& entry : value->values) {
		if(!map.count(entry.first)) {
			if(latest_point && entry.first < latest_point->num_iters) {
				log(WARN) << "Missed infusion point at " << entry.first << " iterations";
			}
			log(DEBUG) << "Infusing at " << entry.first << " on chain " << value->chain << ": " << entry.second;
		}
	}
	if(!value->values.empty()) {
		map.erase(map.lower_bound(value->values.begin()->first), map.end());
	}
	map.insert(value->values.begin(), value->values.end());
}

void TimeLord::handle(std::shared_ptr<const IntervalRequest> request)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);

	checkpoint_iters = (request->end - request->begin) / request->num_segments;

	if(request->has_start)
	{
		vdf_point_t begin;
		begin.num_iters = request->begin;
		begin.output = request->start_values;

		if(!is_running) {
			start_vdf(begin);
		} else {
			auto iter = history.find(request->begin);
			if((iter != history.end() && iter->second != request->start_values)
				|| (iter == history.end() && latest_point && latest_point->num_iters > request->begin))
			{
				do_restart = true;
				clear_history();
				latest_point = std::make_shared<vdf_point_t>(begin);
				log(WARN) << "Our VDF forked from the network, restarting ...";
			}
			else if(!latest_point || begin.num_iters > latest_point->num_iters) {
				// another timelord is faster
				const auto now = vnx::get_time_micros();
				if((now - last_restart) / 1000 > restart_holdoff) {
					last_restart = now;
					latest_point = std::make_shared<vdf_point_t>(begin);
				}
			}
		}
	}

	if(request->end > request->begin) {
		pending.emplace(std::make_pair(request->end, request->begin), request->height);
	}
	update();
}

void TimeLord::update()
{
	std::lock_guard<std::recursive_mutex> lock(mutex);

	for(auto iter = pending.begin(); iter != pending.end();)
	{
		const auto iters_end = iter->first.first;
		const auto iters_begin = iter->first.second;

		auto end = history.find(iters_end);
		if(end != history.end())
		{
			auto begin = history.find(iters_begin);
			if(begin != history.end())
			{
				auto proof = ProofOfTime::create();
				proof->start = iters_begin;
				proof->height = iter->second;
				proof->input = begin->second;

				for(uint32_t k = 0; k < 2; ++k) {
					auto iter = infuse_history[k].find(iters_begin);
					if(iter != infuse_history[k].end()) {
						proof->infuse[k] = iter->second;
					}
				}

				end++;
				begin++;
				auto prev_iters = iters_begin;
				for(auto iter = begin; iter != end; ++iter) {
					time_segment_t seg;
					seg.num_iters = iter->first - prev_iters;
					seg.output = iter->second;
					prev_iters = iter->first;
					proof->segments.push_back(seg);
				}

				proof->timelord_reward = reward_addr;
				proof->timelord_key = timelord_key;
				proof->timelord_sig = signature_t::sign(timelord_sk, proof->calc_hash());

				publish(proof, output_proofs);

				iter = pending.erase(iter);
				continue;
			}
		}
		if(!history.empty() && history.upper_bound(iters_begin) == history.begin())
		{
			iter = pending.erase(iter);
			continue;
		}
		iter++;
	}
}

void TimeLord::vdf_loop(vdf_point_t point)
{
	bool do_notify = false;

	while(vnx_do_run())
	{
		const auto time_begin = vnx::get_wall_time_micros();

		uint64_t next_target = 0;
		{
			std::lock_guard<std::recursive_mutex> lock(mutex);

			if(!is_running) {
				break;
			}
			if(latest_point && (do_restart || latest_point->num_iters > point.num_iters))
			{
				do_restart = false;
				point = *latest_point;
				log(INFO) << "Restarted VDF at " << point.num_iters;
			}
			else {
				if(!latest_point) {
					latest_point = std::make_shared<vdf_point_t>();
				}
				*latest_point = point;
			}
			history.emplace(point.num_iters, point.output);

			// purge history
			while(history.size() > max_history) {
				history.erase(history.begin());
			}

			for(uint32_t k = 0; k < 2; ++k)
			{
				// purge history
				if(history.size() >= max_history)
				{
					const auto begin = history.begin()->first;
					infuse_history[k].erase(infuse_history[k].begin(), infuse_history[k].lower_bound(begin));
				}
				{
					// apply infusions
					auto iter = infuse[k].find(point.num_iters);
					if(iter != infuse[k].end()) {
						point.output[k] = hash_t(point.output[k] + iter->second);
						infuse_history[k].insert(*iter);
					}
				}
				{
					// check for upcoming infusion point
					auto iter = infuse[k].upper_bound(point.num_iters);
					if(iter != infuse[k].end()) {
						if(!next_target || iter->first < next_target) {
							next_target = iter->first;
						}
					}
				}
			}
			{
				// check for upcoming boundary point
				auto iter = pending.upper_bound(std::make_pair(point.num_iters, point.num_iters));
				if(iter != pending.end()) {
					const auto iters_end = iter->first.first;
					if(!next_target || iters_end < next_target) {
						next_target = iters_end;
					}
				}
				if(iter != pending.begin()) {
					do_notify = true;
				}
			}
			if(do_notify) {
				add_task(std::bind(&TimeLord::update, this));
			}
		}
		do_notify = false;

		const auto checkpoint = point.num_iters + checkpoint_iters;

		if(next_target <= point.num_iters) {
			next_target = checkpoint;
		}
		else if(next_target <= checkpoint) {
			do_notify = true;
		}
		else {
			next_target = checkpoint;
		}
		const auto num_iters = next_target - point.num_iters;

#pragma omp parallel for num_threads(2)
		for(uint32_t k = 0; k < 2; ++k) {
			point.output[k] = compute(point.output[k], num_iters);
		}
		point.num_iters += num_iters;

		const auto time_end = vnx::get_wall_time_micros();

		if(time_end > time_begin && num_iters > checkpoint_iters / 2)
		{
			// update estimated number of iterations per second
			const auto speed = (num_iters * 1000000) / (time_end - time_begin);
			avg_iters_per_sec = (avg_iters_per_sec * 255 + speed) / 256;
		}
	}
}

hash_t TimeLord::compute(const hash_t& input, const uint64_t num_iters)
{
	hash_t hash = input;
	for(uint64_t i = 0; i < num_iters; ++i) {
		hash = hash_t(hash.bytes);
	}
	return hash;
}

void TimeLord::print_info()
{
	if(is_running) {
		log(INFO) << double(avg_iters_per_sec) / 1e6 << " M/s iterations";
	}
	vnx::open_flow(vnx::get_pipe(node_server), vnx::get_pipe(vnx_name));
}


} // mmx
