/*
 * ProofOfTime.cpp
 *
 *  Created on: Nov 30, 2021
 *      Author: mad
 */

#include <mmx/ProofOfTime.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

mmx::hash_t ProofOfTime::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	buffer.reserve(16 * 1024);

	for(const auto& seg : segments) {
		write_bytes(out, seg.num_iters);
		write_bytes(out, seg.output);
	}
	out.flush();

	return hash_t(buffer);
}

mmx::hash_t ProofOfTime::get_output() const
{
	hash_t res;
	if(!segments.empty()) {
		res = segments.back().output;
	}
	return res;
}

uint64_t ProofOfTime::get_num_iters() const
{
	uint64_t sum = 0;
	for(const auto& seg : segments) {
		sum += seg.num_iters;
	}
	return sum;
}

std::shared_ptr<const ProofOfTime> ProofOfTime::compressed() const
{
	auto out = ProofOfTime::create();
	out->start = start;
	{
		time_segment_t seg;
		seg.num_iters = get_num_iters();
		seg.output = get_output();
		out->segments.push_back(seg);
	}
	return out;
}


} // mmx
