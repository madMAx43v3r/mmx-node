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
	assign(constvar_e::NIL, new var_t(vartype_e::NIL));
	assign(constvar_e::ZERO, new uint_t());
	assign(constvar_e::ONE, new uint_t(1));
	assign(constvar_e::TRUE, new var_t(vartype_e::TRUE));
	assign(constvar_e::FALSE, new var_t(vartype_e::FALSE));
	assign(constvar_e::STRING, binary_t::alloc(0, vartype_e::STRING));
	assign(constvar_e::BINARY, binary_t::alloc(0, vartype_e::BINARY));
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

void Engine::assign(const constvar_e dst, var_t* value)
{
	assign(uint32_t(dst), value);
}

void Engine::assign(const uint32_t dst, var_t* value)
{
	if(value) {
		switch(value->type) {
			case vartype_e::STRING:
			case vartype_e::BINARY:
				if(((const binary_t*)value)->capacity > MAX_BINARY_SIZE) {
					throw std::logic_error("capacity > MAX_BINARY_SIZE");
				}
		}
		auto& var = memory[dst];
		if(var) {
			if(dst < MEM_STACK) {
				throw std::logic_error("write once memory at " + std::to_string(dst));
			}
			erase(var);
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
		if(var == &src) {
			throw std::logic_error("write overlap at " + std::to_string(dst));
		}
	}
	write(var, src);

	if(dst >= MEM_HEAP) {
		cells_erased.erase(dst);
	}
}

void Engine::write(var_t*& var, const var_t& src)
{
	uint32_t ref_count = 0;
	if(var) {
		if(var->type != vartype_e::REF) {
			if(src.type == vartype_e::REF) {
				write(var, read_fail(src.address));
				return;
			}
			ref_count = var->ref_count;
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
			// TODO
		}
		erase(var);
		var = nullptr;
	}
	switch(src.type) {
		case vartype_e::REF:
		case vartype_e::NIL:
		case vartype_e::TRUE:
		case vartype_e::FALSE:
			var = new var_t(src.type);
			break;
		case vartype_e::UINT:
			var = new uint_t((const uint_t&)src);
			break;
		case vartype_e::STRING:
		case vartype_e::BINARY:
			var = binary_t::alloc((const binary_t&)src);
			break;
		// TODO
		default:
			throw std::logic_error("invalid src type");
	}
	if(var->type != vartype_e::REF) {
		var->ref_count = ref_count;
	}
	else if(src.type == vartype_e::REF) {
		addref(src.address);
		var->address = src.address;
	}
	var->flags |= varflags_e::DIRTY;
}

void Engine::erase(const uint32_t dst)
{
	auto iter = memory.find(dst);
	if(iter != memory.end()) {
		if(auto var = iter->second) {
			if(var->type != vartype_e::REF && var->ref_count) {
				throw std::runtime_error("erase fail at " + std::to_string(dst));
			}
			if(dst >= MEM_HEAP) {
				cells_erased.insert(dst);
			}
			erase(var);
		}
		memory.erase(iter);
	}
}

void Engine::erase(const var_t* var)
{
	if(var->type == vartype_e::REF) {
		unref(var->address);
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
