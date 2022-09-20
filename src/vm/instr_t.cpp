/*
 * instr_t.cpp
 *
 *  Created on: Apr 23, 2022
 *      Author: mad
 */

#include <mmx/vm/instr_t.h>

#include <cstring>
#include <stdexcept>
#include <sstream>

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
		OPCODE_INFO(OP_LOG, 2);
		OPCODE_INFO(OP_SEND, 3);
		OPCODE_INFO(OP_MINT, 2);
		OPCODE_INFO(OP_EVENT, 2);
		OPCODE_INFO(OP_FAIL, 1);
		OPCODE_INFO(OP_RCALL, 4);
	}
} global_init;


const opcode_info_t& get_opcode_info(opcode_e code)
{
	return g_opcode_info[code];
}

std::pair<uint8_t*, size_t> serialize(const std::vector<instr_t>& code)
{
	size_t length = 0;
	for(const auto& instr : code) {
		length += 2 + get_opcode_info(instr.code).nargs * 4;
	}
	auto data = (uint8_t*)::malloc(length);

	size_t offset = 0;
	for(const auto& instr : code)
	{
		data[offset] = instr.code;
		data[offset + 1] = instr.flags;
		offset += 2;

		const auto& info = get_opcode_info(instr.code);
		if(info.nargs >= 1) {
			::memcpy(data + offset, &instr.a, 4); offset += 4;
		}
		if(info.nargs >= 2) {
			::memcpy(data + offset, &instr.b, 4); offset += 4;
		}
		if(info.nargs >= 3) {
			::memcpy(data + offset, &instr.c, 4); offset += 4;
		}
		if(info.nargs >= 4) {
			::memcpy(data + offset, &instr.d, 4); offset += 4;
		}
	}
	return std::make_pair(data, offset);
}

size_t deserialize(std::vector<instr_t>& code, const void* data_, const size_t length)
{
	size_t offset = 0;
	auto data = (const uint8_t*)data_;
	while(offset < length) {
		if(length < offset + 2) {
			throw std::runtime_error("unexpected eof");
		}
		instr_t instr;
		instr.code = opcode_e(data[offset]);
		instr.flags = data[offset + 1];
		offset += 2;

		const auto& info = get_opcode_info(instr.code);
		if(info.nargs >= 1) {
			if(length < offset + 4) {
				throw std::runtime_error("unexpected eof");
			}
			::memcpy(&instr.a, data + offset, 4); offset += 4;
		}
		if(info.nargs >= 2) {
			if(length < offset + 4) {
				throw std::runtime_error("unexpected eof");
			}
			::memcpy(&instr.b, data + offset, 4); offset += 4;
		}
		if(info.nargs >= 3) {
			if(length < offset + 4) {
				throw std::runtime_error("unexpected eof");
			}
			::memcpy(&instr.c, data + offset, 4); offset += 4;
		}
		if(info.nargs >= 4) {
			if(length < offset + 4) {
				throw std::runtime_error("unexpected eof");
			}
			::memcpy(&instr.d, data + offset, 4); offset += 4;
		}
		code.push_back(instr);
	}
	return offset;
}

std::string to_string(const instr_t& instr)
{
	const auto info = get_opcode_info(instr.code);
	std::stringstream ss;
	ss << info.name << std::hex << "\t";
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
