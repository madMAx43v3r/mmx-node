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
#include <string>
#include <sstream>


namespace mmx {
namespace vm {

constexpr uint32_t SEG_SIZE = 0x1000000;

constexpr uint32_t MEM_CONST = SEG_SIZE;
constexpr uint32_t MEM_EXTERN = MEM_CONST + SEG_SIZE;
constexpr uint32_t MEM_PARAMS = MEM_EXTERN + SEG_SIZE;
constexpr uint32_t MEM_STACK = MEM_PARAMS + SEG_SIZE;
constexpr uint32_t MEM_STATIC = MEM_STACK + SEG_SIZE;
constexpr uint32_t MEM_HEAP = MEM_STATIC + SEG_SIZE;

constexpr uint32_t MAX_ARRAY_SIZE = MEM_STATIC;


class Engine {
public:
	std::map<uint32_t, var_t*> memory;
	std::map<var_t*, uint32_t, varptr_less_t> key_map;
	std::map<std::pair<uint32_t, uint32_t>, var_t*> entries;

	std::set<uint32_t> keys_added;
	std::set<uint32_t> cells_free;
	std::set<uint32_t> cells_erased;
	std::set<std::pair<uint32_t, uint32_t>> entries_erased;

	Engine(const uint256_t& contract, std::shared_ptr<Storage> backend);

	void assign(const constvar_e dst, var_t* value);
	void assign(const uint32_t dst, var_t* value);
	void assign(const uint32_t dst, const uint32_t key, var_t* value);

	uint32_t lookup(const uint32_t src);

	var_t* write(const uint32_t dst, const var_t& src);

	void push_back(const uint32_t dst, const var_t& src);
	void pop_back(const uint32_t dst, const uint32_t& src);

	void erase(const uint32_t dst);

	bool is_protected(const uint32_t address) const;

	var_t* read(const uint32_t src);
	var_t& read_fail(const uint32_t src);
	var_t& read_entry(const uint32_t src, const uint32_t index);

	void copy(const uint32_t dst, const uint32_t src);

protected:
	void addref(const uint32_t dst);
	void unref(const uint32_t dst);

	var_t* clone(const var_t& src);
	var_t* create_ref(const uint32_t& src);

	var_t* write(var_t*& var, const uint32_t* dst, const var_t& src);

	array_t* clone_array(const uint32_t dst, const array_t& src);
	map_t* clone_map(const uint32_t dst, const map_t& src);

	void write_entry(const uint32_t dst, const uint32_t key, const var_t& src);
	void write_map_entry(const uint32_t dst, const uint32_t key, const var_t& src);

	void erase_entry(const uint32_t dst, const uint32_t key);
	void erase_entries(const uint32_t dst);

	void erase(const uint32_t dst);
	void erase(var_t*& var);

	uint32_t alloc();

	void protect_fail(const uint32_t address) const;

private:
	uint256_t contract;
	std::shared_ptr<Storage> storage;

};


inline std::string to_hex(const uint32_t addr) {
	std::stringstream ss;
	ss << "0x" << std::hex << addr;
	return ss.str();
}

} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_ENGINE_H_ */
