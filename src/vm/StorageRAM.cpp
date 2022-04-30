/*
 * StorageRAM.cpp
 *
 *  Created on: Apr 30, 2022
 *      Author: mad
 */

#include <mmx/vm/StorageRAM.h>


namespace mmx {
namespace vm {

StorageRAM::~StorageRAM()
{
	key_map.clear();
	for(const auto& entry : memory) {
		delete entry.second;
		entry.second = nullptr;
	}
	for(const auto& entry : entries) {
		delete entry.second;
		entry.second = nullptr;
	}
}

var_t* StorageRAM::read(const addr_t& contract, const uint64_t src) const
{
	auto iter = memory.find(std::make_pair(contract, src));
	if(iter != memory.end()) {
		return iter->second;
	}
	return nullptr;
}

var_t* StorageRAM::read(const addr_t& contract, const uint64_t src, const uint64_t key) const
{
	auto iter = entries.find(std::make_pair(std::make_pair(contract, src), key));
	if(iter != entries.end()) {
		return iter->second;
	}
	return nullptr;
}

void StorageRAM::write(const addr_t& contract, const uint64_t dst, const var_t& value)
{
	const auto key = std::make_pair(contract, dst);
	if(value.flags & varflags_e::DELETED) {
		if(auto var = read(contract, dst)) {
			erase(contract, var);
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
	var->flags &= varflags_e::STORED;
	memory[key] = var;

	if(value.flags & varflags_e::KEY) {
		key_map[std::make_pair(contract, var)] = dst;
	}
}

void StorageRAM::write(const addr_t& contract, const uint64_t dst, const uint64_t key, const var_t& value)
{
	const auto mapkey = std::make_pair(std::make_pair(contract, dst), key);
	if(value.flags & varflags_e::DELETED) {
		if(auto var = read(contract, dst, key)) {
			erase(contract, var);
			entries.erase(mapkey);
		}
		return;
	}
	auto var = clone(value);
	var->flags &= varflags_e::STORED;
	entries[mapkey] = var;
}

uint64_t StorageRAM::lookup(const addr_t& contract, const var_t& value) const
{
	auto iter = key_map.find(std::make_pair(contract, &value));
	if(iter != key_map.end()) {
		return iter->second;
	}
	return 0;
}

void StorageRAM::erase_entries(const addr_t& contract, const uint64_t dst)
{
	const auto begin = entries.lower_bound(std::make_pair(std::make_pair(contract, dst), 0));
	const auto end = entries.lower_bound(std::make_pair(std::make_pair(contract, dst + 1), 0));
	for(auto iter = begin; iter != end;) {
		erase(contract, iter->second);
		iter = entries.erase(iter);
	}
}

void StorageRAM::erase(const addr_t& contract, var_t* var)
{
	switch(var->type) {
		case vartype_e::REF: {
			const auto address = ((const ref_t*)var)->address;
			if(auto var = read(contract, address)) {
				if(var->unref()) {
					erase(contract, var);
				}
			}
			break;
		}
		case vartype_e::ARRAY:
			erase_entries(contract, ((const array_t*)var)->address);
			break;
		case vartype_e::MAP:
			erase_entries(contract, ((const map_t*)var)->address);
			break;
	}
	if(var->flags & varflags_e::KEY) {
		key_map.erase(std::make_pair(contract, var));
	}
	delete var;
}


} // vm
} // mmx
