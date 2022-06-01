/*
 * MutableRelay.cpp
 *
 *  Created on: Mar 14, 2022
 *      Author: mad
 */

#include <mmx/solution/MutableRelay.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace solution {

hash_t MutableRelay::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", version);
	write_field(out, "target",	target ? target->calc_hash() : hash_t());
	out.flush();

	return hash_t(buffer);
}

uint64_t MutableRelay::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	return target ? target->calc_cost(params) : 0;
}


} // solution
} // mmx
