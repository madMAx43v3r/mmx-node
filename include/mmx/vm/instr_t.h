/*
 * instr_t.h
 *
 *  Created on: Apr 21, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_VM_INSTR_T_H_
#define INCLUDE_MMX_VM_INSTR_T_H_

#include <cstdint>


namespace mmx {
namespace vm {

enum class opcode_e : uint8_t {

	NOP,
	NEW,		// dst, src
	COPY,		// dst, src
	BEGIN,		// cond
	END,
	LOOP,		// cond
	BREAK,		// cond

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

	TYPE,		// dst, addr
	SIZE,		// dst, addr
	GET,		// dst, addr, key
	SET,		// addr, key, src
	ERASE,		// addr, key

	PUSH_BACK,		// dst, src
	POP_BACK,		// dst, src

	CONVERT,	// dst, src
	CONCAT,		// dst, src, count, offset
	MEMCPY,		// dst, src, count, offset
	SHA256,		// dst, src

	LOG,		// level, message
	SEND,		// addr, amount, currency
	MINT,		// addr, amount
	EVENT,		// name, data

};

enum opflags_e : uint8_t {

	DEREF_A = 1,
	DEREF_B = 2,
	DEREF_C = 4,
	DEREF_D = 8,
	HARD_FAIL = 16,
	CATCH_OVERFLOW = 32,
	CATCH_UNDERFLOW = 64,

};

enum convflags_e : uint8_t {

	NONE,
	DEC,
	HEX,
	OCT,
	STRING,

};

struct instr_t {

	opcode_e code = opcode_e::NOP;

	opflags_e flags = 0;

	convflags_e conv_flags[2] = {};

	uint32_t a = 0;
	uint32_t b = 0;
	uint32_t c = 0;
	uint32_t d = 0;

};


} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_INSTR_T_H_ */
