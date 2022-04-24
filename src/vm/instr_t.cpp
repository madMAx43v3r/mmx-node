/*
 * instr_t.cpp
 *
 *  Created on: Apr 23, 2022
 *      Author: mad
 */

#include <mmx/vm/instr_t.h>

#define OPCODE_INFO(op, num) g_opcode_info[opcode_e::op] = {#op, num}


namespace mmx {
namespace vm {

opcode_info_t g_opcode_info[256] = {};

const struct global_init_t {
	global_init_t() {
		OPCODE_INFO(NOP, 0);
		OPCODE_INFO(CLONE, 2);
		OPCODE_INFO(COPY, 2);
		OPCODE_INFO(BEGIN, 1);
		OPCODE_INFO(END, 0);
		OPCODE_INFO(LOOP, 1);
		OPCODE_INFO(BREAK, 1);
		OPCODE_INFO(ADD, 3);
		OPCODE_INFO(SUB, 3);
		OPCODE_INFO(MUL, 3);
		OPCODE_INFO(DIV, 3);
		OPCODE_INFO(MOD, 3);
		OPCODE_INFO(NOT, 2);
		OPCODE_INFO(XOR, 3);
		OPCODE_INFO(AND, 3);
		OPCODE_INFO(OR, 3);
		OPCODE_INFO(MIN, 3);
		OPCODE_INFO(MAX, 3);
		OPCODE_INFO(SHL, 3);
		OPCODE_INFO(SHR, 3);
		OPCODE_INFO(CMP_EQ, 3);
		OPCODE_INFO(CMP_NEQ, 3);
		OPCODE_INFO(CMP_LT, 3);
		OPCODE_INFO(CMP_GT, 3);
	}
} global_init;


const opcode_info_t& get_opcode_info(opcode_e code)
{
	return g_opcode_info[code];
}


} // vm
} // mmx
