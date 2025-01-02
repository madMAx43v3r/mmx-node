/*
 * TimeLord.cpp
 *
 *  Created on: Dec 6, 2021
 *      Author: mad
 */

#include <mmx/TimeLord.h>
#include <mmx/ProofOfTime.hxx>
#include <mmx/WalletClient.hxx>
#include <mmx/utils.h>
#include <mmx/helpers.h>

#include <vnx/vnx.h>

#include <sha256_ni.h>
#include <sha256_arm.h>


namespace mmx {

TimeLord::TimeLord(const std::string& _vnx_name)
	:	TimeLordBase(_vnx_name)
{
	segment_iters = get_params()->vdf_segment_size;
}

void TimeLord::init()
{
	subscribe(input_request, 1000);

	vnx::open_pipe(vnx_name, this, 1000);
}

void TimeLord::main()
{
	if(!reward_addr) {
		try {
			WalletClient wallet(wallet_server);
			for(const auto& entry : wallet.get_all_accounts()) {
				if(entry.address) {
					reward_addr = entry.address;
					break;
				}
			}
			if(!reward_addr) {
				throw std::logic_error("no wallet available");
			}
		}
		catch(const std::exception& ex) {
			log(WARN) << "Failed to get reward address from wallet: " << ex.what();
		}
	}
	if(reward_addr) {
		log(INFO) << "Reward address: " << reward_addr->to_string();
	} else {
		log(WARN) << "Reward is disabled!";
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
			timelord_sk = skey_t(hash_t::random());
			try {
				file.open("wb");
				vnx::write_generic(file.out, timelord_sk);
				file.close();
			}
			catch(const std::exception& ex) {
				log(WARN) << "Failed to write key to file: " << ex.what();
			}
		}
		timelord_key = pubkey_t(timelord_sk);
		log(INFO) << "Timelord Key: " << timelord_key;
	}

	set_timer_millis(10000, std::bind(&TimeLord::print_info, this));

	Super::main();

	stop_vdf();
}

void TimeLord::start_vdf(vdf_point_t begin)
{
	if(!is_running) {
		is_running = true;
		peak = nullptr;
		log(INFO) << "Started VDF at " << begin.num_iters;
		vdf_thread = std::thread(&TimeLord::vdf_loop, this, begin);
	}
}

void TimeLord::stop_vdf()
{
	{
		std::lock_guard<std::mutex> lock(mutex);
		is_running = false;
	}
	if(vdf_thread.joinable()) {
		vdf_thread.join();
	}
	peak = nullptr;
	peak_iters = 0;
	history.clear();
	infuse.clear();
}

void TimeLord::handle(std::shared_ptr<const IntervalRequest> req)
{
	std::lock_guard<std::mutex> lock(mutex);

	const auto start = req->start;
	const auto end = req->end;

	// clear obsolete requests + infusions in-between
	infuse.erase( infuse.upper_bound(start),  infuse.lower_bound(end));
	pending.erase(pending.upper_bound(start), pending.lower_bound(end));

	{
		const bool passed = peak && start <= peak->num_iters;

		bool is_fork = false;
		auto iter = infuse.find(start);
		if(iter != infuse.end()) {
			if(passed && req->infuse != iter->second) {
				is_fork = true;
				log(WARN) << "Infusion value at " << start << " changed, restarting ...";
			}
		} else {
			if(passed) {
				is_fork = true;
				log(WARN) << "Missed infusion at " << start << " iterations, restarting ...";
			}
			log(DEBUG) << "Infusing at " << start << " iterations: " << req->infuse;
		}
		infuse[start] = req->infuse;

		if(is_fork) {
			vdf_point_t begin;
			begin.output = history[start];
			begin.num_iters = start;
			peak = std::make_shared<vdf_point_t>(begin);
			is_reset = true;
		}
	}

	if(auto input = req->input)
	{
		vdf_point_t begin;
		begin.output = *input;
		begin.num_iters = start;

		if(is_running) {
			const bool is_fork = peak
					&& find_value(history, start) != input
					&& (!is_reset || start > peak->num_iters);
			if(is_fork) {
				if(start >= peak->num_iters) {
					if(!is_reset) {
						log(INFO) << "Another Timelord was faster, restarting ...";
					}
				} else {
					log(WARN) << "Our VDF forked from the network, restarting ...";
				}
			}
			if(!peak || is_fork || begin.num_iters > peak->num_iters) {
				// another timelord is faster
				peak = std::make_shared<vdf_point_t>(begin);
				is_reset = true;
			}
		} else {
			start_vdf(begin);
		}
	}

	if(is_reset) {
		peak_iters = 0;
	}
	if(req->end > peak_iters) {
		pending[end] = req;
	}
	if(peak && peak->num_iters >= end) {
		add_task(std::bind(&TimeLord::update, this));
	}
}

