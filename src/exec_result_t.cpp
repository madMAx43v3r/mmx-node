/*
 * exec_result_t.cpp
 *
 *  Created on: Aug 23, 2022
 *      Author: mad
 */

#include <mmx/exec_result_t.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

vnx::bool_t exec_result_t::is_valid() const
{
	return !error || error->is_valid();
}

hash_t exec_result_t::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "did_fail", 	did_fail);
	write_field(out, "total_cost", 	total_cost);
	write_field(out, "total_fee", 	total_fee);
	write_field(out, "inputs",		inputs);
	write_field(out, "outputs", 	outputs);
	write_field(out, "error", 		error ? error->calc_hash() : hash_t());
	out.flush();

	return hash_t(buffer);
}

std::string exec_result_t::get_error_msg() const
{
	if(did_fail) {
		if(error) {
			std::string location = "0x" + vnx::to_hex_string(error->address);
			if(error->line) {
				location += ", line " + std::to_string(*error->line);
			}
			return "exception at " + location + ": " + error->message;
		}
		return "tx failed";
	}
	return std::string();
}


} // mmx
