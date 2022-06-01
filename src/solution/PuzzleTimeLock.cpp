/*
 * PuzzleTimeLock.cpp
 *
 *  Created on: Mar 13, 2022
 *      Author: mad
 */

#include <mmx/solution/PuzzleTimeLock.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace solution {

hash_t PuzzleTimeLock::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", 	version);
	write_field(out, "puzzle",		puzzle ? puzzle->calc_hash() : hash_t());
	write_field(out, "target",		target ? target->calc_hash() : hash_t());
	out.flush();

	return hash_t(buffer);
}

uint64_t PuzzleTimeLock::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	return (puzzle ? puzzle->calc_cost(params) : 0) + (target ? target->calc_cost(params) : 0);
}


} // solution
} // mmx
