/*
 * Challenge.cpp
 *
 *  Created on: Jun 1, 2022
 *      Author: mad
 */

#include <mmx/Challenge.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

mmx::hash_t Challenge::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	buffer.reserve(4 * 1024);

	write_bytes(out, get_type_hash());
	write_field(out, "height",		height);
	write_field(out, "challenge", 	challenge);
	write_field(out, "space_diff",	space_diff);
	out.flush();

	return hash_t(buffer);
}


} // mmx
