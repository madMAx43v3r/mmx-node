/*
 * MultiSig.cpp
 *
 *  Created on: Jan 15, 2022
 *      Author: mad
 */

#include <mmx/solution/MultiSig.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace solution {

hash_t MultiSig::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", 	version);
	write_bytes(out, "solutions");
	write_bytes(out, uint64_t(solutions.size()));
	for(const auto& sol : solutions) {
		write_bytes(out, sol ? sol->calc_hash() : hash_t());
	}
	out.flush();

	return hash_t(buffer);
}

uint64_t MultiSig::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	uint64_t cost = 0;
	for(const auto& sol : solutions) {
		if(sol) {
			cost += sol->calc_cost(params);
		}
	}
	return cost;
}


} // solution
} // mmx
