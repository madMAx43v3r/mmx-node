/*
 * Binary.cpp
 *
 *  Created on: Sep 7, 2022
 *      Author: mad
 */

#include <mmx/contract/PubKey.hxx>
#include <mmx/contract/Binary.hxx>
#include <mmx/operation/Execute.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace contract {

bool Binary::is_valid() const
{
	for(const auto& entry : methods) {
		const auto& method = entry.second;
		if(method.is_payable && method.is_const) {
			return false;
		}
	}
	return Super::is_valid();
}

hash_t Binary::calc_hash(const vnx::bool_t& full_hash) const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", 	version);
	write_field(out, "name", 		name);
	write_field(out, "fields", 		fields);
	write_field(out, "methods", 	methods);
	write_field(out, "constant", 	constant);
	write_field(out, "binary", 		binary);
	write_field(out, "line_info", 	line_info);
	write_field(out, "source_info",	source_info);
	write_field(out, "source", 		source);
	write_field(out, "compiler", 	compiler);
	write_field(out, "build_flags", build_flags);
	out.flush();

	return hash_t(buffer);
}

uint64_t Binary::calc_cost(std::shared_ptr<const ChainParams> params, const vnx::bool_t& is_read) const
{
	uint64_t payload = fields.size() * 4 + line_info.size() * 8 + source_info.size() * 8;
	for(const auto& entry : fields) {
		payload += entry.first.size();
	}
	for(const auto& entry : methods) {
		payload += entry.first.size() + entry.second.num_bytes();
	}
	for(const auto& entry : source_info) {
		payload += entry.second.first.size();
	}
	payload += name.size() + constant.size() + binary.size() + source.size() + compiler.size();

	return calc_byte_cost(params, payload, is_read);
}

vnx::optional<uint32_t> Binary::find_field(const std::string& name) const
{
	auto iter = fields.find(name);
	if(iter != fields.end()) {
		return iter->second;
	}
	return nullptr;
}

vnx::optional<method_t> Binary::find_method(const std::string& name) const
{
	auto iter = methods.find(name);
	if(iter != methods.end()) {
		return iter->second;
	}
	return nullptr;
}


} // contract
} // mmx
