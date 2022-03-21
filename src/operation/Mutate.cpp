/*
 * Mutate.cpp
 *
 *  Created on: Jan 19, 2022
 *      Author: mad
 */

#include <mmx/operation/Mutate.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace operation {

hash_t Mutate::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_bytes(out, version);
	write_bytes(out, address);
	write_bytes(out, method);
	out.flush();

	return hash_t(buffer);
}

uint64_t Mutate::calc_cost(std::shared_ptr<const ChainParams> params) const {
	uint32_t payload = 0;
	for(const auto& entry : method.field) {
		payload += entry.first.size();
		payload += entry.second.data.size();
	}
	return (8 + 4 + 32 + payload) * params->min_txfee_byte * 3;
}


} // operation
} // mmx
