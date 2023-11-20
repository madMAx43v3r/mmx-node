/*
 * encoding.cpp
 *
 *  Created on: Nov 20, 2023
 *      Author: mad
 */

#include <mmx/pos/encoding.h>


namespace mmx {
namespace pos {

std::pair<uint32_t, uint32_t> encode_symbol(const uint8_t sym)
{
	switch(sym) {
		case 0: return std::make_pair(0, 2);
		case 1: return std::make_pair(1, 2);
		case 2: return std::make_pair(2, 2);
	}
	const uint32_t index = sym / 3;
	const uint32_t mod = sym % 3;

	if(index > 15) {
		throw std::logic_error("symbol out of range");
	}
	uint32_t out = uint32_t(-1) >> (32 - 2 * index);
	out |= mod << (2 * index);
	return std::make_pair(out, 2 * index + 2);
}

std::pair<uint32_t, uint32_t> decode_symbol(const uint32_t bits)
{
	switch(bits & 3) {
		case 0: return std::make_pair(0, 2);
		case 1: return std::make_pair(1, 2);
		case 2: return std::make_pair(2, 2);
	}
	uint32_t shift = bits;

	for(uint32_t index = 0; index < 16; ++index)
	{
		const auto mod = shift & 3;
		if(mod == 3) {
			shift >>= 2;
		} else {
			return std::make_pair(3 * index + mod, 2 * index + 2);
		}
	}
	return std::make_pair(48, 32);
}

std::vector<uint64_t> encode(const std::vector<uint8_t>& symbols, uint64_t& total_bits)
{
	std::vector<uint64_t> out;

	total_bits = 0;
	uint32_t offset = 0;
	uint64_t buffer = 0;

	for(const auto sym : symbols)
	{
		const auto bits = encode_symbol(sym);
		buffer |= uint64_t(bits.first) << offset;

		const auto end = offset + bits.second;
		if(end >= 64) {
			out.push_back(buffer);
			buffer = 0;
		}
		if(end > 64) {
			buffer = bits.first >> (64 - offset);
		}
		offset = end % 64;

		total_bits += bits.second;
	}
	if(offset) {
		out.push_back(buffer);
	}
	return out;
}

std::vector<uint8_t> decode(const std::vector<uint64_t>& bit_stream, const uint64_t num_symbols)
{
	std::vector<uint8_t> out;
	out.reserve(num_symbols);

	uint32_t bits = 0;
	uint64_t offset = 0;
	uint64_t buffer = 0;

	while(out.size() < num_symbols)
	{
		if(bits <= 32) {
			const auto index = offset / 64;
			if(index < bit_stream.size()) {
				buffer |= uint64_t((bit_stream[index] >> (offset % 64)) & 0xFFFFFFFF) << bits;
				offset += 32;
				bits += 32;
			} else if(bits == 0) {
				throw std::logic_error("bit stream underflow");
			}
		}
		const auto sym = decode_symbol(buffer);
		out.push_back(sym.first);

		if(sym.second > bits) {
			throw std::logic_error("symbol decode error");
		}
		buffer >>= sym.second;
		bits -= sym.second;
	}
	return out;
}





} // pos
} // mmx
