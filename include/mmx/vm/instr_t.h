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

enum class opcode_e : uint16_t {

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

	XOR,		// dst, lhs, rhs
	AND,		// dst, lhs, rhs
	OR,			// dst, lhs, rhs
	MIN,		// dst, lhs, rhs
	MAX,		// dst, lhs, rhs
	NOT,		// dst, src

	SHL,		// dst, src, count
	SHR,		// dst, src, count

	BW_XOR,		// dst, lhs, rhs
	BW_AND,		// dst, lhs, rhs
	BW_OR,		// dst, lhs, rhs
	BW_NOT,		// dst, src

	CMP_EQ,		// dst, lhs, rhs
	CMP_NEQ,	// dst, lhs, rhs
	CMP_LT,		// dst, lhs, rhs
	CMP_GT,		// dst, lhs, rhs

	SIZE,		// dst, addr
	GET,		// dst, addr, key
	FIND,		// dst, addr, key
	SET,		// addr, key, rhs
	ERASE,		// addr, key

	PUSH_BACK,		// addr, src
	POP_BACK,		// dst, addr

	CONCAT,		// dst, src, count, offset
	MEMCPY,		// dst, src, count, offset
	SHA256,		// dst, src

	LOG,		// level, message
	SEND,		// addr, currency, amount
	EVENT,		// name, data

	MASK = 0xFF

};

enum class opflags_e : uint16_t {

	NONE,

	CATCH_OVERFLOW = 1,
	CATCH_UNDERFLOW = 2,

	MASK = 0xFF

};

struct instr_t {

	opcode_e code = opcode_e::NOP;

	opflags_e flags = opflags_e::NONE;

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
