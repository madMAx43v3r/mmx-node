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

vnx::bool_t MultiSig::is_valid() const
{
	for(const auto& entry : solutions) {
		if(!entry.second || !entry.second->is_valid()) {
			return false;
		}
	}
	return Super::is_valid();
}

hash_t MultiSig::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", 	version);
	write_field(out, "solutions");
	write_bytes(out, uint32_t(solutions.size()));
	for(const auto& sol : solutions) {
		write_bytes_cstr(out, "pair<>");
		write_bytes(out, sol.first);
		write_bytes(out, sol ? sol.second->calc_hash() : hash_t());
	}
	write_field(out, "num_required", num_required);
	out.flush();

	return hash_t(buffer);
}

uint64_t MultiSig::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	uint64_t cost = params->min_txfee_sign;
	for(const auto& entry : solutions) {
		if(auto sol = entry.second) {
			cost += sol->calc_cost(params);
		}
	}
	return cost;
}


} // solution
} // mmx
