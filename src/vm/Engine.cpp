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
}

void Engine::addref(const uint64_t dst)
{
	read_fail(dst).addref();
}

void Engine::unref(const uint64_t dst, const size_t count)
{
	auto& var = read_fail(dst);
	if(count > var.ref_count) {
		throw std::logic_error("unref underflow at " + to_hex(dst));
	}
	var.ref_count -= count;
	var.flags |= varflags_e::DIRTY_REF;

	if(!var.ref_count && dst >= MEM_HEAP) {
		erase(dst);
	}
}

var_t* Engine::assign(const uint64_t dst, var_t* value)
{
	if(!value) {
		return nullptr;
	}
	switch(value->type) {
		case vartype_e::ARRAY:
			((array_t*)value)->address = dst;
			break;
		case vartype_e::MAP:
			((map_t*)value)->address = dst;
			break;
	}
	auto& var = memory[dst];
	if(!var && dst >= MEM_STATIC) {
		var = storage->read(contract, dst);
	}
	return assign(var, value);
}

var_t* Engine::assign(const uint64_t dst, const uint64_t key, var_t* value)
{
	if(!value) {
		return nullptr;
	}
	switch(value->type) {
		case vartype_e::ARRAY:
		case vartype_e::MAP:
			delete value;
			throw std::logic_error("cannot assign array / map here");
	}
	const auto mapkey = std::make_pair(dst, key);
	auto& var = entries[mapkey];
	if(!var && dst >= MEM_STATIC) {
		var = storage->read(contract, dst, key);
	}
	return assign(var, value);
}

var_t* Engine::assign(var_t*& var, var_t* value)
{
	if(var) {
		if(var->flags & varflags_e::CONST) {
			delete value;
			throw std::logic_error("read-only memory");
		}
		value->flags = (var->flags & ~varflags_e::DELETED) | varflags_e::DIRTY;
		value->ref_count = var->ref_count;
		try {
			erase(var);
		} catch(...) {
			delete var; throw;
		}
	}
	var = value;
	return var;
}

uint64_t Engine::lookup(const uint64_t src)
{
	if(src < MEM_EXTERN) {
		return src;		// constant memory
	}
	return lookup(read_fail(src));
}

uint64_t Engine::lookup(const var_t* var)
{
	if(var) {
		return lookup(*var);
	}
	throw std::logic_error("!var");
}

uint64_t Engine::lookup(const varptr_t& var)
{
	return lookup(var.ptr);
}

uint64_t Engine::lookup(const var_t& var)
{
	const auto iter = key_map.find(&var);
	if(iter != key_map.end()) {
		return iter->second;
	}
	if(auto key = storage->lookup(contract, var)) {
		key_map[&read_fail(key)] = key;
		return key;
	}
	const auto key = alloc();
	const auto value = write(key, var);
	value->flags |= varflags_e::CONST | varflags_e::KEY;
	key_map[value] = key;
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
	auto& var = memory[dst];
	if(!var && dst >= MEM_STATIC) {
		var = storage->read(contract, dst);
	}
	return write(var, &dst, src);
}

var_t* Engine::write(const uint64_t dst, const varptr_t& var)
{
	return write(dst, var.ptr);
}

var_t* Engine::write(const uint64_t dst, const std::vector<varptr_t>& var)
{
	if(var.size() >= MEM_HEAP) {
		throw std::logic_error("array too large");
	}
	for(size_t i = 0; i < var.size(); ++i) {
		write_entry(dst, i, var[i]);
	}
	return write(dst, array_t(var.size()));
}

var_t* Engine::write(const uint64_t dst, const std::map<varptr_t, varptr_t>& var)
{
	for(const auto& entry : var) {
		write_key(dst, entry.first, entry.second);
	}
	return write(dst, map_t());
}

