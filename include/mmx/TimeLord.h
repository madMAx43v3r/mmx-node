/*
 * TimeLord.h
 *
 *  Created on: Dec 6, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_TIMELORD_H_
#define INCLUDE_MMX_TIMELORD_H_

#include <mmx/TimeLordBase.hxx>
#include <mmx/pubkey_t.hpp>

#include <vnx/ThreadPool.h>


namespace mmx {

class TimeLord : public TimeLordBase {
public:
	TimeLord(const std::string& _vnx_name);

protected:
	void init() override;

	void main() override;

	void handle(std::shared_ptr<const IntervalRequest> value) override;

	void stop_vdf() override;

private:
	struct vdf_point_t {
		hash_t output;
		uint64_t num_iters = 0;
	};

	void update();

	void start_vdf(vdf_point_t begin);

	void vdf_loop();

	static hash_t compute(const hash_t& input, const uint64_t num_iters);

	void print_info();

private:
	std::mutex mutex;
	std::thread vdf_thread;
	std::condition_variable vdf_signal;

	bool do_run = false;
	bool is_reset = false;

	uint64_t segment_iters = 1000;
	uint64_t avg_iters_per_sec = 0;

	std::map<uint64_t, hash_t> history;
	std::map<uint64_t, hash_t> infuse;

	uint64_t peak_iters = 0;
	std::map<uint64_t, std::shared_ptr<const IntervalRequest>> pending;		// [end => request]

	std::shared_ptr<vdf_point_t> peak;

	skey_t timelord_sk;
	pubkey_t timelord_key;

};


} // mmx

#endif /* INCLUDE_MMX_TIMELORD_H_ */
