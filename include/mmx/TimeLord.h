/*
 * TimeLord.h
 *
 *  Created on: Dec 6, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_TIMELORD_H_
#define INCLUDE_MMX_TIMELORD_H_

#include <mmx/TimeLordBase.hxx>


namespace mmx {

class TimeLord : public TimeLordBase {
public:
	TimeLord(const std::string& _vnx_name);

protected:
	void init() override;

	void main() override;

	void handle(std::shared_ptr<const TimeInfusion> value) override;

	void handle(std::shared_ptr<const IntervalRequest> value) override;

private:
	struct time_point_t {
		uint64_t num_iters = 0;
		hash_t output;
	};

	void update();

	void start_vdf(time_point_t begin);

	void vdf_loop(time_point_t point);

	hash_t compute(const hash_t& input, const uint64_t start, const uint64_t num_iters);

	void print_info();

private:
	std::thread vdf_thread;
	std::recursive_mutex mutex;

	bool do_restart = false;
	uint64_t checkpoint_iters = 0;

	std::map<uint64_t, hash_t> infuse;
	std::map<uint64_t, hash_t> history;
	std::map<uint64_t, hash_t> infuse_history;
	std::set<std::pair<uint64_t, uint64_t>> pending;

	std::shared_ptr<time_point_t> latest_point;

};


} // mmx

#endif /* INCLUDE_MMX_TIMELORD_H_ */
