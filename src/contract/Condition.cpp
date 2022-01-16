/*
 * Condition.cpp
 *
 *  Created on: Jan 16, 2022
 *      Author: mad
 */

#include <mmx/contract/Condition.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace contract {

vnx::bool_t Condition::is_valid() const {
	return version == 0;
}

hash_t Condition::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_bytes(out, version);
	write_bytes(out, type);
	write_bytes(out, compare);
	write_bytes(out, value);
	write_bytes(out, currency);
	for(const auto& cond : nested) {
		write_bytes(out, cond ? cond->calc_hash() : hash_t());
	}
	out.flush();

	return hash_t(buffer);
}

uint64_t Condition::calc_min_fee(std::shared_ptr<const ChainParams> params) const {
	uint64_t more = 0;
	for(const auto& cond : nested) {
		if(cond) {
			more += cond->calc_min_fee(params);
		}
	}
	return (8 + 4 + 4 + 4 + (value ? 8 : 0) + (currency ? 32 : 0)) * params->min_txfee_byte + more;
}


} // contract
} // mmx
