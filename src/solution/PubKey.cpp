/*
 * PubKey.cpp
 *
 *  Created on: Jan 15, 2022
 *      Author: mad
 */

#include <mmx/solution/PubKey.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace solution {

hash_t PubKey::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", 	version);
	write_field(out, "pubkey",		pubkey);
	write_field(out, "signature", 	signature);
	out.flush();

	return hash_t(buffer);
}

uint64_t PubKey::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	return params->min_txfee_sign;
}


} // solution
} // mmx
