/*
 * TimeLord.cpp
 *
 *  Created on: Dec 6, 2021
 *      Author: mad
 */

#include <mmx/TimeLord.h>
#include <mmx/ProofOfTime.hxx>

#include <vnx/Config.hpp>


namespace mmx {

TimeLord::TimeLord(const std::string& _vnx_name)
	:	TimeLordBase(_vnx_name)
{
}

void TimeLord::init()
{
	subscribe(input_infuse, 1000);
	subscribe(input_request, 1000);
}

void TimeLord::main()
{
	set_timer_millis(10000, std::bind(&TimeLord::print_info, this));

	Super::main();

	if(vdf_thread.joinable()) {
		vdf_thread.join();
	}
}

void TimeLord::start_vdf(vdf_point_t begin)
{
	if(!is_running) {
		is_running = true;
		log(INFO) << "Started VDF at " << begin.num_iters;
		vdf_thread = std::thread(&TimeLord::vdf_loop, this, begin);
	}
}

void TimeLord::handle(std::shared_ptr<const TimeInfusion> value)
{
	if(value->chain > 1) {
		return;
	}
	std::lock_guard<std::recursive_mutex> lock(mutex);

	for(const auto& entry : value->values) {
		if(!infuse[value->chain].count(entry.first)) {
			if(latest_point && entry.first < latest_point->num_iters) {
				log(WARN) << "Missed infusion point at " << entry.first << " iterations";
			}
			log(DEBUG) << "Infusing at " << entry.first << " iterations on chain " << value->chain << ": " << entry.second;
		}
	}
	infuse[value->chain] = value->values;
}

void TimeLord::handle(std::shared_ptr<const IntervalRequest> request)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);

	if(request->end > request->begin) {
		pending.emplace(request->end, request->begin);
	}
	checkpoint_interval = request->interval;			// should be constant

	update();

	if(request->has_start) {
		vdf_point_t begin;
		begin.num_iters = request->begin;
		begin.output = request->start_values;

		if(!is_running) {
			start_vdf(begin);
		} else {
			auto iter = history.find(request->begin);
			if(iter != history.end()) {
				if(iter->second != request->start_values) {
					do_restart = true;
					latest_point = std::make_shared<vdf_point_t>(begin);
					log(WARN) << "Our VDF forked from the network, restarting...";
				}
			}
		}
	}
}

void TimeLord::update()
{
	std::lock_guard<std::recursive_mutex> lock(mutex);

	for(auto iter = pending.begin(); iter != pending.end();)
	{
		const auto iters_begin = iter->second;
		const auto iters_end = iter->first;

		auto end = history.lower_bound(iters_end);
		if(end != history.end())
		{
			auto begin = history.upper_bound(iters_begin);
			if(begin != history.end() && begin != end)
			{
				auto proof = ProofOfTime::create();
				proof->start = iters_begin;

				for(uint32_t k = 0; k < 2; ++k) {
					proof->infuse[k].insert(infuse_history[k].lower_bound(iters_begin), infuse_history[k].lower_bound(iters_end));
				}

				auto prev_iters = iters_begin;
				for(auto iter = begin; iter != end; ++iter) {
					time_segment_t seg;
					seg.num_iters = iter->first - prev_iters;
					seg.output = iter->second;
					prev_iters = iter->first;
					proof->segments.push_back(seg);
				}

				time_segment_t seg;
				if(end->first == iters_end) {
					// history has exact end
					seg.num_iters = end->first - prev_iters;
					seg.output = end->second;
				} else {
					// need to recompute end point from previous checkpoint
					auto prev = end; prev--;
					seg.num_iters = iters_end - prev->first;
					for(uint32_t k = 0; k < 2; ++k) {
						seg.output[k] = compute(prev->second[k], seg.num_iters);
					}
				}
				proof->segments.push_back(seg);

				publish(proof, output_proofs);
			}
		} else {
			// nothing to do
			break;
		}
		iter = pending.erase(iter);
	}
}

void TimeLord::vdf_loop(vdf_point_t point)
{
	bool do_notify = false;
	checkpoint_iters = checkpoint_interval;

	while(vnx_do_run())
	{
		const auto time_begin = vnx::get_wall_time_micros();

		uint64_t next_target = 0;
		{
			std::lock_guard<std::recursive_mutex> lock(mutex);

			if(latest_point && (do_restart || latest_point->num_iters > point.num_iters))
			{
				if(do_restart) {
					history.clear();
					infuse_history[0].clear();
					infuse_history[1].clear();
				}
				do_restart = false;
				point = *latest_point;
				log(INFO) << "Restarted VDF at " << point.num_iters;
			}
			else {
				if(!latest_point) {
					latest_point = std::make_shared<vdf_point_t>();
				}
				*latest_point = point;
				history.emplace(point.num_iters, point.output);
			}

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
					if(!next_target || iter->first < next_target) {
						next_target = iter->first;
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
			// update estimated number of iterations per checkpoint
			const auto interval = (num_iters * checkpoint_interval) / (time_end - time_begin);
			checkpoint_iters = (checkpoint_iters * 255 + interval) / 256;
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
	log(INFO) << double(checkpoint_iters) / checkpoint_interval << " M/s iterations";
}


} // mmx
