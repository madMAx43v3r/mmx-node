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

constexpr uint64_t SEG_SIZE = 0x4000000;

constexpr uint64_t MEM_CONST = SEG_SIZE;
constexpr uint64_t MEM_EXTERN = MEM_CONST + SEG_SIZE;
constexpr uint64_t MEM_PARAMS = MEM_EXTERN + SEG_SIZE;
constexpr uint64_t MEM_STACK = MEM_PARAMS + 16 * SEG_SIZE;
constexpr uint64_t MEM_STATIC = MEM_STACK + SEG_SIZE;
constexpr uint64_t MEM_HEAP = uint64_t(1) << 32;


class Engine {
public:
	std::map<uint64_t, var_t*> memory;
	std::map<std::pair<uint64_t, uint64_t>, var_t*> entries;
	std::map<const var_t*, uint64_t, varptr_less_t> key_map;

	std::set<uint64_t> keys_added;
	std::set<uint64_t> cells_erased;
	std::set<std::pair<uint64_t, uint64_t>> entries_erased;

	Engine(const uint256_t& contract, std::shared_ptr<Storage> backend);

	void assign(const constvar_e dst, var_t* value);
	void assign(const uint64_t dst, var_t* value);
	void assign(const uint64_t dst, const uint64_t key, var_t* value);

	uint64_t lookup(const uint64_t src);

	var_t* write(const uint64_t dst, const var_t* src);
	var_t* write(const uint64_t dst, const var_t& src);

	void push_back(const uint64_t dst, const var_t& src);
	void pop_back(const uint64_t dst, const uint64_t& src);

	void write_key(const uint64_t dst, const uint64_t key, const var_t& src);
	void erase_key(const uint64_t dst, const uint64_t key);

	var_t* read(const uint64_t src);
	var_t& read_fail(const uint64_t src);

	var_t* read_entry(const uint64_t src, const uint64_t key);
	var_t& read_entry_fail(const uint64_t src, const uint64_t key);

	var_t* read_key(const uint64_t src, const uint64_t key);
	var_t& read_key_fail(const uint64_t src, const uint64_t key);

	void copy(const uint64_t dst, const uint64_t src);
	void new_copy(const uint64_t dst, const uint64_t src);

	uint64_t deref(const uint64_t src);
	uint64_t deref_if(const uint64_t src, const bool flag);

	void exec(const instr_t& instr);

	template<typename T>
	T* read(const uint64_t src, const vartype_e& type);
	template<typename T>
	T& read_fail(const uint64_t src, const vartype_e& type);

	bool is_protected(const uint64_t address) const;

protected:
	void addref(const uint64_t dst);
	void unref(const uint64_t dst);

	var_t* write(var_t*& var, const uint64_t* dst, const var_t& src);

	array_t* clone_array(const uint64_t dst, const array_t& src);
	map_t* clone_map(const uint64_t dst, const map_t& src);

	void write_entry(const uint64_t dst, const uint64_t key, const var_t& src);

	void erase_entry(const uint64_t dst, const uint64_t key);
	void erase_entries(const uint64_t dst);

	void erase(const uint64_t dst);
	void erase(var_t*& var);

	uint64_t alloc();

	void protect_fail(const uint64_t address) const;

private:
	uint256_t contract;
	std::shared_ptr<Storage> storage;

};


template<typename T>
T* Engine::read(const uint64_t src, const vartype_e& type)
{
	if(auto var = read(src)) {
		if(var->type != type) {
			throw std::logic_error("read type mismatch");
		}
		return (T*)var;
	}
	return nullptr;
}

template<typename T>
T& Engine::read_fail(const uint64_t src, const vartype_e& type)
{
	auto var = read_fail(src);
	if(var.type != type) {
		throw std::logic_error("read type mismatch");
	}
	return (T*)var;
}

inline std::string to_hex(const uint64_t addr) {
	std::stringstream ss;
	ss << "0x" << std::hex << addr;
	return ss.str();
}

} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_ENGINE_H_ */
