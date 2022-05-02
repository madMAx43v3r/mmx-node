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
	for(auto& entry : memory) {
		delete entry.second;
		entry.second = nullptr;
	}
	for(auto& entry : entries) {
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

	auto var = clone(value);
	memory[key] = var;

	if(value.flags & FLAG_KEY) {
		key_map[contract][var] = dst;
	}
}

void StorageRAM::write(const addr_t& contract, const uint64_t dst, const uint64_t key, const var_t& value)
{
	const auto mapkey = std::make_pair(std::make_pair(contract, dst), key);

	auto var = clone(value);
	entries[mapkey] = var;
}

uint64_t StorageRAM::lookup(const addr_t& contract, const var_t& value) const
{
	auto iter = key_map.find(contract);
	if(iter != key_map.end()) {
		const auto& map = iter->second;
		auto iter2 = map.find(&value);
		if(iter2 != map.end()) {
			return iter2->second;
		}
	}
	return 0;
}


} // vm
} // mmx
