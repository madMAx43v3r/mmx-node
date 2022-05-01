/*
 * instr_t.cpp
 *
 *  Created on: Apr 23, 2022
 *      Author: mad
 */

#include <mmx/vm/instr_t.h>

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
	}
} global_init;


const opcode_info_t& get_opcode_info(opcode_e code)
{
	return g_opcode_info[code];
}


} // vm
} // mmx
