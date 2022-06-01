/*
 * ProofOfTime.cpp
 *
 *  Created on: Nov 30, 2021
 *      Author: mad
 */

#include <mmx/ProofOfTime.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

bool ProofOfTime::is_valid(std::shared_ptr<const ChainParams> params) const
{
	return version == 0
		&& segments.size() >= params->min_vdf_segments
		&& segments.size() <= params->max_vdf_segments
		&& calc_hash() == hash;
}

hash_t ProofOfTime::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	buffer.reserve(64 * 1024);

	write_bytes(out, get_type_hash());
	write_field(out, "version", version);
	write_field(out, "height", 	height);
	write_field(out, "start", 	start);
	write_field(out, "input", 	input);
	write_field(out, "infuse", 	infuse);
	write_field(out, "segments", segments);
	write_field(out, "timelord_reward", timelord_reward);
	write_field(out, "timelord_key", 	timelord_key);
	out.flush();

	return hash_t(buffer);
}

hash_t ProofOfTime::get_full_hash() const
{
	return hash_t(hash + timelord_sig);
}

hash_t ProofOfTime::get_output(const uint32_t& chain) const
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

void ProofOfTime::validate() const
{
	if(!timelord_sig.verify(timelord_key, calc_hash())) {
		throw std::logic_error("invalid timelord signature");
	}
}


} // mmx
