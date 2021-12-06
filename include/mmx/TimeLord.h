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

	void handle(std::shared_ptr<const TimePoint> value) override;

	void handle(std::shared_ptr<const IntervalRequest> value) override;

private:
	void update();

	void vdf_loop(TimePoint point);

private:
	std::mutex mutex;
	std::thread vdf_thread;

	std::map<uint64_t, hash_t> history;

	std::shared_ptr<const TimePoint> latest_point;

	std::set<std::pair<uint64_t, uint64_t>> pending;

};


} // mmx

#endif /* INCLUDE_MMX_TIMELORD_H_ */
