/*
 * VirtualPlot.cpp
 *
 *  Created on: Apr 13, 2022
 *      Author: mad
 */

#include <mmx/contract/VirtualPlot.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace contract {

vnx::bool_t VirtualPlot::is_valid() const
{
	return Super::is_valid() && farmer_key != bls_pubkey_t() && (!pool_address || *pool_address != addr_t());
}

hash_t VirtualPlot::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", 		version);
	write_field(out, "farmer_key", 		farmer_key);
	write_field(out, "pool_address", 	pool_address);
	out.flush();

	return hash_t(buffer);
}

uint64_t VirtualPlot::calc_cost(std::shared_ptr<const ChainParams> params) const {
	return 0;
}

void VirtualPlot::bls_transfer(const bls_pubkey_t& new_farmer_key)
{
	farmer_key = new_farmer_key;
}


} // contract
} // mmx
