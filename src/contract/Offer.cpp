/*
 * Offer.cpp
 *
 *  Created on: Apr 25, 2022
 *      Author: mad
 */

#include <mmx/contract/Offer.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace contract {

vnx::bool_t Offer::is_valid() const
{
	return Super::is_valid() && base && base->is_extendable;
}

hash_t Offer::calc_hash(const vnx::bool_t& full_hash) const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", version);
	write_field(out, "base", base ? base->calc_hash() : hash_t());
	out.flush();

	return hash_t(buffer);
}

uint64_t Offer::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	return base ? base->calc_cost(params) : 0;
}


} // contract
} // mmx
