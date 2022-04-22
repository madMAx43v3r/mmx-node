/*
 * Engine.cpp
 *
 *  Created on: Apr 22, 2022
 *      Author: mad
 */

#include <mmx/vm/Engine.h>


namespace mmx {
namespace vm {

Engine::Engine()
{
	write(constvar_e::NIL, new var_t(vartype_e::NIL));
	write(constvar_e::ZERO, new uint_t());
	write(constvar_e::ONE, new uint_t(1));
	write(constvar_e::TRUE, new var_t(vartype_e::TRUE));
	write(constvar_e::FALSE, new var_t(vartype_e::FALSE));
	write(constvar_e::STRING, string_t::alloc(0));
	write(constvar_e::BINARY, binary_t::alloc(0));
}

void Engine::addref(const uint32_t dst)
{
	const auto var = read_fail(dst);
	if(var.type == vartype_e::REF) {
		throw std::logic_error("addref fail at " + std::to_string(dst));
	}
	var.ref_count++;
}

void Engine::unref(const uint32_t dst)
{
	const auto var = read_fail(dst);
	if(var.type == vartype_e::REF) {
		throw std::logic_error("unref fail at " + std::to_string(dst));
	}
	if(var.ref_count == 0) {
		throw std::logic_error("unref underflow at " + std::to_string(dst));
	}
	if(--var.ref_count == 0) {
		if(dst >= MEM_HEAP) {
			erase(dst);
		}
	}
}

void Engine::write(const constvar_e dst, var_t* value)
{
	write(uint32_t(dst), value);
}

void Engine::write(const uint32_t dst, var_t* value)
{
	if(value) {
		auto& var = memory[dst];
		if(var) {
			if(dst < MEM_STACK) {
				throw std::logic_error("write once memory at " + std::to_string(dst));
			}
			erase(dst, var);
		}
		var = value;
	} else {
		erase(dst);
	}
}

void Engine::write(const uint32_t dst, const var_t& src)
{
	auto& var = memory[dst];
	if(var) {
		if(dst < MEM_STACK) {
			throw std::logic_error("write once memory at " + std::to_string(dst));
		}
		switch(src.type) {
			case vartype_e::REF:
			case vartype_e::NIL:
			case vartype_e::TRUE:
			case vartype_e::FALSE:
				switch(var->type) {
					case vartype_e::REF:
						unref(var->address);
						/* no break */
					case vartype_e::NIL:
					case vartype_e::TRUE:
					case vartype_e::FALSE:
						if(src.type == vartype_e::REF) {
							addref(src.address);
							var->address = src.address;
						}
						var->type = src.type;
						var->flags |= varflags_e::DIRTY;
						return;
					// TODO
				}
				break;
			// TODO
		}
		erase(dst, var);
	}
	if(dst >= MEM_HEAP) {
		cells_erased.erase(dst);
	}
	switch(src.type) {
		case vartype_e::REF:
		case vartype_e::NIL:
		case vartype_e::TRUE:
		case vartype_e::FALSE:
			var = new var_t(src.type);
			break;
		// TODO
	}
	switch(src.type) {
		case vartype_e::REF:
			addref(src.address);
			var->address = src.address;
			break;
		// TODO
	}
	var->flags |= varflags_e::DIRTY;
}

void Engine::erase(const uint32_t dst)
{
	auto iter = memory.find(dst);
	if(iter != memory.end()) {
		if(auto var = iter->second) {
			erase(dst, var);
		}
		memory.erase(iter);
	}
}

void Engine::erase(const uint32_t dst, var_t* var)
{
	if(var->type == vartype_e::REF) {
		unref(var->address);
	} else if(var->ref_count) {
		throw std::runtime_error("erase fail at " + std::to_string(dst));
	}
	if(dst >= MEM_HEAP) {
		cells_erased.insert(dst);
	}
	::delete var;
}

uint32_t Engine::alloc(const uint32_t src)
{
	return alloc(read_fail(src));
}

uint32_t Engine::alloc(const var_t& src)
{
	// TODO
}

bool Engine::is_protected(const uint32_t address) const
{
	return address < MEM_STACK;
}

void Engine::protect_fail(const uint32_t address) const
{
	if(is_protected(address)) {
		throw std::runtime_error("protect fail at " + std::to_string(address));
	}
}

var_t* Engine::read(const uint32_t src) const
{
	auto iter = memory.find(src);
	if(iter != memory.end()) {
		return iter->second;
	}
	return nullptr;
}

var_t& Engine::read_fail(const uint32_t src) const
{
	if(auto var = read(src)) {
		return *var;
	}
	throw std::runtime_error("read fail at " + std::to_string(src));
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
