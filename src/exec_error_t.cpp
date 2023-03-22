/*
 * exec_error_t.cpp
 *
 *  Created on: Mar 22, 2023
 *      Author: mad
 */

#include <mmx/exec_error_t.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

vnx::bool_t exec_error_t::is_valid() const
{
	return message.size() <= MAX_MESSAGE_LENGTH;
}

hash_t exec_error_t::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "code", 	code);
	write_field(out, "address", address);
	write_field(out, "line", 	line);
	write_field(out, "message", message);
	out.flush();

	return hash_t(buffer);
}


} // mmx
