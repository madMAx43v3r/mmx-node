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

	buffer.reserve(64 * 1024);

	for(const auto& entry : infuse[0]) {
		write_bytes(out, entry.first);
		write_bytes(out, entry.second);
	}
	for(const auto& entry : infuse[1]) {
		write_bytes(out, entry.first);
		write_bytes(out, entry.second);
	}
	for(const auto& seg : segments) {
		write_bytes(out, seg.num_iters);
		write_bytes(out, seg.output[0]);
		write_bytes(out, seg.output[1]);
	}
	write_bytes(out, timelord_key);
	out.flush();

	return hash_t(buffer);
}

mmx::hash_t ProofOfTime::get_output(const uint32_t& chain) const
{
	if(chain > 1) {
		throw std::logic_error("invalid chain");
	}
	hash_t res;
	if(!segments.empty()) {
		res = segments.back().output[chain];
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
	out->infuse = infuse;
	{
		time_segment_t seg;
		seg.num_iters = get_num_iters();
		seg.output[0] = get_output(0);
		seg.output[1] = get_output(1);
		out->segments.push_back(seg);
	}
	return out;
}


} // mmx