var_t* Engine::write(var_t*& var, const uint64_t* dst, const var_t& src)
{
	if(var == &src) {
		return var;
	}
	uint32_t ref_count = 0;
	varflags_e flags = varflags_e::DIRTY;
	if(var) {
		if(var->flags & varflags_e::CONST) {
			if(dst) {
				throw std::logic_error("read-only memory at " + to_hex(*dst));
			} else {
				throw std::logic_error("read-only memory");
			}
		}
		switch(src.type) {
			case vartype_e::NIL:
			case vartype_e::TRUE:
			case vartype_e::FALSE:
				switch(var->type) {
					case vartype_e::NIL:
					case vartype_e::TRUE:
					case vartype_e::FALSE:
						*var = src;
						return var;
				}
				break;
			case vartype_e::REF:
				switch(var->type) {
					case vartype_e::REF: {
						const auto ref = (ref_t*)var;
						unref(ref->address);
						*ref = (const ref_t&)src;
						addref(ref->address);
						return var;
					}
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
		flags |= (var->flags & ~varflags_e::DELETED);
		ref_count = var->ref_count;
		erase(var);
	}
	switch(src.type) {
		case vartype_e::NIL:
		case vartype_e::TRUE:
		case vartype_e::FALSE:
			var = new var_t(src.type);
			break;
		case vartype_e::REF: {
			const auto address = ((const ref_t&)src).address;
			addref(address);
			var = new ref_t(address);
			break;
		}
		case vartype_e::UINT:
			var = new uint_t((const uint_t&)src);
			break;
		case vartype_e::STRING:
		case vartype_e::BINARY:
			var = binary_t::alloc((const binary_t&)src);
			break;
		case vartype_e::ARRAY:
			if(!dst) {
				throw std::logic_error("cannot assign array here");
			}
			var = clone_array(*dst, (const array_t&)src);
			break;
		case vartype_e::MAP:
			if(!dst) {
				throw std::logic_error("cannot assign map here");
			}
			var = clone_map(*dst, (const map_t&)src);
			break;
		default:
			throw std::logic_error("invalid type");
	}
	var->flags = flags;
	var->ref_count = ref_count;
	return var;
}

void Engine::push_back(const uint64_t dst, const var_t& src)
{
	auto& var = read_fail(dst);
	if(var.type != vartype_e::ARRAY) {
		throw std::logic_error("invalid type for push_back");
	}
	auto array = (array_t&)var;
	if(array.size >= std::numeric_limits<uint32_t>::max()) {
		throw std::runtime_error("push_back overflow at " + to_hex(dst));
	}
	write_entry(dst, array.size++, src);
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
		throw std::logic_error("pop_back underflow at " + to_hex(dst));
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
	try {
		var->address = dst;
		for(uint64_t i = 0; i < src.size; ++i) {
			write_entry(dst, i, read_entry_fail(src.address, i));
		}
		var->size = src.size;
	} catch(...) {
		delete var; throw;
	}
	return var;
}

map_t* Engine::clone_map(const uint64_t dst, const map_t& src)
{
	if(src.address >= MEM_STATIC) {
		throw std::logic_error("cannot clone map from storage");
	}
	auto var = new map_t();
	try {
		var->address = dst;
		const auto begin = entries.lower_bound(std::make_pair(src.address, 0));
		const auto end = entries.lower_bound(std::make_pair(src.address + 1, 0));
		for(auto iter = begin; iter != end; ++iter) {
			if(auto value = iter->second) {
				write_entry(dst, iter->first.second, *value);
			}
		}
	} catch(...) {
		delete var; throw;
	}
	return var;
}

var_t* Engine::write_entry(const uint64_t dst, const uint64_t key, const var_t& src)
{
	const auto mapkey = std::make_pair(dst, key);
	auto& var = entries[mapkey];
	if(!var && dst >= MEM_STATIC) {
		var = storage->read(contract, dst, key);
	}
	const bool is_new = !var;
	const auto out = write(var, nullptr, src);

	if(is_new && key >= MEM_HEAP) {
		addref(key);
	}
	return out;
}

var_t* Engine::write_entry(const uint64_t dst, const uint64_t key, const varptr_t& var)
{
	if(auto value = var.ptr) {
		switch(value->type) {
			case vartype_e::ARRAY:
			case vartype_e::MAP: {
				const auto heap = alloc();
				write(heap, value);
				return write_entry(dst, key, ref_t(heap));
			}
		}
		return write_entry(dst, key, *value);
	}
	return write_entry(dst, key, var_t());
}

var_t* Engine::write_key(const uint64_t dst, const uint64_t key, const var_t& src)
{
	return write_entry(dst, lookup(key), src);
}

var_t* Engine::write_key(const uint64_t dst, const varptr_t& key, const varptr_t& var)
{
	return write_entry(dst, lookup(key), var);
}

void Engine::erase_entry(const uint64_t dst, const uint64_t key)
{
	const auto mapkey = std::make_pair(dst, key);
	const auto iter = entries.find(mapkey);
	if(iter != entries.end()) {
		if(erase_entry(iter->second, dst, key)) {
			entries.erase(iter);
		}
	}
}

bool Engine::erase_entry(var_t*& var, const uint64_t dst, const uint64_t key)
{
	const auto ret = erase(var);
	if(key >= MEM_HEAP) {
		unref(key);
	}
	return ret;
}

void Engine::erase_key(const uint64_t dst, const uint64_t key)
{
	erase_entry(dst, lookup(key));
}

void Engine::erase_entries(const uint64_t dst)
{
	const auto begin = entries.lower_bound(std::make_pair(dst, 0));
	const auto end = entries.lower_bound(std::make_pair(dst + 1, 0));
	for(auto iter = begin; iter != end;) {
		if(erase(iter->second)) {
			iter = entries.erase(iter);
		} else {
			iter++;
		}
	}
}

bool Engine::erase(const uint64_t dst)
{
	auto iter = memory.find(dst);
	if(iter != memory.end()) {
		if(auto& var = iter->second) {
			if(var->ref_count) {
				throw std::runtime_error("erase with ref_count "
						+ std::to_string(var->ref_count) + " at " + to_hex(dst));
			}
			if(var->flags & varflags_e::CONST) {
				throw std::logic_error("read-only memory at " + to_hex(dst));
			}
		}
		if(erase(iter->second)) {
			memory.erase(iter);
		} else {
			return false;
		}
	}
	return true;
}

bool Engine::erase(var_t*& var, const uint64_t* dst)
{
	if(!var) {
		return true;
	}
	switch(var->type) {
		case vartype_e::REF:
			unref(((const ref_t*)var)->address);
			break;
		case vartype_e::ARRAY:
			erase_entries(((const array_t*)var)->address);
			break;
		case vartype_e::MAP:
			erase_entries(((const map_t*)var)->address);
			break;
	}
	if(var->flags & varflags_e::STORED) {
		write(var, nullptr, var_t())->flags |= varflags_e::DELETED;
		return false;
	}
	delete var;
	var = nullptr;
	return true;
}

var_t* Engine::read(const uint64_t src)
{
	auto iter = memory.find(src);
	if(iter != memory.end()) {
		if(auto var = iter->second) {
			if(var->flags & varflags_e::DELETED) {
				return nullptr;
			}
		}
		return iter->second;
	}
	if(src >= MEM_STATIC) {
		if(auto var = storage->read(contract, src)) {
			return memory[src] = var;
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

var_t* Engine::read_entry(const uint64_t src, const uint64_t key)
{
	const auto mapkey = std::make_pair(src, key);
	const auto iter = entries.find(mapkey);
	if(iter != entries.end()) {
		if(auto var = iter->second) {
			if(var->flags & varflags_e::DELETED) {
				return nullptr;
			}
		}
		return iter->second;
	}
	if(src >= MEM_STATIC) {
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

uint64_t Engine::alloc()
{
	auto offset = read_fail<uint_t>(MEM_HEAP + NEXT_ALLOC, vartype_e::UINT);
	offset.flags |= varflags_e::DIRTY;
	if(offset.value >= std::numeric_limits<uint64_t>::max()) {
		throw std::runtime_error("out of memory");
	}
	return offset.value++;
}

void Engine::begin(const uint32_t instr_ptr)
{
	for(auto iter = memory.begin(); iter != memory.lower_bound(MEM_STACK); ++iter) {
		if(auto var = iter->second) {
			var->flags |= varflags_e::CONST;
		}
	}
	if(!read(MEM_HEAP + HAVE_INIT)) {
		write(MEM_HEAP + HAVE_INIT, var_t(vartype_e::TRUE))->pin();
		write(MEM_HEAP + NEXT_ALLOC, uint_t(MEM_HEAP + DYNAMIC_START))->pin();
		write(MEM_HEAP + BALANCE, map_t())->pin();
		write(MEM_HEAP + LOG_HISTORY, array_t())->pin();
		write(MEM_HEAP + SEND_HISTORY, array_t())->pin();
		write(MEM_HEAP + MINT_HISTORY, array_t())->pin();
	}
	frame_t frame;
	frame.instr_ptr = instr_ptr;
	call_stack.push_back(frame);
}

void Engine::run()
{
	while(!call_stack.empty()) {
		step();
	}
}

void Engine::step()
{
	if(call_stack.empty()) {
		throw std::logic_error("empty call stack");
	}
	const auto instr_ptr = call_stack.back().instr_ptr;
	if(instr_ptr >= code.size()) {
		throw std::logic_error("instr_ptr out of bounds: " + to_hex(instr_ptr) + " > " + to_hex(code.size()));
	}
	try {
		exec(code[instr_ptr]);
	} catch(const std::exception& ex) {
		throw std::runtime_error("exception at ip " + to_hex(instr_ptr) + ": " + ex.what());
	}
}

void Engine::copy(const uint64_t dst, const uint64_t src)
{
	if(dst != src) {
		write(dst, read_fail(src));
	}
}

void Engine::clone(const uint64_t dst, const uint64_t src)
{
	const auto address = alloc();
	write(address, read_fail(src));
	write(dst, ref_t(address));
}

void Engine::get(const uint64_t dst, const uint64_t addr, const uint64_t key, const opflags_e flags)
{
	const auto& var = read_fail(addr);
	switch(var.type) {
		case vartype_e::ARRAY: {
			const auto index = key;
			if(index < ((const array_t&)var).size) {
				write(dst, read_entry_fail(addr, index));
			}
			else if(flags & opflags_e::HARD_FAIL) {
				throw std::runtime_error("out of bounds");
			}
			else {
				write(dst, var_t());
			}
			break;
		}
		case vartype_e::MAP:
			write(dst, read_key(addr, key));
			break;
		default:
			if(flags & opflags_e::HARD_FAIL) {
				throw std::logic_error("invalid type");
			} else {
				write(dst, var_t());
			}
	}
}

void Engine::set(const uint64_t addr, const uint64_t key, const uint64_t src, const opflags_e flags)
{
	const auto& var = read_fail(addr);
	switch(var.type) {
		case vartype_e::ARRAY:
			const auto index = key;
			if(index < ((const array_t&)var).size) {
				write_entry(addr, index, read_fail(src));
			}
			else if(flags & opflags_e::HARD_FAIL) {
				throw std::runtime_error("out of bounds");
			}
			break;
		case vartype_e::MAP:
			write_key(addr, key, read_fail(src));
			break;
		default:
			if(flags & opflags_e::HARD_FAIL) {
				throw std::logic_error("invalid type");
			}
	}
}

void Engine::erase(const uint64_t addr, const uint64_t key, const opflags_e flags)
{
	const auto& var = read_fail(addr);
	switch(var.type) {
		case vartype_e::MAP:
			erase_key(addr, key);
			break;
		default:
			if(flags & opflags_e::HARD_FAIL) {
				throw std::logic_error("invalid type");
			}
	}
}

void Engine::concat(const uint64_t dst, const uint64_t src)
{
	const auto& dvar = read_fail(dst);
	const auto& svar = read_fail(src);
	if(dvar.type != svar.type) {
		throw std::logic_error("type mismatch");
	}
	switch(dvar.type) {
		case vartype_e::STRING:
		case vartype_e::BINARY: {
			const auto& L = (const binary_t&)dvar;
			const auto& R = (const binary_t&)svar;
			auto res = binary_t::unsafe_alloc(L.size + R.size, dvar.type);
			::memcpy(res->data(), L.data(), L.size);
			::memcpy(res->data(L.size), R.data(), R.size);
			assign(dst, res);
			break;
		}
		case vartype_e::ARRAY: {
			const auto& R = (const array_t&)svar;
			for(uint64_t i = 0; i < R.size; ++i) {
				push_back(dst, read_entry_fail(src, i));
			}
			break;
		}
		default: throw std::logic_error("invalid type");
	}
}

void Engine::memcpy(const uint64_t dst, const uint64_t src, const uint32_t count, const uint32_t offset)
{
	const auto& svar = read_fail(src);
	switch(svar.type) {
		case vartype_e::STRING:
		case vartype_e::BINARY: {
			const auto& sbin = (const binary_t&)svar;
			if(sbin.size < offset + count) {
				throw std::logic_error("out of bounds read");
			}
			auto res = binary_t::unsafe_alloc(count, svar.type);
			::memcpy(res->data(), sbin.data(offset), count);
			assign(dst, res);
			break;
		}
		default: throw std::logic_error("invalid type");
	}
}

void Engine::conv(const uint64_t dst, const uint64_t src, const uint32_t dflags, const uint32_t sflags)
{
	const auto& svar = read_fail(src);
	switch(svar.type) {
		case vartype_e::UINT: {
			const auto& sint = (const uint_t&)svar;
			switch(dflags & 0xFF) {
				case convtype_e::STRING: {
					int base = 10;
					switch((dflags >> 8) & 0xFF) {
						case convtype_e::DEFAULT: break;
						case convtype_e::BIN: base = 2; break;
						case convtype_e::OCT: base = 8; break;
						case convtype_e::DEC: base = 10; break;
						case convtype_e::HEX: base = 16; break;
						default: throw std::logic_error("invalid conversion");
					}
					assign(dst, binary_t::alloc(sint.value.str(base)));
					break;
				}
				case convtype_e::BINARY:
					assign(dst, binary_t::alloc(&sint.value, sizeof(sint.value)));
					break;
				case convtype_e::ADDRESS:
					assign(dst, binary_t::alloc(addr_t(sint.value).to_string()));
					break;
				default:
					throw std::logic_error("invalid conversion");
			}
			break;
		}
		case vartype_e::STRING: {
			const auto& sstr = (const binary_t&)svar;
			switch(dflags & 0xFF) {
				case convtype_e::UINT: {
					int base = 10;
					switch((sflags >> 8) & 0xFF) {
						case convtype_e::DEFAULT: break;
						case convtype_e::BIN: base = 2; break;
						case convtype_e::OCT: base = 8; break;
						case convtype_e::DEC: base = 10; break;
						case convtype_e::HEX: base = 16; break;
						default: throw std::logic_error("invalid conversion");
					}
					write(dst, uint_t(uint256_t((const char*)sstr.data(), base)));
					break;
				}
				case convtype_e::BINARY:
					assign(dst, binary_t::alloc(sstr, vartype_e::BINARY));
					break;
				default:
					throw std::logic_error("invalid conversion");
			}
			break;
		}
		case vartype_e::BINARY: {
			const auto& sbin = (const binary_t&)svar;
			switch(dflags & 0xFF) {
				case convtype_e::STRING: {
					assign(dst, binary_t::alloc(vnx::to_hex_string(sbin.data(), sbin.size)));
					break;
				}
				default:
					throw std::logic_error("invalid conversion");
			}
			break;
		}
		default:
			throw std::logic_error("invalid conversion");
	}
}

void Engine::log(const uint64_t level, const uint64_t msg)
{
	// TODO
}

void Engine::jump(const uint32_t instr_ptr)
{
	get_frame().instr_ptr = instr_ptr;
}

void Engine::call(const uint32_t instr_ptr, const uint32_t stack_ptr)
{
	frame_t frame;
	frame.instr_ptr = instr_ptr;
	frame.stack_ptr = get_frame().stack_ptr + stack_ptr;
	call_stack.push_back(frame);
}

bool Engine::ret()
{
	clear_stack(get_frame().stack_ptr + 1);
	call_stack.pop_back();
	return call_stack.empty();
}

const Engine::frame_t& Engine::get_frame() const
{
	if(call_stack.empty()) {
		throw std::logic_error("empty call stack");
	}
	return call_stack.back();
}

uint64_t Engine::deref(const uint64_t src)
{
	const auto& var = read_fail(src);
	if(var.type != vartype_e::REF) {
		return src;
	}
	return ((const ref_t&)var).address;
}

uint64_t Engine::deref_addr(uint32_t src, const bool flag)
{
	if(src >= MEM_STACK && src < MEM_STATIC) {
		const auto& frame = get_frame();
		if(uint64_t(frame.stack_ptr) + src >= MEM_STATIC) {
			throw std::logic_error("stack overflow");
		}
		src += frame.stack_ptr;
	}
	return flag ? deref(src) : src;
}

uint64_t Engine::deref_value(uint32_t src, const bool flag)
{
	if(flag) {
		return read_fail<uint_t>(deref_addr(src, false), vartype_e::UINT).value;
	}
	return src;
}

void Engine::exec(const instr_t& instr)
{
	switch(instr.code) {
	case opcode_e::NOP:
		break;
	case opcode_e::CLR:
		erase(	deref_addr(instr.a, instr.flags & opflags_e::REF_A));
		break;
	case opcode_e::COPY:
		copy(	deref_addr(instr.a, instr.flags & opflags_e::REF_A),
				deref_addr(instr.b, instr.flags & opflags_e::REF_B));
		break;
	case opcode_e::CLONE:
		clone(	deref_addr(instr.a, instr.flags & opflags_e::REF_A),
				deref_addr(instr.b, instr.flags & opflags_e::REF_B));
		break;
	case opcode_e::JUMP: {
		jump(	deref_value(instr.a, instr.flags & opflags_e::REF_A));
		return;
	}
	case opcode_e::JUMPI: {
		const auto dst = deref_value(instr.a, instr.flags & opflags_e::REF_A);
		const auto cond = deref_addr(instr.b, instr.flags & opflags_e::REF_B);
		const auto& var = read_fail(cond);
		if(var.type == vartype_e::TRUE) {
			jump(dst);
			return;
		}
		break;
	}
	case opcode_e::CALL:
		call(	deref_value(instr.a, instr.flags & opflags_e::REF_A),
				instr.b);
		return;
	case opcode_e::RET:
		if(ret()) {
			return;
		}
		break;
	case opcode_e::ADD: {
		const auto dst = deref_addr(instr.a, instr.flags & opflags_e::REF_A);
		const auto lhs = deref_addr(instr.b, instr.flags & opflags_e::REF_B);
		const auto rhs = deref_addr(instr.c, instr.flags & opflags_e::REF_C);
		const auto& L = read_fail<uint_t>(lhs, vartype_e::UINT).value;
		const auto& R = read_fail<uint_t>(rhs, vartype_e::UINT).value;
		const uint256_t D = L + R;
		if((instr.flags & opflags_e::CATCH_OVERFLOW) && D < L) {
			throw std::runtime_error("integer overflow");
		}
		write(dst, uint_t(D));
		break;
	}
	case opcode_e::SUB: {
		const auto dst = deref_addr(instr.a, instr.flags & opflags_e::REF_A);
		const auto lhs = deref_addr(instr.b, instr.flags & opflags_e::REF_B);
		const auto rhs = deref_addr(instr.c, instr.flags & opflags_e::REF_C);
		const auto& L = read_fail<uint_t>(lhs, vartype_e::UINT).value;
		const auto& R = read_fail<uint_t>(rhs, vartype_e::UINT).value;
		const uint256_t D = L - R;
		if((instr.flags & opflags_e::CATCH_OVERFLOW) && D > L) {
			throw std::runtime_error("integer overflow");
		}
		write(dst, uint_t(D));
		break;
	}
	case opcode_e::TYPE: {
		const auto dst = deref_addr(instr.a, instr.flags & opflags_e::REF_A);
		const auto addr = deref_addr(instr.b, instr.flags & opflags_e::REF_B);
		const auto& var = read_fail(addr);
		write(dst, uint_t(uint32_t(var.type)));
		break;
	}
	case opcode_e::SIZE: {
		const auto dst = deref_addr(instr.a, instr.flags & opflags_e::REF_A);
		const auto addr = deref_addr(instr.b, instr.flags & opflags_e::REF_B);
		const auto& var = read_fail(addr);
		switch(var.type) {
			case vartype_e::STRING:
			case vartype_e::BINARY:
				write(dst, uint_t(((const binary_t&)var).size));
				break;
			case vartype_e::ARRAY:
				write(dst, uint_t(((const array_t&)var).size));
				break;
			default:
				write(dst, var_t());
		}
		break;
	}
	case opcode_e::GET:
		get(	deref_addr(instr.a, instr.flags & opflags_e::REF_A),
				deref_addr(instr.b, instr.flags & opflags_e::REF_B),
				deref_addr(instr.c, instr.flags & opflags_e::REF_C), instr.flags);
		break;
	case opcode_e::SET:
		set(	deref_addr(instr.a, instr.flags & opflags_e::REF_A),
				deref_addr(instr.b, instr.flags & opflags_e::REF_B),
				deref_addr(instr.c, instr.flags & opflags_e::REF_C), instr.flags);
		break;
	case opcode_e::ERASE:
		erase(	deref_addr(instr.a, instr.flags & opflags_e::REF_A),
				deref_addr(instr.b, instr.flags & opflags_e::REF_B), instr.flags);
		break;
	case opcode_e::PUSH_BACK: {
		const auto dst = deref_addr(instr.a, instr.flags & opflags_e::REF_A);
		const auto src = deref_addr(instr.b, instr.flags & opflags_e::REF_B);
		push_back(dst, read_fail(src));
		break;
	}
	case opcode_e::POP_BACK:
		pop_back(	deref_addr(instr.a, instr.flags & opflags_e::REF_A),
					deref_addr(instr.b, instr.flags & opflags_e::REF_B));
		break;
	case opcode_e::CONCAT:
		concat(	deref_addr(instr.a, instr.flags & opflags_e::REF_A),
				deref_addr(instr.b, instr.flags & opflags_e::REF_B));
		break;
	case opcode_e::MEMCPY:
		memcpy(	deref_addr(instr.a, instr.flags & opflags_e::REF_A),
				deref_addr(instr.b, instr.flags & opflags_e::REF_B),
				deref_value(instr.c, instr.flags & opflags_e::REF_C),
				deref_value(instr.d, instr.flags & opflags_e::REF_D));
		break;
	case opcode_e::LOG:
		log(	deref_value(instr.a, instr.flags & opflags_e::REF_A),
				deref_addr(instr.b, instr.flags & opflags_e::REF_B));
		break;
	}
	get_frame().instr_ptr++;
}

void Engine::clear_extern(const uint32_t offset = 0)
{
	for(auto iter = memory.lower_bound(MEM_EXTERN + offset); iter != memory.lower_bound(MEM_STACK);)
	{
		if(erase(iter->second)) {
			iter = memory.erase(iter);
		} else {
			throw std::logic_error("cannot clear extern at " + to_hex(iter->first));
		}
	}
}

void Engine::clear_stack(const uint32_t offset)
{
	for(auto iter = memory.lower_bound(MEM_STACK + offset); iter != memory.lower_bound(MEM_STATIC);)
	{
		if(erase(iter->second)) {
			iter = memory.erase(iter);
		} else {
			throw std::logic_error("cannot clear stack at " + to_hex(iter->first));
		}
	}
}

void Engine::reset()
{
	key_map.clear();
	clear_stack();
}

void Engine::commit()
{
	for(auto iter = memory.lower_bound(MEM_STATIC); iter != memory.end(); ++iter)
	{
		if(auto var = iter->second) {
			if(var->flags & (varflags_e::DIRTY | varflags_e::DIRTY_REF)) {
				storage->write(contract, iter->first, *var);
			}
		}
	}
	for(auto iter = entries.lower_bound(std::make_pair(MEM_STATIC, 0)); iter != entries.end(); ++iter)
	{
		if(auto var = iter->second) {
			if(var->flags & (varflags_e::DIRTY | varflags_e::DIRTY_REF)) {
				storage->write(contract, iter->first.first, iter->first.second, *var);
			}
		}
	}
}










} // vm
} // mmx
