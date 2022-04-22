/*
 * Engine.h
 *
 *  Created on: Apr 21, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_VM_ENGINE_H_
#define INCLUDE_MMX_VM_ENGINE_H_

#include <mmx/vm/var_t.h>
#include <mmx/vm/instr_t.h>

#include <set>
#include <map>
#include <limits>


namespace mmx {
namespace vm {

constexpr uint32_t MEM_CONST = 0;
constexpr uint32_t MEM_EXTERN = 0x100000;
constexpr uint32_t MEM_STATIC = MEM_EXTERN + 0x100000;
constexpr uint32_t MEM_STACK = MEM_STATIC + 0x100000;
constexpr uint32_t MEM_HEAP = MEM_STACK + 0x100000;


class Engine {
public:
	uint32_t MAX_BINARY_SIZE = std::numeric_limits<uint32_t>::max();

	std::map<uint32_t, var_t*> memory;

	std::set<uint32_t> cells_free;
	std::set<uint32_t> cells_erased;

	Engine();

	void assign(const constvar_e dst, var_t* value);
	void assign(const uint32_t dst, var_t* value);
	void write(const uint32_t dst, const var_t& src);

	void erase(const uint32_t dst);

	uint32_t alloc(const uint32_t src);
	uint32_t alloc(const var_t& src);

	bool is_protected(const uint32_t address) const;

	var_t* read(const uint32_t src) const;
	var_t& read_fail(const uint32_t src) const;

	void copy(const uint32_t dst, const uint32_t src);

protected:
	void addref(const uint32_t dst);
	void unref(const uint32_t dst);

	void erase(const var_t* var);
	void write(var_t*& var, const var_t& src);

	void protect_fail(const uint32_t address) const;

};


} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_ENGINE_H_ */
