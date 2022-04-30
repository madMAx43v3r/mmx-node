/*
 * StorageRAM.h
 *
 *  Created on: Apr 30, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_VM_STORAGERAM_H_
#define INCLUDE_MMX_VM_STORAGERAM_H_

#include <mmx/vm/Storage.h>


namespace mmx {
namespace vm {

class StorageRAM : public Storage {
public:
	var_t* read(const addr_t& contract, const uint64_t src) const override
	{
		auto iter = memory.find(std::make_pair(contract, src));
		if(iter != memory.end()) {
			return iter->second;
		}
		return nullptr;
	}

	var_t* read(const addr_t& contract, const uint64_t src, const uint64_t key) const override
	{
		auto iter = entries.find(std::make_pair(std::make_pair(contract, src), key));
		if(iter != entries.end()) {
			return iter->second;
		}
		return nullptr;
	}

	void write(const addr_t& contract, const uint64_t dst, const var_t& value) override
	{
		const auto key = std::make_pair(contract, dst);
		if(value.flags & varflags_e::DELETED) {
			if(auto var = read(contract, dst)) {
				if(var->flags & varflags_e::KEY) {
					key_map.erase(std::make_pair(contract, var));
				}
				memory.erase(key);
			}
			return;
		}
		if(!(value.flags & varflags_e::DIRTY) && (value.flags & varflags_e::DIRTY_REF)) {
			if(auto var = read(contract, dst)) {
				var->ref_count = value.ref_count;
				return;
			}
		}
		auto var = clone(value);
		var->flags &= ~(varflags_e::DIRTY | varflags_e::DIRTY_REF);
		var->flags &= varflags_e::STORED;
		memory[key] = var;

		if(value.flags & varflags_e::KEY) {
			key_map[std::make_pair(contract, var)] = dst;
		}
	}

	void write(const addr_t& contract, const uint64_t dst, const uint64_t key, const var_t& value) override
	{
		const auto mapkey = std::make_pair(std::make_pair(contract, dst), key);
		if(value.flags & varflags_e::DELETED) {
			entries.erase(mapkey);
			return;
		}
		auto var = clone(value);
		var->flags &= ~varflags_e::DIRTY;
		var->flags &= varflags_e::STORED;
		entries[mapkey] = var;
	}

	uint64_t lookup(const addr_t& contract, const var_t& value) const override
	{
		auto iter = key_map.find(std::make_pair(contract, &value));
		if(iter != key_map.end()) {
			return iter->second;
		}
		return 0;
	}

private:
	std::map<std::pair<addr_t, uint64_t>, var_t*> memory;
	std::map<std::pair<std::pair<addr_t, uint64_t>, uint64_t>, var_t*> entries;
	std::map<std::pair<addr_t, const var_t*>, uint64_t, varptr_less_t> key_map;

};


} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_STORAGERAM_H_ */
