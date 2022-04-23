/*
 * Engine.cpp
 *
 *  Created on: Apr 22, 2022
 *      Author: mad
 */

#include <mmx/vm/Engine.h>


namespace mmx {
namespace vm {

Engine::Engine(const addr_t& contract, std::shared_ptr<Storage> storage)
	:	contract(contract),
		storage(storage)
{
	assign(constvar_e::NIL, new var_t(vartype_e::NIL));
	assign(constvar_e::TRUE, new var_t(vartype_e::TRUE));
	assign(constvar_e::FALSE, new var_t(vartype_e::FALSE));
	assign(constvar_e::ZERO, new uint_t());
	assign(constvar_e::ONE, new uint_t(1));
	assign(constvar_e::STRING, binary_t::alloc(0, vartype_e::STRING));
	assign(constvar_e::BINARY, binary_t::alloc(0, vartype_e::BINARY));
	assign(constvar_e::ARRAY, new array_t());
	assign(constvar_e::MAP, new map_t());
}

void Engine::addref(const uint32_t dst)
{
	const auto var = read_fail(dst);
	if(var.type == vartype_e::REF) {
		throw std::logic_error("addref fail at " + to_hex(dst));
	}
	var.ref_count++;
	var.flags |= varflags_e::DIRTY;
}

void Engine::unref(const uint32_t dst)
{
	const auto var = read_fail(dst);
	if(var.type == vartype_e::REF) {
		throw std::logic_error("unref fail at " + to_hex(dst));
	}
	if(var.ref_count == 0) {
		throw std::logic_error("unref underflow at " + to_hex(dst));
	}
	var.flags |= varflags_e::DIRTY;

	if(--var.ref_count == 0) {
		if(dst >= MEM_STATIC) {
			erase(dst);
		}
	}
}

void Engine::assign(const constvar_e dst, var_t* value)
{
	assign(uint32_t(dst), value);
}

void Engine::assign(const uint32_t dst, var_t* value)
{
	if(!value) {
		throw std::logic_error("!value");
	}
	switch(value->type) {
		case vartype_e::ARRAY:
			((const array_t*)value)->address = dst;
			break;
		case vartype_e::MAP:
			((const map_t*)value)->address = dst;
			break;
	}
	auto& var = memory[dst];
	if(var) {
		throw std::logic_error("assign overwrite at " + to_hex(dst));
	}
	var = value;
}

void Engine::assign(const uint32_t dst, const uint32_t key, var_t* value)
{
	if(!value) {
		throw std::logic_error("!value");
	}
	const auto mapkey = std::make_pair(dst, key);
	auto& var = entries[mapkey];
	if(var) {
		throw std::logic_error("assign overwrite at " + to_hex(dst) + "[" + std::to_string(key) + "]");
	}
	var = value;
}

void Engine::write(const uint32_t dst, const var_t& src)
{
	write(memory[dst], &dst, src);
}

void Engine::write(var_t*& var, const uint32_t* dst, const var_t& src)
{
	if(var == &src) {
		return;
	}
	uint32_t ref_count = 0;
	varflags_e flags = varflags_e::DIRTY;
	if(var) {
		if(var->type != vartype_e::REF) {
			if(src.type == vartype_e::REF) {
				// auto dereference src, since we cannot overwrite dst with a reference
				write(var, dst, read_fail(src.ref_addr));
				return;
			}
			flags |= var->flags;
			ref_count = var->ref_count;
		}
		switch(src.type) {
			case vartype_e::REF:
			case vartype_e::NIL:
			case vartype_e::TRUE:
			case vartype_e::FALSE:
				switch(var->type) {
					case vartype_e::REF:
						unref(var->ref_addr);
						/* no break */
					case vartype_e::NIL:
					case vartype_e::TRUE:
					case vartype_e::FALSE:
						if(src.type == vartype_e::REF) {
							addref(src.ref_addr);
							var->ref_addr = src.ref_addr;
						}
						var->type = src.type;
						var->flags |= varflags_e::DIRTY;
						return;
				}
				break;
			case vartype_e::UINT:
				if(var->type == vartype_e::UINT) {
					*((uint_t*)var) = ((const uint_t&)src);
					return;
				}
				break;
			case vartype_e::STRING:
			case vartype_e::BINARY:
				switch(var->type) {
					case vartype_e::STRING:
					case vartype_e::BINARY:
						const auto bvar = (binary_t*)var;
						const auto bsrc = (const binary_t&)src;
						if(bvar->capacity < bsrc.size * 4) {
							if(bvar->assign(bsrc)) {
								return;
							}
						}
				}
				break;
		}
		erase(var);
	}
	switch(src.type) {
		case vartype_e::NIL:
		case vartype_e::TRUE:
		case vartype_e::FALSE:
		case vartype_e::REF:
		case vartype_e::UINT:
		case vartype_e::STRING:
		case vartype_e::BINARY:
			var = clone(src);
			break;
		case vartype_e::ARRAY: {
			if(!dst) {
				throw std::logic_error("cannot assign array here");
			}
			var = clone_array(*dst, (const array_t&)src);
			break;
		}
		case vartype_e::MAP:
			if(!dst) {
				throw std::logic_error("cannot assign map here");
			}
			var = clone_map(*dst, (const map_t&)src);
			break;
	}
	if(!var) {
		throw std::logic_error("invalid src type");
	}
	if(dst) {
		cells_erased.erase(*dst);
	}
	if(var->type != vartype_e::REF) {
		var->ref_count = ref_count;
	}
	var->flags = flags;
}

var_t* Engine::clone(const var_t& src)
{
	var_t* var = nullptr;
	switch(src.type) {
		case vartype_e::NIL:
		case vartype_e::TRUE:
		case vartype_e::FALSE:
			var = new var_t(src.type);
			break;
		case vartype_e::REF:
			var = create_ref(src.ref_addr);
			break;
		case vartype_e::UINT:
			var = new uint_t((const uint_t&)src);
			break;
		case vartype_e::STRING:
		case vartype_e::BINARY:
			var = binary_t::alloc((const binary_t&)src);
			break;
	}
	return var;
}

var_t* Engine::create_ref(const uint32_t& src)
{
	auto var = new var_t(vartype_e::REF);
	var->ref_addr = src;
	addref(src);
	return var;
}

void Engine::push_back(const uint32_t dst, const var_t& src)
{
	auto var = read_fail(dst);
	if(var.type != vartype_e::ARRAY) {
		throw std::logic_error("invalid type for push_back");
	}
	auto array = (array_t&)var;
	write_entry(dst, array.size, src);
	array.size++;
	array.flags |= varflags_e::DIRTY;
}

void Engine::pop_back(const uint32_t dst, const uint32_t& src)
{
	auto var = read_fail(src);
	if(var.type != vartype_e::ARRAY) {
		throw std::logic_error("invalid type for pop_back");
	}
	auto array = (array_t&)var;
	if(array.size == 0) {
		throw std::logic_error("pop_back on empty array");
	}
	const auto index = array.size - 1;
	write(dst, read_entry(src, index));
	erase_entry(src, index);
	array.size--;
	array.flags |= varflags_e::DIRTY;
}

array_t* Engine::clone_array(const uint32_t dst, const array_t& src)
{
	auto var = new array_t();
	var->address = dst;
	for(size_t i = 0; i < src.size; ++i) {
		write_entry(dst, i, read_entry(src.address, i));
	}
	var->size = src.size;
	return var;
}

map_t* Engine::clone_map(const uint32_t dst, const map_t& src)
{
	if(src.address >= MEM_STATIC) {
		throw std::logic_error("cannot clone map from storage");
	}
	auto var = new map_t();
	var->address = dst;
	const auto begin = entries.lower_bound(std::make_pair(src.address, 0));
	const auto end = entries.upper_bound(std::make_pair(src.address + 1, 0));
	for(auto iter = begin; iter != end; ++iter) {
		if(auto value = iter->second) {
			write_entry(dst, iter->first.second, *value);
		}
	}
	return var;
}

void Engine::write_entry(const uint32_t dst, const uint32_t key, const var_t& src)
{
	const auto mapkey = std::make_pair(dst, key);
	write(entries[mapkey], nullptr, src);

	if(dst >= MEM_STATIC) {
		entries_erased.erase(mapkey);
	}
}

void Engine::erase_entry(const uint32_t dst, const uint32_t key)
{
	const auto mapkey = std::make_pair(dst, key);
	const auto iter = entries.find(mapkey);
	if(iter != entries.end()) {
		erase(iter->second);
		entries.erase(iter);
	}
	if(dst >= MEM_STATIC) {
		entries_erased.insert(mapkey);
	}
}

void Engine::erase_entries(const uint32_t dst)
{
	if(dst < MEM_STATIC) {
		const auto begin = entries.lower_bound(std::make_pair(dst, 0));
		const auto end = entries.upper_bound(std::make_pair(dst + 1, 0));
		for(auto iter = begin; iter != end; ++iter) {
			erase(iter->second);
		}
		entries.erase(begin, end);
	}
	else if(dst < MEM_HEAP) {
		throw std::logic_error("cannot delete entries at " + to_hex(dst));
	}
}

var_t& Engine::read_entry(const uint32_t src, const uint32_t key)
{
	const auto mapkey = std::make_pair(src, key);
	const auto iter = entries.find(mapkey);
	if(iter == entries.end()) {
		if(storage) {
			if(auto var = storage->read(contract, src, key)) {
				return entries[key] = var;
			}
		}
		throw std::logic_error("invalid access at " + to_hex(src) + "[" + std::to_string(key) + "]");
	}
	return iter->second;
}

void Engine::erase(const uint32_t dst)
{
	auto iter = memory.find(dst);
	if(iter != memory.end()) {
		if(auto var = iter->second) {
			if(var->type != vartype_e::REF && var->ref_count) {
				throw std::runtime_error("cannot erase value with ref_count "
						+ std::to_string(var->ref_count) + " at " + to_hex(dst));
			}
			erase(var);
		}
		memory.erase(iter);
	}
	cells_erased.insert(dst);
}

void Engine::erase(var_t*& var)
{
	if(!var) {
		return;
	}
	switch(var->type) {
		case vartype_e::REF:
			unref(var->ref_addr);
			break;
		case vartype_e::ARRAY:
		case vartype_e::MAP:
			erase_entries(((const exvar_t*)var)->address);
			break;
	}
	::delete var;
	var = nullptr;
}

uint32_t Engine::alloc(const uint32_t src)
{
	return alloc(read_fail(src));
}

uint32_t Engine::alloc(const var_t& src)
{
	// TODO
	// Note: never use address 2^31-1
}

bool Engine::is_protected(const uint32_t address) const
{
	return address < MEM_STACK;
}

void Engine::protect_fail(const uint32_t address) const
{
	if(is_protected(address)) {
		throw std::runtime_error("protect fail at " + to_hex(address));
	}
}

var_t* Engine::read(const uint32_t src)
{
	auto iter = memory.find(src);
	if(iter != memory.end()) {
		return iter->second;
	}
	if(storage) {
		if(auto var = storage->read(contract, src)) {
			assign(src, var);
			return var;
		}
	}
	return nullptr;
}

var_t& Engine::read_fail(const uint32_t src)
{
	if(auto var = read(src)) {
		return *var;
	}
	throw std::runtime_error("read fail at " + to_hex(src));
}

void Engine::copy(const uint32_t dst, const uint32_t src)
{
	protect_fail(dst);
	if(dst != src) {
		write(dst, read_fail(src));
	}
}


} // vm
} // mmx
