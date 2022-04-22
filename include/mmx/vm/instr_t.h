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
	FIND,		// dst, addr, key
	SET,		// addr, key, rhs
	ERASE,		// addr, key

	PUSH_BACK,		// addr, src
	POP_BACK,		// dst, addr

	CONVERT,	// dst, src
	CONCAT,		// dst, src, count, offset
	MEMCPY,		// dst, src, count, offset
	SHA256,		// dst, src

	LOG,		// level, message
	SEND,		// addr, currency, amount
	EVENT,		// name, data

	MASK = 0xFF

};

enum class opflags_e : uint8_t {

	NONE,
	DEREF_A = 1,
	DEREF_B = 2,
	DEREF_C = 4,
	DEREF_D = 8,
	CATCH_OVERFLOW = 16,
	CATCH_UNDERFLOW = 32,

	MASK = 0xFF

};

enum class convflags_e : uint8_t {

	NONE,
	DEC,
	HEX,
	OCT,
	STRING,

	MASK = 0xF

};

struct instr_t {

	opcode_e code = opcode_e::NOP;

	opflags_e flags = opflags_e::NONE;

	convflags_e conv_flags[2] = {convflags_e::NONE, convflags_e::NONE};

	union {
		uint32_t val;
		uint32_t dst;
		uint32_t addr;
		uint32_t cond;
	} a;

	union {
		uint32_t val;
		uint32_t src;
		uint32_t lhs;
		uint32_t addr;
		uint32_t key;
	} b;

	union {
		uint32_t val;
		uint32_t rhs;
		uint32_t key;
		uint32_t count;
	} c;

	union {
		uint32_t val;
		uint32_t offset;
	} d;

};


} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_INSTR_T_H_ */