void TimeLord::update()
{
	std::unique_lock<std::mutex> lock(mutex);

	// clear old requests first
	if(!history.empty()) {
		const auto begin = history.begin()->first;
		pending.erase(pending.begin(), pending.lower_bound(begin));
	}
	std::vector<std::shared_ptr<ProofOfTime>> out;

	for(auto iter = pending.begin(); iter != pending.end();)
	{
		const auto req = iter->second;

		auto end = history.find(req->end);
		if(end != history.end())
		{
			auto begin = history.find(req->start);
			if(begin != history.end())
			{
				auto proof = ProofOfTime::create();
				proof->vdf_height = req->vdf_height;
				proof->start = req->start;
				proof->num_iters = req->end - req->start;
				proof->segment_size = segment_iters;
				proof->input = begin->second;
				proof->prev = req->infuse;
				proof->reward_addr = reward_addr ? *reward_addr : addr_t();
				proof->timelord_key = timelord_key;
				proof->segments.reserve(1024);

				end++;
				begin++;
				for(auto iter = begin; iter != end; ++iter) {
					proof->segments.push_back(iter->second);
				}
				out.push_back(proof);
				peak_iters = req->end;
			}
			iter = pending.erase(iter);
		} else {
			break;
		}
	}

	while(infuse.size() > 1000) {
		infuse.erase(infuse.begin());
	}
	while(history.size() > max_history) {
		history.erase(history.begin());
	}

	lock.unlock();	// --------------------------------------------------------------------------------------------

	for(auto proof : out) {
		proof->hash = proof->calc_hash();
		proof->timelord_sig = signature_t::sign(timelord_sk, proof->hash);
		proof->content_hash = proof->calc_content_hash();

		publish(proof, output_proofs);

		log(DEBUG) << "Created VDF for height " << proof->vdf_height << " with " << proof->segments.size() << " segments";
	}
}

void TimeLord::vdf_loop(vdf_point_t point)
{
	while(vnx_do_run()) {
		{
			std::lock_guard<std::mutex> lock(mutex);

			if(!is_running) {
				break;
			}
			if(is_reset) {
				point = *peak;
				history.clear();
				is_reset = false;
				log(DEBUG) << "Restarted VDF at " << point.num_iters;
			} else {
				if(!peak) {
					peak = std::make_shared<vdf_point_t>();
				}
				*peak = point;
			}
			history[point.num_iters] = point.output;

			if(pending.count(point.num_iters)) {
				add_task(std::bind(&TimeLord::update, this));
			}

			// apply infusion
			auto iter = infuse.find(point.num_iters);
			if(iter != infuse.end()) {
				point.output = hash_t(point.output + iter->second);
				point.output = hash_t(point.output + (reward_addr ? *reward_addr : addr_t()));
			}
		}
		const auto time_begin = vnx::get_wall_time_micros();

		point.output = compute(point.output, segment_iters);
		point.num_iters += segment_iters;

		// update estimated speed
		const auto time_end = vnx::get_wall_time_micros();
		if(time_end > time_begin) {
			const auto speed = (segment_iters * 1000000) / (time_end - time_begin);
			avg_iters_per_sec = (avg_iters_per_sec * 1023 + speed) / 1024;
		}
	}
	log(INFO) << "Stopped VDF";
}

hash_t TimeLord::compute(const hash_t& input, const uint64_t num_iters)
{
	static bool have_sha_ni = sha256_ni_available();
	static bool have_sha_arm = sha256_arm_available();

	hash_t hash = input;
	if(have_sha_ni) {
		recursive_sha256_ni(hash.data(), num_iters);
	} else if(have_sha_arm) {
		recursive_sha256_arm(hash.data(), num_iters);
	} else {
		for(uint64_t i = 0; i < num_iters; ++i) {
			hash = hash_t(hash.bytes);
		}
	}
	return hash;
}

void TimeLord::print_info()
{
	if(is_running) {
		log(INFO) << double(avg_iters_per_sec) / 1e6 << " MH/s";
	}
	vnx::open_flow(vnx::get_pipe(node_server), vnx::get_pipe(vnx_name));
}


} // mmx
