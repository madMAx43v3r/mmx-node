/*
 * ProofOfTime.cpp
 *
 *  Created on: Nov 30, 2021
 *      Author: mad
 */

#include <mmx/ProofOfTime.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

bool ProofOfTime::is_valid() const
{
	return version == 0
		&& hash == calc_hash()
		&& content_hash == calc_content_hash();
}

hash_t ProofOfTime::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	buffer.reserve(64 * 1024);

	write_bytes(out, get_type_hash());
	write_field(out, "version", 	version);
	write_field(out, "hash", 		hash);
	write_field(out, "vdf_height", 	vdf_height);
	write_field(out, "start", 		start);
	write_field(out, "num_iters", 	num_iters);
	write_field(out, "segment_size", segment_size);
	write_field(out, "input", 		input);
	write_field(out, "prev", 		prev);
	write_field(out, "reward_addr", reward_addr);
	write_field(out, "segments", 	segments);
	out.flush();

	return hash_t(buffer);
}

hash_t ProofOfTime::calc_content_hash() const
{
	return hash_t(hash + timelord_key + timelord_sig);
}

hash_t ProofOfTime::get_output() const
{
	if(segments.empty()) {
		throw std::logic_error("empty proof");
	}
	return segments.back();
}

uint64_t ProofOfTime::get_vdf_iters() const
{
	return start + num_iters;
}

void ProofOfTime::validate() const
{
	if(!timelord_sig.verify(timelord_key, hash)) {
		throw std::logic_error("invalid timelord signature");
	}
}


} // mmx
