/*
 * instr_t.cpp
 *
 *  Created on: Apr 23, 2022
 *      Author: mad
 */

#include <mmx/vm/instr_t.h>
#include <vnx/vnx.h>

#include <cstring>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <unordered_map>

#define OPCODE_INFO(op, num) g_opcode_info[op] = {#op, num}


namespace mmx {
namespace vm {

opcode_info_t g_opcode_info[256] = {};

const struct global_init_t {
	global_init_t() {
		OPCODE_INFO(OP_NOP, 0);
		OPCODE_INFO(OP_CLR, 1);
		OPCODE_INFO(OP_COPY, 2);
		OPCODE_INFO(OP_CLONE, 2);
		OPCODE_INFO(OP_JUMP, 1);
		OPCODE_INFO(OP_JUMPI, 2);
		OPCODE_INFO(OP_JUMPN, 2);
		OPCODE_INFO(OP_CALL, 2);
		OPCODE_INFO(OP_RET, 0);
		OPCODE_INFO(OP_ADD, 3);
		OPCODE_INFO(OP_SUB, 3);
		OPCODE_INFO(OP_MUL, 3);
		OPCODE_INFO(OP_DIV, 3);
		OPCODE_INFO(OP_MOD, 3);
		OPCODE_INFO(OP_NOT, 2);
		OPCODE_INFO(OP_XOR, 3);
		OPCODE_INFO(OP_AND, 3);
		OPCODE_INFO(OP_OR, 3);
		OPCODE_INFO(OP_MIN, 3);
		OPCODE_INFO(OP_MAX, 3);
		OPCODE_INFO(OP_SHL, 3);
		OPCODE_INFO(OP_SHR, 3);
		OPCODE_INFO(OP_CMP_EQ, 3);
		OPCODE_INFO(OP_CMP_NEQ, 3);
		OPCODE_INFO(OP_CMP_LT, 3);
		OPCODE_INFO(OP_CMP_GT, 3);
		OPCODE_INFO(OP_CMP_LTE, 3);
		OPCODE_INFO(OP_CMP_GTE, 3);
		OPCODE_INFO(OP_TYPE, 2);
		OPCODE_INFO(OP_SIZE, 2);
		OPCODE_INFO(OP_GET, 3);
		OPCODE_INFO(OP_SET, 3);
		OPCODE_INFO(OP_ERASE, 2);
		OPCODE_INFO(OP_PUSH_BACK, 2);
		OPCODE_INFO(OP_POP_BACK, 2);
		OPCODE_INFO(OP_CONV, 4);
		OPCODE_INFO(OP_CONCAT, 3);
		OPCODE_INFO(OP_MEMCPY, 4);
		OPCODE_INFO(OP_SHA256, 2);
		OPCODE_INFO(OP_VERIFY, 4);
		OPCODE_INFO(OP_LOG, 2);
		OPCODE_INFO(OP_SEND, 4);
		OPCODE_INFO(OP_MINT, 3);
		OPCODE_INFO(OP_EVENT, 2);
		OPCODE_INFO(OP_FAIL, 2);
		OPCODE_INFO(OP_RCALL, 4);
		OPCODE_INFO(OP_CREAD, 3);
	}
} global_init;


const opcode_info_t& get_opcode_info(opcode_e code)
{
	return g_opcode_info[code];
}

void encode_symbol(
		vnx::OutputBuffer& out, const uint32_t symbol,
		const std::unordered_map<uint32_t, std::pair<uint32_t, uint8_t>>& sym_map)
{
	auto iter = sym_map.find(symbol);
	if(iter != sym_map.end()) {
		out.write(&iter->second.first, iter->second.second);
	}
	else if(symbol < 0x7FFF) {
		// raw 15-bit value
		const uint32_t tmp = (((uint32_t(1) << 15) | symbol) << 8) | 0xFF;
		out.write(&tmp, 3);
	}
	else {
		// raw 32-bit value
		const uint64_t tmp = (uint64_t(symbol) << 24) | 0xFFFFFF;
		out.write(&tmp, 7);
	}
}

std::vector<uint8_t> serialize(const std::vector<instr_t>& code)
{
	std::unordered_map<uint32_t, uint32_t> sym_count;
	for(const auto& instr : code) {
		const auto& info = get_opcode_info(instr.code);
		if(info.nargs >= 1) {
			sym_count[instr.a]++;
		}
		if(info.nargs >= 2) {
			sym_count[instr.b]++;
		}
		if(info.nargs >= 3) {
			sym_count[instr.c]++;
		}
		if(info.nargs >= 4) {
			sym_count[instr.d]++;
		}
	}
	std::vector<std::pair<uint32_t, uint32_t>> sym_list;

	for(const auto& entry : sym_count) {
		if(entry.second > 1) {
			sym_list.push_back(entry);
		}
	}
	std::sort(sym_list.begin(), sym_list.end(),
		[](const std::pair<uint32_t, uint32_t>& L, const std::pair<uint32_t, uint32_t>& R) -> bool {
			return L.second == R.second ? L.first < R.first : L.second > R.second;
		});

	sym_list.resize(std::min<size_t>(sym_list.size(), 0x7FFF + 0xFF));

	std::unordered_map<uint32_t, std::pair<uint32_t, uint8_t>> sym_map;

	for(uint32_t i = 0; i < sym_list.size(); ++i)
	{
		const auto& key = sym_list[i].first;
		if(i < 0xFF) {
			// 1-byte entry with 8-bit symbol
			sym_map[key] = std::make_pair(i, 1);
		} else {
			// 3-byte entry with 15-bit symbol
			sym_map[key] = std::make_pair(((i - 0xFF) << 8) | 0xFF, 3);
		}
	}

	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::TypeOutput out(&stream);

	vnx::write(out, uint8_t(1));					// version
	vnx::write(out, uint32_t(sym_list.size()));		// number of symbols

	for(const auto& entry : sym_list) {
		vnx::write(out, entry.first);				// symbols
	}
	vnx::write(out, uint32_t(code.size()));			// number of instructions

	for(const auto& instr : code)
	{
		out.write(&instr.code, 1);
		out.write(&instr.flags, 1);

		const auto& info = get_opcode_info(instr.code);
		if(info.nargs >= 1) {
			encode_symbol(out, instr.a, sym_map);
		}
		if(info.nargs >= 2) {
			encode_symbol(out, instr.b, sym_map);
		}
		if(info.nargs >= 3) {
			encode_symbol(out, instr.c, sym_map);
		}
		if(info.nargs >= 4) {
			encode_symbol(out, instr.d, sym_map);
		}
	}
	out.flush();

	return buffer;
}

uint32_t decode_symbol(vnx::InputBuffer& in, const std::vector<uint32_t>& sym_list)
{
	uint8_t first = 0;
	in.read(&first, 1);

	if(first < 0xFF) {
		if(first < sym_list.size()) {
			return sym_list[first];
		} else {
			throw std::logic_error("decode_symbol(): invalid 8-bit symbol: " + std::to_string(first));
		}
	}
	uint16_t second = 0;
	in.read(&second, 2);

	if(second < 0x7FFF) {
		second += 0xFF;
		if(second < sym_list.size()) {
			return sym_list[second];
		} else {
			throw std::logic_error("decode_symbol(): invalid 15-bit symbol: " + std::to_string(second));
		}
	}
	if(second < 0xFFFF) {
		// raw 15-bit value
		return second & 0x7FFF;
	}
	uint32_t value = 0;
	in.read(&value, 4);
	return value;
}

size_t deserialize(std::vector<instr_t>& code, const void* data, const size_t length)
{
	vnx::PointerInputStream stream(data, length);
	vnx::TypeInput in(&stream);

	uint8_t version = 0;
	vnx::read(in, version);

	if(version != 1) {
		throw std::logic_error("deserialize(): invalid version: " + std::to_string(version));
	}
	uint32_t sym_count = 0;
	vnx::read(in, sym_count);

	if(sym_count > 0x7FFF + 0xFF) {
		throw std::logic_error("deserialize(): invalid symbol count: " + std::to_string(sym_count));
	}
	std::vector<uint32_t> sym_list(sym_count);

	for(uint32_t i = 0; i < sym_count; ++i) {
		vnx::read(in, sym_list[i]);
	}
	uint32_t instr_count = 0;
	vnx::read(in, instr_count);

	for(uint32_t i = 0; i < instr_count; ++i)
	{
		instr_t instr;
		{
			auto buf = in.read(2);
			instr.code = opcode_e(uint8_t(buf[0]));
			instr.flags = uint8_t(buf[1]);
		}
		const auto& info = get_opcode_info(instr.code);
		if(info.nargs >= 1) {
			instr.a = decode_symbol(in, sym_list);
		}
		if(info.nargs >= 2) {
			instr.b = decode_symbol(in, sym_list);
		}
		if(info.nargs >= 3) {
			instr.c = decode_symbol(in, sym_list);
		}
		if(info.nargs >= 4) {
			instr.d = decode_symbol(in, sym_list);
		}
		code.push_back(instr);
	}
	return in.get_input_pos();
}

std::string to_string(const instr_t& instr)
{
	const auto info = get_opcode_info(instr.code);
	std::stringstream ss;
	ss << info.name << std::uppercase << std::hex << "\t";
	for(uint32_t i = 0; i < info.nargs; ++i) {
		ss << " ";
		bool ref = false;
		switch(i) {
			case 0: ref = instr.flags & OPFLAG_REF_A; break;
			case 1: ref = instr.flags & OPFLAG_REF_B; break;
			case 2: ref = instr.flags & OPFLAG_REF_C; break;
			case 3: ref = instr.flags & OPFLAG_REF_D; break;
		}
		if(ref) {
			ss << "[";
		}
		switch(i) {
			case 0: ss << "0x" << instr.a; break;
			case 1: ss << "0x" << instr.b; break;
			case 2: ss << "0x" << instr.c; break;
			case 3: ss << "0x" << instr.d; break;
		}
		if(ref) {
			ss << "]";
		}
	}
	return ss.str();
}


} // vm
} // mmx
