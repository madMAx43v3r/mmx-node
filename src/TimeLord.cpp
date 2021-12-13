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
	subscribe(input_points);
	subscribe(input_request);
}

void TimeLord::main()
{
	if(self_test) {
		auto request = IntervalRequest::create();
		request->begin = 0;
		request->end = 10 * 1000 * 1000;
		request->interval = checkpoint_interval;
		handle(request);
	}

	set_timer_millis(10000, std::bind(&TimeLord::print_info, this));

	Super::main();

	if(vdf_thread.joinable()) {
		vdf_thread.join();
	}
}

void TimeLord::start_vdf(TimePoint begin)
{
	log(INFO) << "Started VDF at " << begin.num_iters;
	vdf_thread = std::thread(&TimeLord::vdf_loop, this, begin);
}

void TimeLord::handle(std::shared_ptr<const TimePoint> value)
{
	std::lock_guard<std::mutex> lock(mutex);

	if(!latest_point || value->num_iters > latest_point->num_iters)
	{
		latest_point = value;
		log(DEBUG) << "Got new verified peak at " << value->num_iters;
	}
	if(!vdf_thread.joinable())
	{
		start_vdf(*value);
	}
}

void TimeLord::handle(std::shared_ptr<const IntervalRequest> value)
{
	{
		std::lock_guard<std::mutex> lock(mutex);

		if(value->end > value->begin) {
			pending.emplace(value->end, value->begin);
		}
		checkpoint_interval = value->interval;			// should be constant
	}
	if(!vdf_thread.joinable())
	{
		// start from seed
		std::string seed;
		vnx::read_config("chain.params.vdf_seed", seed);

		TimePoint begin;
		begin.output = hash_t(seed);
		start_vdf(begin);
	}
	update();
}

void TimeLord::update()
{
	std::lock_guard<std::mutex> lock(mutex);

	for(auto iter = pending.begin(); iter != pending.end();)
	{
		const auto iters_begin = iter->second;
		const auto iters_end = iter->first;

		auto end = history.lower_bound(iters_end);
		if(end != history.end()) {
			auto begin = history.upper_bound(iters_begin);
			if(begin != history.end() && begin != end)
			{
				auto proof = ProofOfTime::create();
				proof->start = iters_begin;

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

					hash_t hash = prev->second;
					for(uint64_t i = 0; i < seg.num_iters; ++i) {
						hash = hash_t(hash.bytes);
					}
					seg.output = hash;
				}
				proof->segments.push_back(seg);

				publish(proof, output_proofs);

				if(self_test) {
					add_task([this, proof]() {
						std::lock_guard<std::mutex> lock(mutex);
						const auto end = proof->start + proof->get_num_iters();
						pending.emplace(end + 10 * 1000 * 1000, end);
					});
				}
			} else {
				// we don't have enough history
			}
		} else {
			// nothing to do
			break;
		}
		iter = pending.erase(iter);
	}
}

void TimeLord::vdf_loop(TimePoint point)
{
	bool do_notify = false;
	checkpoint_iters = checkpoint_interval;

	while(vnx_do_run())
	{
		const auto time_begin = vnx::get_wall_time_micros();

		uint64_t next_target = 0;
		{
			std::lock_guard<std::mutex> lock(mutex);

			if(latest_point && latest_point->num_iters > point.num_iters)
			{
				// somebody else is faster
				point = *latest_point;
				pending.clear();
				log(INFO) << "Restarted VDF at " << point.num_iters;
			}
			while(history.size() >= max_history) {
				history.erase(history.begin());
			}
			history.emplace(point.num_iters, point.output);

			for(const auto& entry : pending) {
				if(entry.first > point.num_iters) {
					next_target = entry.first;
					break;
				} else {
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

		hash_t hash = point.output;
		for(uint64_t i = 0; i < num_iters; ++i) {
			hash = hash_t(hash.bytes);
		}
		point.output = hash;
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

void TimeLord::print_info()
{
	log(INFO) << double(checkpoint_iters) / checkpoint_interval << " M/s iterations";
}


} // mmx
