/*
 * instr_t.h
 *
 *  Created on: Apr 21, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_VM_INSTR_T_H_
#define INCLUDE_MMX_VM_INSTR_T_H_

#include <cstdint>
#include <string>


namespace mmx {
namespace vm {

enum opcode_e : uint8_t {

	NOP,
	CLR,		// dst
	COPY,		// dst, src
	CLONE,		// dst, src
	JUMP,		// dst
	JUMPI,		// dst, cond
	CALL,		// dst, stack_ptr
	RET,

	ADD,		// dst, lhs, rhs
	SUB,		// dst, lhs, rhs
	MUL,		// dst, lhs, rhs
	DIV,		// dst, lhs, rhs
	MOD,		// dst, lhs, rhs

	NOT,		// dst, src
	XOR,		// dst, lhs, rhs
	AND,		// dst, lhs, rhs
	OR,			// dst, lhs, rhs
	MIN,		// dst, lhs, rhs
	MAX,		// dst, lhs, rhs
	SHL,		// dst, src, count
	SHR,		// dst, src, count

	CMP_EQ,		// dst, lhs, rhs
	CMP_NEQ,	// dst, lhs, rhs
	CMP_LT,		// dst, lhs, rhs
	CMP_GT,		// dst, lhs, rhs
	CMP_LTE,	// dst, lhs, rhs
	CMP_GTE,	// dst, lhs, rhs

	TYPE,		// dst, addr
	SIZE,		// dst, addr
	GET,		// dst, addr, key
	SET,		// addr, key, src
	ERASE,		// addr, key

	PUSH_BACK,	// dst, src
	POP_BACK,	// dst, src

	CONV,		// dst, src, dflags, sflags
	CONCAT,		// dst, src
	MEMCPY,		// dst, src, count, offset
	SHA256,		// dst, src

	LOG,		// level, message
	SEND,		// addr, amount, currency
	MINT,		// addr, amount
	EVENT,		// name, data

};

enum opflags_e : uint16_t {

	REF_A = (1 << 0),
	REF_B = (1 << 1),
	REF_C = (1 << 2),
	REF_D = (1 << 3),
	HARD_FAIL = (1 << 4),
	CATCH_OVERFLOW = (1 << 5),

};

enum convtype_e : uint16_t {

	DEC,
	HEX,
	UINT,
	STRING,
	BINARY,
	ARRAY,

};

struct instr_t {

	opcode_e code = opcode_e::NOP;

	opflags_e flags = 0;

	uint32_t a = 0;
	uint32_t b = 0;
	uint32_t c = 0;
	uint32_t d = 0;

};

struct opcode_info_t {

	std::string name;

	uint32_t nargs = 0;

};

const opcode_info_t& get_opcode_info(opcode_e code);



} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_INSTR_T_H_ */
