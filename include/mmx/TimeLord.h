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

	void stop_vdf() override;

private:
	struct vdf_point_t {
		uint64_t num_iters = 0;
		std::array<hash_t, 2> output;
	};

	void update();

	void clear_history();

	void start_vdf(vdf_point_t begin);

	void vdf_loop(vdf_point_t point);

	hash_t compute(const hash_t& input, const uint64_t num_iters);

	void print_info();

private:
	std::thread vdf_thread;
	std::recursive_mutex mutex;

	bool do_restart = false;
	bool is_running = false;
	int64_t last_restart = 0;
	uint64_t checkpoint_iters = 1000;
	uint64_t avg_iters_per_sec = 0;

	std::map<uint64_t, hash_t> infuse[2];
	std::map<uint64_t, hash_t> infuse_history[2];
	std::map<uint64_t, std::array<hash_t, 2>> history;

	std::map<std::pair<uint64_t, uint64_t>, uint32_t> pending;		// [[end, start] => height]

	std::shared_ptr<vdf_point_t> latest_point;

	skey_t timelord_sk;
	pubkey_t timelord_key;

};


} // mmx

#endif /* INCLUDE_MMX_TIMELORD_H_ */
