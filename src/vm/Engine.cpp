/*
 * Engine.cpp
 *
 *  Created on: Apr 22, 2022
 *      Author: mad
 */

#include <mmx/vm/Engine.h>


namespace mmx {
namespace vm {

Engine::Engine(const uint256_t& contract, std::shared_ptr<Storage> backend)
	:	contract(contract),
		storage(backend)
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

void Engine::addref(const uint64_t dst)
{
	if(dst < MEM_STACK) {
		return;
	}
	auto& var = read_fail(dst);
	if(var.type == vartype_e::REF) {
		throw std::logic_error("addref fail at " + to_hex(dst));
	}
	var.ref_count++;
	var.flags |= varflags_e::DIRTY;
}

void Engine::unref(const uint64_t dst)
{
	if(dst < MEM_STACK) {
		return;
	}
	auto& var = read_fail(dst);
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
	assign(uint64_t(dst), value);
}

void Engine::assign(const uint64_t dst, var_t* value)
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

void Engine::assign(const uint64_t dst, const uint64_t key, var_t* value)
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

uint64_t Engine::lookup(const uint64_t src)
{
	if(src < MEM_EXTERN) {
		return src;
	}
	const auto& var = read_fail(src);
	const auto iter = key_map.find(&var);
	if(iter != key_map.end()) {
		return iter->second;
	}
	if(storage) {
		if(auto key = storage->lookup(contract, var)) {
			key_map[read(key)] = key;
			return key;
		}
	}
	const auto key = alloc();
	const auto value = write(key, var);
	value->flags |= varflags_e::CONST;
	key_map[value] = key;
	keys_added.insert(key);
	addref(key);
	return key;
}

var_t* Engine::write(const uint64_t dst, const var_t* src)
{
	if(src) {
		return write(dst, *src);
	}
	return write(dst, var_t());
}

var_t* Engine::write(const uint64_t dst, const var_t& src)
{
	return write(memory[dst], &dst, src);
}

var_t* Engine::write(var_t*& var, const uint64_t* dst, const var_t& src)
{
	if(var == &src) {
		return var;
	}
	uint64_t ref_count = 0;
	varflags_e flags = varflags_e::DIRTY;
	if(var) {
		if(var->flags & varflags_e::CONST) {
			throw std::logic_error("cannot write to read-only memory");
		}
		if(var->type != vartype_e::REF) {
			if(src.type == vartype_e::REF) {
				throw std::logic_error("cannot assign reference here");
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
						return var;
				}
				break;
			case vartype_e::UINT:
				if(var->type == vartype_e::UINT) {
					*((uint_t*)var) = ((const uint_t&)src);
					return var;
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
								return var;
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
	return var;
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

var_t* Engine::create_ref(const uint64_t& src)
{
	auto var = new var_t(vartype_e::REF);
	var->ref_addr = src;
	addref(src);
	return var;
}

void Engine::push_back(const uint64_t dst, const var_t& src)
{
	auto& var = read_fail(dst);
	if(var.type != vartype_e::ARRAY) {
		throw std::logic_error("invalid type for push_back");
	}
	auto array = (array_t&)var;
	write_entry(dst, array.size, src);
	array.size++;
	array.flags |= varflags_e::DIRTY;
}

void Engine::pop_back(const uint64_t dst, const uint64_t& src)
{
	auto& var = read_fail(src);
	if(var.type != vartype_e::ARRAY) {
		throw std::logic_error("invalid type for pop_back");
	}
	auto array = (array_t&)var;
	if(array.size == 0) {
		throw std::logic_error("pop_back on empty array");
	}
	const auto index = array.size - 1;
	write(dst, read_entry_fail(src, index));
	erase_entry(src, index);
	array.size--;
	array.flags |= varflags_e::DIRTY;
}

array_t* Engine::clone_array(const uint64_t dst, const array_t& src)
{
	auto var = new array_t();
	var->address = dst;
	for(size_t i = 0; i < src.size; ++i) {
		write_entry(dst, i, read_entry_fail(src.address, i));
	}
	var->size = src.size;
	return var;
}

map_t* Engine::clone_map(const uint64_t dst, const map_t& src)
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

void Engine::write_entry(const uint64_t dst, const uint64_t key, const var_t& src)
{
	const auto mapkey = std::make_pair(dst, key);
	write(entries[mapkey], nullptr, src);

	if(dst >= MEM_STATIC) {
		entries_erased.erase(mapkey);
	}
}

void Engine::write_key(const uint64_t dst, const uint64_t key, const var_t& src)
{
	write_entry(dst, lookup(key), src);
}

void Engine::erase_entry(const uint64_t dst, const uint64_t key)
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

void Engine::erase_key(const uint64_t dst, const uint64_t key)
{
	erase_entry(dst, lookup(key));
}

void Engine::erase_entries(const uint64_t dst)
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

var_t* Engine::read_entry(const uint64_t src, const uint64_t key)
{
	const auto mapkey = std::make_pair(src, key);
	const auto iter = entries.find(mapkey);
	if(iter != entries.end()) {
		return iter->second;
	}
	if(storage) {
		if(auto var = storage->read(contract, src, key)) {
			return entries[key] = var;
		}
	}
	return nullptr;
}

var_t& Engine::read_entry_fail(const uint64_t src, const uint64_t key)
{
	if(auto var = read_entry(src, key)) {
		return *var;
	}
	throw std::logic_error("read fail at " + to_hex(src) + "[" + std::to_string(key) + "]");
}

var_t* Engine::read_key(const uint64_t src, const uint64_t key)
{
	return read_entry(src, lookup(key));
}

var_t& Engine::read_key_fail(const uint64_t src, const uint64_t key)
{
	return read_entry_fail(src, lookup(key));
}

void Engine::erase(const uint64_t dst)
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

uint64_t Engine::alloc()
{
	auto offset = read<uint_t>(MEM_HEAP, vartype_e::UINT);
	if(!offset) {
		offset = new uint_t(MEM_HEAP + 1);
		offset->ref_count++;
		assign(MEM_HEAP, offset);
	}
	offset->flags |= varflags_e::DIRTY;
	return offset->value++;
}

bool Engine::is_protected(const uint64_t address) const
{
	return address < MEM_STACK;
}

void Engine::protect_fail(const uint64_t address) const
{
	if(is_protected(address)) {
		throw std::runtime_error("protect fail at " + to_hex(address));
	}
}

var_t* Engine::read(const uint64_t src)
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

var_t& Engine::read_fail(const uint64_t src)
{
	if(auto var = read(src)) {
		return *var;
	}
	throw std::runtime_error("read fail at " + to_hex(src));
}

void Engine::copy(const uint64_t dst, const uint64_t src)
{
	protect_fail(dst);
	if(dst != src) {
		write(dst, read_fail(src));
	}
}

void Engine::new_copy(const uint64_t dst, const uint64_t src)
{
	protect_fail(dst);
	const auto& var = read_fail(src);
	const auto addr = alloc();
	write(addr, var);
	var_t ref(vartype_e::REF);
	ref.ref_addr = addr;
	write(dst, ref);
}

uint64_t Engine::deref(const uint64_t src)
{
	const auto& var = read_fail(src);
	if(var.type != vartype_e::REF) {
		throw std::logic_error("cannot dereference " + to_hex(src));
	}
	return var.ref_addr;
}

uint64_t Engine::deref_if(const uint64_t src, const bool flag)
{
	return flag ? deref(src) : src;
}

void Engine::exec(const instr_t& instr)
{
	switch(instr.code) {
	case opcode_e::NOP:
		break;
	case opcode_e::NEW:
		new_copy(	instr.a,
					deref_if(instr.b, instr.flags & opflags_e::DEREF_B));
		break;
	case opcode_e::COPY:
		copy(	deref_if(instr.a, instr.flags & opflags_e::DEREF_A),
				deref_if(instr.b, instr.flags & opflags_e::DEREF_B));
		break;
	case opcode_e::GET: {
		const auto dst = deref_if(instr.a, instr.flags & opflags_e::DEREF_A);
		const auto addr = deref_if(instr.b, instr.flags & opflags_e::DEREF_B);
		const auto& var = read_fail(addr);
		switch(var.type) {
			case vartype_e::ARRAY: {
				uint64_t index = instr.c;
				if(instr.flags & opflags_e::DEREF_C) {
					index = read_fail<uint_t>(instr.c, vartype_e::UINT).value;
				}
				if(index < ((const array_t&)var).size) {
					write(dst, read_entry_fail(addr, index));
				}
				else if(instr.flags & opflags_e::HARD_FAIL) {
					throw std::runtime_error("out of bounds");
				}
				else {
					write(dst, var_t());
				}
				break;
			}
			case vartype_e::MAP: {
				const auto key = deref_if(instr.c, instr.flags & opflags_e::DEREF_C);
				if(instr.flags & opflags_e::HARD_FAIL) {
					write(dst, read_key_fail(addr, key));
				} else {
					write(dst, read_key(addr, key));
				}
				break;
			}
			default: throw std::logic_error("invalid type");
		}
		break;
	}
	case opcode_e::SET: {
		const auto addr = deref_if(instr.a, instr.flags & opflags_e::DEREF_A);
		const auto src = deref_if(instr.c, instr.flags & opflags_e::DEREF_C);
		const auto& var = read_fail(addr);
		switch(var.type) {
			case vartype_e::ARRAY:
				uint64_t index = instr.b;
				if(instr.flags & opflags_e::DEREF_B) {
					index = read_fail<uint_t>(instr.b, vartype_e::UINT).value;
				}
				if(index < ((const array_t&)var).size) {
					write_entry(addr, index, read_fail(src));
				}
				else if(instr.flags & opflags_e::HARD_FAIL) {
					throw std::runtime_error("out of bounds");
				}
				break;
			case vartype_e::MAP: {
				const auto key = deref_if(instr.b, instr.flags & opflags_e::DEREF_B);
				write_key(addr, key, read_fail(src));
				break;
			}
			default: throw std::logic_error("invalid type");
		}
		break;
	}
	case opcode_e::ERASE: {
		const auto addr = deref_if(instr.a, instr.flags & opflags_e::DEREF_A);
		const auto& var = read_fail(addr);
		switch(var.type) {
			case vartype_e::MAP: {
				const auto key = deref_if(instr.b, instr.flags & opflags_e::DEREF_B);
				erase_key(addr, key);
				break;
			}
			default: throw std::logic_error("invalid type");
		}
		break;
	}
	case opcode_e::PUSH_BACK: {
		const auto dst = deref_if(instr.a, instr.flags & opflags_e::DEREF_A);
		const auto src = deref_if(instr.b, instr.flags & opflags_e::DEREF_B);
		push_back(dst, read_fail(src));
		break;
	}
	case opcode_e::POP_BACK: {
		const auto dst = deref_if(instr.a, instr.flags & opflags_e::DEREF_A);
		const auto src = deref_if(instr.b, instr.flags & opflags_e::DEREF_B);
		pop_back(dst, src);
		break;
	}
	}
}











} // vm
} // mmx
