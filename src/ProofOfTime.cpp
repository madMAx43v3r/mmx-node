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
		write_bytes(out, seg.output);
	}
	out.flush();

	return hash_t(buffer);
}


} // mmx
