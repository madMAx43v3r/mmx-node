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

void TimeLord::start_vdf(time_point_t begin)
{
	log(INFO) << "Started VDF at " << begin.num_iters;
	vdf_thread = std::thread(&TimeLord::vdf_loop, this, begin);
}

void TimeLord::handle(std::shared_ptr<const TimeInfusion> value)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);

	if(!infuse.count(value->num_iters)) {
		if(latest_point && value->num_iters < latest_point->num_iters) {
			log(WARN) << "Missed infusion point at " << value->num_iters << " iterations";
		}
		log(DEBUG) << "Infusing at " << value->num_iters << " iterations: " << value->value;
	}
	infuse[value->num_iters] = value->value;
}

void TimeLord::handle(std::shared_ptr<const IntervalRequest> value)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);

	if(value->end > value->begin) {
		pending.emplace(value->end, value->begin);
	}
	checkpoint_interval = value->interval;			// should be constant

	if(value->begin_hash) {
		time_point_t begin;
		begin.num_iters = value->begin;
		begin.output = *value->begin_hash;

		if(!vdf_thread.joinable()) {
			start_vdf(begin);
		} else {
			auto iter = history.find(value->begin);
			if(iter != history.end()) {
				if(iter->second != *value->begin_hash) {
					do_restart = true;
					latest_point = std::make_shared<time_point_t>(begin);
					log(WARN) << "Our VDF forked from the network, restarting...";
				}
			}
		}
	}
	update();
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
				proof->infuse.insert(infuse_history.lower_bound(iters_begin), infuse_history.lower_bound(iters_end));

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
					seg.output = compute(prev->second, prev->first, seg.num_iters);
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

void TimeLord::vdf_loop(time_point_t point)
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
					infuse_history.clear();
				}
				do_restart = false;
				point = *latest_point;
				log(INFO) << "Restarted VDF at " << point.num_iters;
			}
			else {
				if(!latest_point) {
					latest_point = std::make_shared<time_point_t>();
				}
				*latest_point = point;
				history.emplace(point.num_iters, point.output);
			}
			// purge history
			while(history.size() > max_history) {
				history.erase(history.begin());
			}
			if(history.size() >= max_history) {
				const auto begin = history.begin()->first;
				infuse.erase(infuse.begin(), infuse.lower_bound(begin));
				infuse_history.erase(infuse_history.begin(), infuse_history.lower_bound(begin));
			}
			{
				// check for upcoming infusion point
				auto iter = infuse.upper_bound(point.num_iters);
				if(iter != infuse.end()) {
					if(!next_target || iter->first < next_target) {
						next_target = iter->first;
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

		const auto next_checkpoint = point.num_iters + checkpoint_iters;
		if(next_target <= point.num_iters) {
			next_target = next_checkpoint;
		}
		else if(next_target <= next_checkpoint) {
			do_notify = true;
		}
		else {
			next_target = next_checkpoint;
		}
		const auto num_iters = next_target - point.num_iters;

		point.output = compute(point.output, point.num_iters, num_iters);
		point.num_iters = next_target;

		const auto time_end = vnx::get_wall_time_micros();

		if(time_end > time_begin && num_iters > checkpoint_iters / 2)
		{
			// update estimated number of iterations per checkpoint
			const auto interval = (num_iters * checkpoint_interval) / (time_end - time_begin);
			checkpoint_iters = (checkpoint_iters * 255 + interval) / 256;
		}
	}
}

hash_t TimeLord::compute(const hash_t& input, const uint64_t start, const uint64_t num_iters)
{
	hash_t hash = input;
	{
		std::lock_guard<std::recursive_mutex> lock(mutex);

		auto iter = infuse.find(start);
		if(iter != infuse.end()) {
			hash = hash_t(hash + iter->second);
			infuse_history.insert(*iter);
		}
	}
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
