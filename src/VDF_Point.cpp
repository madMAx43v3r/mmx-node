/*
 * VDF_Point.cpp
 *
 *  Created on: Jul 7, 2024
 *      Author: mad
 */

#include <mmx/VDF_Point.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

vnx::bool_t VDF_Point::is_valid() const
{
	return content_hash == calc_hash();
}

hash_t VDF_Point::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	buffer.reserve(4 * 1024);

	write_bytes(out, get_type_hash());
	write_field(out, "start", 		start);
	write_field(out, "num_iters", 	num_iters);
	write_field(out, "input", 		input);
	write_field(out, "output", 		output);
	write_field(out, "prev", 		prev);
	write_field(out, "reward_addr", reward_addr);
	out.flush();

	return hash_t(buffer);
}


} // mmx
