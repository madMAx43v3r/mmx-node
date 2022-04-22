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
#include <mmx/vm/Storage.h>

#include <set>
#include <map>
#include <limits>
#include <memory>


namespace mmx {
namespace vm {

constexpr uint32_t MEM_CONST = 0;
constexpr uint32_t MEM_EXTERN = 0x100000;
constexpr uint32_t MEM_STATIC = MEM_EXTERN + 0x100000;
constexpr uint32_t MEM_STACK = MEM_STATIC + 0x100000;
constexpr uint32_t MEM_HEAP = MEM_STACK + 0x100000;


class Engine {
public:
	std::map<uint32_t, var_t*> memory;
	std::map<std::pair<uint32_t, uint32_t>, var_t*> elements;
	std::map<std::pair<uint32_t, varptr_t>, var_t*> entries;

	std::set<uint32_t> cells_free;
	std::set<uint32_t> cells_erased;
	std::set<std::pair<uint32_t, uint32_t>> elements_erased;
	std::set<std::pair<uint32_t, varptr_t>> entries_erased;

	Engine(const addr_t& contract, std::shared_ptr<Storage> storage);

	void assign(const constvar_e dst, var_t* value);
	void assign(const uint32_t dst, var_t* value);
	void assign_entry(const uint32_t dst, var_t* key, var_t* value);
	void assign_element(const uint32_t dst, const uint32_t index, var_t* value);

	void write(const uint32_t dst, const var_t& src);

	void push_back(const uint32_t dst, const var_t& src);
	void pop_back(const uint32_t dst, const uint32_t& src);

	void erase(const uint32_t dst);

	uint32_t alloc(const uint32_t src);
	uint32_t alloc(const var_t& src);

	bool is_protected(const uint32_t address) const;

	var_t* read(const uint32_t src);
	var_t& read_fail(const uint32_t src);
	var_t& read_element(const uint32_t src, const uint32_t index);

	void copy(const uint32_t dst, const uint32_t src);

protected:
	void addref(const uint32_t dst);
	void unref(const uint32_t dst);

	var_t* clone(const var_t& src);
	var_t* create_ref(const uint32_t& src);

	void write(var_t*& var, const uint32_t* dst, const var_t& src);

	size_t copy_elements(const uint32_t dst, const uint32_t src);
	void write_element(const uint32_t dst, const uint32_t index, const var_t& src);
	void erase_element(const uint32_t dst, const uint32_t index);

	void erase(const uint32_t dst);
	void erase(var_t*& var);

	void protect_fail(const uint32_t address) const;

private:
	addr_t contract;
	std::shared_ptr<Storage> storage;

};


} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_ENGINE_H_ */
