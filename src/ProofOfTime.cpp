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

	// TODO: write_bytes(out, get_type_hash());
	// TODO: write_bytes(out, version);
	write_bytes(out, height);
	write_bytes(out, start);
	write_bytes(out, input);
	write_bytes(out, infuse);
	for(const auto& seg : segments) {
		write_bytes(out, seg.num_iters);
		write_bytes(out, seg.output);
	}
	write_bytes(out, timelord_proof);
	write_bytes(out, timelord_reward);
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

uint64_t ProofOfTime::get_vdf_iters() const
{
	return start + get_num_iters();
}


} // mmx
