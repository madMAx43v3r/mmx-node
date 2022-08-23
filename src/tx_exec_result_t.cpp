/*
 * tx_exec_result_t.cpp
 *
 *  Created on: Aug 23, 2022
 *      Author: mad
 */

#include <mmx/tx_exec_result_t.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

vnx::bool_t tx_exec_result_t::is_valid() const
{
	return !message || message->size() <= MAX_MESSAGE_LENGTH;
}

hash_t tx_exec_result_t::calc_hash() const
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
	write_field(out, "message", 	message);
	out.flush();

	return hash_t(buffer);
}


} // mmx
