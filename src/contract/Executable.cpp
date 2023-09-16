/*
 * Executable.cpp
 *
 *  Created on: May 10, 2022
 *      Author: mad
 */

#include <mmx/contract/PubKey.hxx>
#include <mmx/contract/Executable.hxx>
#include <mmx/operation/Execute.hxx>
#include <mmx/write_bytes.h>
#include <mmx/utils.h>


namespace mmx {
namespace contract {

bool Executable::is_valid() const
{
	return Super::is_valid() && binary != addr_t();
}

addr_t Executable::get_external(const std::string& name) const
{
	auto iter = depends.find(name);
	if(iter == depends.end()) {
		throw std::runtime_error("no such external contract: " + name);
	}
	return iter->second;
}

hash_t Executable::calc_hash(const vnx::bool_t& full_hash) const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", 	version);
	write_field(out, "name", 		name);
	write_field(out, "symbol", 		symbol);
	write_field(out, "decimals", 	decimals);
	write_field(out, "meta_data", 	meta_data);
	write_field(out, "binary", 		binary);
	write_field(out, "init_method", init_method);
	write_field(out, "init_args", 	init_args);
	write_field(out, "depends", 	depends);
	out.flush();

	return hash_t(buffer);
}

uint64_t Executable::num_bytes(const vnx::bool_t& total) const
{
	uint64_t sum = (total ? Super::num_bytes() : 0)
			+ 32 + init_method.size() + depends.size() * 32;

	for(const auto& arg : init_args) {
		sum += arg.size();
	}
	for(const auto& entry : depends) {
		sum += entry.first.size();
	}
	return sum;
}

uint64_t Executable::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	return Super::calc_cost(params) + num_bytes(false) * params->min_txfee_byte;
}

vnx::Variant Executable::read_field(const std::string& name) const
{
	return Super::read_field(name);
}


} // contract
} // mmx
