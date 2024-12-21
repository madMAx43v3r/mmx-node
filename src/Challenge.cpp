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

	write_bytes(out, get_type_hash());
	write_field(out, "base",		base);
	write_field(out, "index",		index);
	write_field(out, "challenge", 	challenge);
	write_field(out, "difficulty",	difficulty);
	write_field(out, "max_delay",	max_delay);
	out.flush();

	return hash_t(buffer);
}


} // mmx
