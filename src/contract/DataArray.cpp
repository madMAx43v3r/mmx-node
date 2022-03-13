/*
 * DataArray.cpp
 *
 *  Created on: Mar 13, 2022
 *      Author: mad
 */

#include <mmx/contract/DataArray.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace contract {

vnx::bool_t DataArray::is_valid() const
{
	return Contract::is_valid() && num_bytes() <= MAX_BYTES;
}

hash_t DataArray::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_bytes(out, version);
	for(const auto& elem : array) {
		write_bytes(out, elem);
	}
	out.flush();

	return hash_t(buffer);
}

uint64_t DataArray::num_bytes() const
{
	uint64_t num_bytes = 0;
	for(const auto& elem : array) {
		num_bytes += elem.size();
	}
	return num_bytes;
}

uint64_t DataArray::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	return (8 + 4 + num_bytes()) * params->min_txfee_byte;
}

void DataArray::append(const vnx::Variant& data)
{
	array.push_back(data);
}

void DataArray::clear()
{
	array.clear();
}


} // contract
} // mmx
