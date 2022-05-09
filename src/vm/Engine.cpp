/*
 * Engine.cpp
 *
 *  Created on: Apr 22, 2022
 *      Author: mad
 */

#include <mmx/vm/Engine.h>
#include <mmx/vm/StorageProxy.h>

#include <iostream>


namespace mmx {
namespace vm {

std::string to_hex(const uint64_t addr) {
	std::stringstream ss;
	ss << "0x" << std::hex << addr;
	return ss.str();
}

Engine::Engine(const addr_t& contract, std::shared_ptr<Storage> backend, bool read_only)
	:	contract(contract),
		storage(std::make_shared<StorageProxy>(backend, read_only))
{
}

Engine::~Engine()
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

void Engine::addref(const uint64_t dst)
{
	read_fail(dst).addref();
}

void Engine::unref(const uint64_t dst)
{
	auto& var = read_fail(dst);
	if(var.unref()) {
		// cannot erase stored arrays or maps
		if(!(var.flags & FLAG_STORED) || (var.type != TYPE_ARRAY && var.type != TYPE_MAP)) {
			erase(dst);
		}
	}
}

var_t* Engine::assign(const uint64_t dst, var_t* value)
{
	if(!value) {
		return nullptr;
	}
	if(have_init && dst < MEM_EXTERN) {
		delete value;
		throw std::logic_error("already initialized");
	}
	switch(value->type) {
		case TYPE_ARRAY:
			((array_t*)value)->address = dst;
			break;
		case TYPE_MAP:
			((map_t*)value)->address = dst;
			break;
		default:
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
	if(have_init && dst < MEM_EXTERN) {
		delete value;
		throw std::logic_error("already initialized");
	}
	switch(value->type) {
		case TYPE_ARRAY:
		case TYPE_MAP:
			delete value;
			throw std::logic_error("cannot assign array or map as entry");
		default:
			break;
	}
	auto& var = entries[std::make_pair(dst, key)];
	if(!var && dst >= MEM_STATIC) {
		var = storage->read(contract, dst, key);
	}
	return assign(var, value);
}

var_t* Engine::assign(var_t*& var, var_t* value)
{
	if(var) {
		if(var->flags & FLAG_CONST) {
			delete value;
			throw std::logic_error("read-only memory");
		}
		value->flags = (var->flags & ~FLAG_DELETED) | FLAG_DIRTY;
		value->ref_count = var->ref_count;
		try {
			clear(var);
			delete var;
		} catch(...) {
			delete value; throw;
		}
	}
	var = value;
	num_write++;
	num_bytes_write += num_bytes(value);
	return var;
}

uint64_t Engine::lookup(const uint64_t src, const bool read_only)
{
	return lookup(read_fail(src), read_only);
}

uint64_t Engine::lookup(const var_t* var, const bool read_only)
{
	if(var) {
		return lookup(*var, read_only);
	}
	return 0;
}

uint64_t Engine::lookup(const varptr_t& var, const bool read_only)
{
	return lookup(var.ptr, read_only);
}

uint64_t Engine::lookup(const var_t& var, const bool read_only)
{
	if(var.type == TYPE_NIL) {
		throw std::runtime_error("invalid key type");
	}
	const auto iter = key_map.find(&var);
	if(iter != key_map.end()) {
		return iter->second;
	}
	if(auto key = storage->lookup(contract, var)) {
		auto& value = read_fail(key);
		if(value.ref_count == 0) {
			throw std::logic_error("lookup(): key with ref_count == 0");
		}
		if(!(value.flags & FLAG_CONST) || !(value.flags & FLAG_KEY)) {
			throw std::logic_error("lookup(): key missing flag CONST / KEY");
		}
		key_map[&value] = key;
		return key;
	}
	if(read_only) {
		return 0;
	}
	const auto key = alloc();
	const auto value = write(key, var);
	value->flags |= FLAG_CONST | FLAG_KEY;
	value->addref();
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
	if(have_init && dst < MEM_EXTERN) {
		throw std::logic_error("already initialized");
	}
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
	auto array = assign(dst, new array_t());
	for(const auto& entry : var) {
		push_back(dst, entry);
	}
	return array;
}

var_t* Engine::write(const uint64_t dst, const std::map<varptr_t, varptr_t>& var)
{
	auto map = assign(dst, new map_t());
	for(const auto& entry : var) {
		write_key(dst, entry.first, entry.second);
	}
	return map;
}

var_t* Engine::write(var_t*& var, const uint64_t* dst, const var_t& src)
{
	if(var == &src) {
		return var;
	}
	if(var) {
		if(var->flags & FLAG_CONST) {
			if(dst) {
				throw std::logic_error("read-only memory at " + to_hex(*dst));
			} else {
				throw std::logic_error("read-only memory");
			}
		}
		var->flags |= FLAG_DIRTY;
		var->flags &= ~FLAG_DELETED;

		switch(src.type) {
			case TYPE_NIL:
			case TYPE_TRUE:
			case TYPE_FALSE:
				switch(var->type) {
					case TYPE_NIL:
					case TYPE_TRUE:
					case TYPE_FALSE:
						var->type = src.type;
						return var;
					default:
						break;
				}
				break;
			case TYPE_REF:
				if(var->type == TYPE_REF) {
					const auto ref = (ref_t*)var;
					unref(ref->address);
					ref->address = ((const ref_t&)src).address;
					addref(ref->address);
					return var;
				}
				break;
			case TYPE_UINT:
				if(var->type == TYPE_UINT) {
					((uint_t*)var)->value = ((const uint_t&)src).value;
					return var;
				}
				break;
			default:
				break;
		}
	}
	switch(src.type) {
		case TYPE_NIL:
		case TYPE_TRUE:
		case TYPE_FALSE:
			assign(var, new var_t(src.type));
			break;
		case TYPE_REF: {
			const auto address = ((const ref_t&)src).address;
			addref(address);
			assign(var, new ref_t(address));
			break;
		}
		case TYPE_UINT:
			assign(var, new uint_t(((const uint_t&)src).value));
			break;
		case TYPE_STRING:
		case TYPE_BINARY:
			assign(var, binary_t::alloc((const binary_t&)src));
			break;
		case TYPE_ARRAY:
			if(!dst) {
				throw std::logic_error("cannot assign array here");
			}
			assign(var, clone_array(*dst, (const array_t&)src));
			break;
		case TYPE_MAP:
			if(!dst) {
				throw std::logic_error("cannot assign map here");
			}
			assign(var, clone_map(*dst, (const map_t&)src));
			break;
		default:
			throw std::logic_error("invalid type");
	}
	var->flags |= FLAG_DIRTY;
	var->flags &= ~FLAG_DELETED;
	return var;
}

void Engine::push_back(const uint64_t dst, const var_t& var)
{
	if(var.type == TYPE_ARRAY) {
		const auto& array = (const array_t&)var;
		if(dst == array.address) {
			throw std::logic_error("dst == src");
		}
		for(uint32_t i = 0; i < array.size; ++i) {
			push_back(dst, read_entry_fail(array.address, i));
		}
		return;
	}
	auto& array = read_fail<array_t>(dst, TYPE_ARRAY);
	if(array.size >= std::numeric_limits<uint32_t>::max()) {
		throw std::runtime_error("push_back overflow at " + to_hex(dst));
	}
	write_entry(dst, array.size++, var);
	array.flags |= FLAG_DIRTY;
}

void Engine::push_back(const uint64_t dst, const varptr_t& var)
{
	if(auto value = var.ptr) {
		push_back(dst, *value);
	} else {
		push_back(dst, var_t());
	}
}

void Engine::push_back(const uint64_t dst, const uint64_t src)
{
	if(dst == src) {
		throw std::logic_error("dst == src");
	}
	push_back(dst, read_fail(src));
}

void Engine::pop_back(const uint64_t dst, const uint64_t& src)
{
	if(dst == src) {
		throw std::logic_error("dst == src");
	}
	auto& array = read_fail<array_t>(src, TYPE_ARRAY);
	if(array.size == 0) {
		throw std::logic_error("pop_back underflow at " + to_hex(dst));
	}
	const auto index = array.size - 1;
	write(dst, read_entry_fail(src, index));
	erase_entry(src, index);
	array.size--;
	array.flags |= FLAG_DIRTY;
}

array_t* Engine::clone_array(const uint64_t dst, const array_t& src)
{
	if(dst == src.address) {
		throw std::logic_error("dst == src");
	}
	for(uint32_t i = 0; i < src.size; ++i) {
		write_entry(dst, i, read_entry_fail(src.address, i));
	}
	auto var = new array_t();
	var->address = dst;
	var->size = src.size;
	return var;
}

map_t* Engine::clone_map(const uint64_t dst, const map_t& src)
{
	if(dst == src.address) {
		throw std::logic_error("dst == src");
	}
	if(src.address >= MEM_STATIC) {
		throw std::logic_error("cannot clone map from storage");
	}
	const auto begin = entries.lower_bound(std::make_pair(src.address, 0));
	for(auto iter = begin; iter != entries.end() && iter->first.first == src.address; ++iter) {
		if(auto value = iter->second) {
			write_entry(dst, iter->first.second, *value);
		}
	}
	auto var = new map_t();
	var->address = dst;
	return var;
}

var_t* Engine::write_entry(const uint64_t dst, const uint64_t key, const var_t& src)
{
	if(have_init && dst < MEM_EXTERN) {
		throw std::logic_error("already initialized");
	}
	switch(src.type) {
		case TYPE_ARRAY:
		case TYPE_MAP: {
			const auto heap = alloc();
			write(heap, src);
			return write_entry(dst, key, ref_t(heap));
		}
		default: break;
	}
	auto& var = entries[std::make_pair(dst, key)];
	if(!var && dst >= MEM_STATIC) {
		var = storage->read(contract, dst, key);
	}
	return write(var, nullptr, src);
}

var_t* Engine::write_entry(const uint64_t dst, const uint64_t key, const varptr_t& var)
{
	if(auto value = var.ptr) {
		return write_entry(dst, key, *value);
	}
	return write_entry(dst, key, var_t());
}

var_t* Engine::write_key(const uint64_t dst, const uint64_t key, const var_t& var)
{
	return write_entry(dst, lookup(key, false), var);
}

var_t* Engine::write_key(const uint64_t dst, const var_t& key, const var_t& var)
{
	return write_entry(dst, lookup(key, false), var);
}

var_t* Engine::write_key(const uint64_t dst, const varptr_t& key, const varptr_t& var)
{
	return write_entry(dst, lookup(key, false), var);
}

void Engine::erase_entry(const uint64_t dst, const uint64_t key)
{
	const auto iter = entries.find(std::make_pair(dst, key));
	if(iter != entries.end()) {
		erase(iter->second);
	}
}

void Engine::erase_key(const uint64_t dst, const uint64_t key)
{
	if(auto addr = lookup(key, true)) {
		erase_entry(dst, addr);
	}
}

void Engine::erase_entries(const uint64_t dst)
{
	const auto begin = entries.lower_bound(std::make_pair(dst, 0));
	for(auto iter = begin; iter != entries.end() && iter->first.first == dst; ++iter) {
		erase(iter->second);
	}
}

void Engine::erase(const uint64_t dst)
{
	auto iter = memory.find(dst);
	if(iter != memory.end()) {
		erase(iter->second);
	}
	else if(dst >= MEM_STATIC) {
		if(auto var = storage->read(contract, dst)) {
			erase(memory[dst] = var);
		}
	}
}

void Engine::erase(var_t*& var)
{
	if(var->flags & FLAG_CONST) {
		throw std::logic_error("erase on read-only memory");
	}
	if(var->ref_count) {
		throw std::runtime_error("erase with ref_count " + std::to_string(var->ref_count));
	}
	clear(var);

	auto prev = var;
	var = new var_t();
	if(var->flags & FLAG_STORED) {
		var->flags = (prev->flags | FLAG_DIRTY | FLAG_DELETED);
	} else {
		var->flags = FLAG_DELETED;
	}
	delete prev;
}

void Engine::clear(var_t* var)
{
	switch(var->type) {
		case TYPE_REF:
			unref(((const ref_t*)var)->address);
			break;
		case TYPE_ARRAY: {
			const auto dst = ((const array_t*)var)->address;
			if(var->flags & FLAG_STORED) {
				throw std::logic_error("cannot erase array in storage");
			}
			erase_entries(dst);
			break;
		}
		case TYPE_MAP: {
			const auto dst = ((const map_t*)var)->address;
			if(var->flags & FLAG_STORED) {
				throw std::logic_error("cannot erase map in storage");
			}
			erase_entries(dst);
			break;
		}
		default:
			break;
	}
}

var_t* Engine::read(const uint64_t src)
{
	auto iter = memory.find(src);
	if(iter != memory.end()) {
		if(auto var = iter->second) {
			if(var->flags & FLAG_DELETED) {
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
			if(var->flags & FLAG_DELETED) {
				return nullptr;
			}
		}
		return iter->second;
	}
	if(src >= MEM_STATIC) {
		if(auto var = storage->read(contract, src, key)) {
			return entries[mapkey] = var;
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
	if(auto addr = lookup(key, true)) {
		return read_entry(src, addr);
	}
	return nullptr;
}

var_t& Engine::read_key_fail(const uint64_t src, const uint64_t key)
{
	if(auto addr = lookup(key, true)) {
		return read_entry_fail(src, addr);
	}
	throw std::logic_error("no such key at " + to_hex(src));
}

uint64_t Engine::alloc()
{
	auto& offset = read_fail<uint_t>(MEM_HEAP + GLOBAL_NEXT_ALLOC, TYPE_UINT);
	offset.flags |= FLAG_DIRTY;
	if(offset.value == std::numeric_limits<uint64_t>::max()) {
		throw std::runtime_error("out of memory");
	}
	return offset.value++;
}

void Engine::init()
{
	if(have_init) {
		throw std::logic_error("already initialized");
	}
	for(auto iter = memory.lower_bound(1); iter != memory.lower_bound(MEM_EXTERN); ++iter) {
		key_map.emplace(iter->second, iter->first);
	}
	if(!read(MEM_HEAP + GLOBAL_HAVE_INIT)) {
		assign(MEM_HEAP + GLOBAL_HAVE_INIT, new var_t(TYPE_TRUE))->pin();
		assign(MEM_HEAP + GLOBAL_NEXT_ALLOC, new uint_t(MEM_HEAP + GLOBAL_DYNAMIC_START))->pin();
		assign(MEM_HEAP + GLOBAL_LOG_HISTORY, new array_t())->pin();
		assign(MEM_HEAP + GLOBAL_SEND_HISTORY, new array_t())->pin();
		assign(MEM_HEAP + GLOBAL_MINT_HISTORY, new array_t())->pin();
		assign(MEM_HEAP + GLOBAL_EVENT_HISTORY, new array_t())->pin();
	}
	have_init = true;
}

void Engine::begin(const uint32_t instr_ptr)
{
	call_stack.clear();

	for(auto iter = memory.begin(); iter != memory.lower_bound(MEM_STACK); ++iter) {
		if(auto var = iter->second) {
			var->flags |= FLAG_CONST;
			var->flags &= ~FLAG_DIRTY;
		}
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
	const auto instr_ptr = get_frame().instr_ptr;
	if(instr_ptr >= code.size()) {
		throw std::logic_error("instr_ptr out of bounds: " + to_hex(instr_ptr) + " > " + to_hex(code.size()));
	}
	try {
		exec(code[instr_ptr]);
	} catch(const std::exception& ex) {
		throw std::runtime_error("exception at " + to_hex(instr_ptr) + ": " + ex.what());
	}
	total_cost = num_instr * INSTR_COST + num_calls * CALL_COST
			+ num_write * WRITE_COST + num_bytes_write * WRITE_BYTE_COST
			+ storage->num_read * STOR_READ_COST + storage->num_bytes_read * STOR_READ_BYTE_COST
			+ num_bytes_sha256 * SHA256_BYTE_COST;
	if(total_cost > total_gas) {
		throw std::runtime_error("out of gas: " + std::to_string(total_cost) + " > " + std::to_string(total_gas));
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
	const auto addr = alloc();
	write(addr, read_fail(src));
	write(dst, ref_t(addr));
}

void Engine::get(const uint64_t dst, const uint64_t addr, const uint64_t key, const uint8_t flags)
{
	const auto& var = read_fail(addr);
	switch(var.type) {
		case TYPE_ARRAY: {
			const auto index = key;
			if(index < ((const array_t&)var).size) {
				write(dst, read_entry_fail(addr, index));
			}
			else if(flags & OPFLAG_HARD_FAIL) {
				throw std::runtime_error("out of bounds");
			}
			else {
				write(dst, var_t());
			}
			break;
		}
		case TYPE_MAP:
			write(dst, read_key(addr, key));
			break;
		default:
			if(flags & OPFLAG_HARD_FAIL) {
				throw std::logic_error("invalid type");
			} else {
				write(dst, var_t());
			}
	}
}

void Engine::set(const uint64_t addr, const uint64_t key, const uint64_t src, const uint8_t flags)
{
	const auto& var = read_fail(addr);
	switch(var.type) {
		case TYPE_ARRAY: {
			const auto index = key;
			if(index < ((const array_t&)var).size) {
				write_entry(addr, index, read_fail(src));
			}
			else if(flags & OPFLAG_HARD_FAIL) {
				throw std::runtime_error("out of bounds");
			}
			break;
		}
		case TYPE_MAP:
			write_key(addr, key, read_fail(src));
			break;
		default:
			if(flags & OPFLAG_HARD_FAIL) {
				throw std::logic_error("invalid type");
			}
	}
}

void Engine::erase(const uint64_t addr, const uint64_t key, const uint8_t flags)
{
	const auto& var = read_fail(addr);
	switch(var.type) {
		case TYPE_MAP:
			erase_key(addr, key);
			break;
		default:
			if(flags & OPFLAG_HARD_FAIL) {
				throw std::logic_error("invalid type");
			}
	}
}

void Engine::concat(const uint64_t dst, const uint64_t lhs, const uint64_t rhs)
{
	const auto& lvar = read_fail(lhs);
	const auto& rvar = read_fail(rhs);
	if(lvar.type != rvar.type) {
		throw std::logic_error("type mismatch");
	}
	switch(lvar.type) {
		case TYPE_STRING:
		case TYPE_BINARY: {
			const auto& L = (const binary_t&)lvar;
			const auto& R = (const binary_t&)rvar;
			const auto size = uint64_t(L.size) + R.size;
			auto res = binary_t::unsafe_alloc(size, lvar.type);
			::memcpy(res->data(), L.data(), L.size);
			::memcpy(res->data(L.size), R.data(), R.size);
			assign(dst, res);
			break;
		}
		default: throw std::logic_error("invalid type");
	}
}

void Engine::memcpy(const uint64_t dst, const uint64_t src, const uint32_t count, const uint32_t offset)
{
	const auto& svar = read_fail(src);
	switch(svar.type) {
		case TYPE_STRING:
		case TYPE_BINARY: {
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

void Engine::sha256(const uint64_t dst, const uint64_t src)
{
	const auto& svar = read_fail(src);
	switch(svar.type) {
		case TYPE_STRING:
		case TYPE_BINARY: {
			const auto& sbin = (const binary_t&)svar;
			const auto num_bytes = std::max<uint32_t>(sbin.size, 64);
			if(num_bytes * SHA256_BYTE_COST > total_gas - total_cost) {
				throw std::runtime_error("insufficient gas");
			}
			num_bytes_sha256 += num_bytes;
			write(dst, uint_t(hash_t(sbin.data(), sbin.size).to_uint256()));
			break;
		}
		default: throw std::logic_error("invalid type");
	}
}

void Engine::conv(const uint64_t dst, const uint64_t src, const uint32_t dflags, const uint32_t sflags)
{
	const auto& svar = read_fail(src);
	switch(svar.type) {
		case TYPE_NIL:
			switch(dflags & 0xFF) {
				case CONVTYPE_BOOL: write(dst, var_t(TYPE_FALSE)); break;
				case CONVTYPE_UINT: write(dst, uint_t()); break;
				default: throw std::logic_error("invalid conversion");
			}
			break;
		case TYPE_TRUE:
			switch(dflags & 0xFF) {
				case CONVTYPE_BOOL: write(dst, var_t(TYPE_TRUE)); break;
				case CONVTYPE_UINT: write(dst, uint_t(1)); break;
				default: throw std::logic_error("invalid conversion");
			}
			break;
		case TYPE_FALSE:
			switch(dflags & 0xFF) {
				case CONVTYPE_BOOL: write(dst, var_t(TYPE_FALSE)); break;
				case CONVTYPE_UINT: write(dst, uint_t()); break;
				default: throw std::logic_error("invalid conversion");
			}
			break;
		case TYPE_UINT: {
			const auto& sint = (const uint_t&)svar;
			switch(dflags & 0xFF) {
				case CONVTYPE_BOOL:
					write(dst, var_t(sint.value != uint256_0 ? TYPE_TRUE : TYPE_FALSE));
					break;
				case CONVTYPE_STRING: {
					int base = 10;
					switch((dflags >> 8) & 0xFF) {
						case CONVTYPE_DEFAULT: break;
						case CONVTYPE_BASE_2: base = 2; break;
						case CONVTYPE_BASE_8: base = 8; break;
						case CONVTYPE_BASE_10: base = 10; break;
						case CONVTYPE_BASE_16: base = 16; break;
						default: throw std::logic_error("invalid conversion");
					}
					assign(dst, binary_t::alloc(sint.value.str(base)));
					break;
				}
				case CONVTYPE_BINARY:
					assign(dst, binary_t::alloc(&sint.value, sizeof(sint.value)));
					break;
				case CONVTYPE_ADDRESS:
					assign(dst, binary_t::alloc(addr_t(sint.value).to_string()));
					break;
				default:
					throw std::logic_error("invalid conversion");
			}
			break;
		}
		case TYPE_STRING: {
			const auto& sstr = (const binary_t&)svar;
			switch(dflags & 0xFF) {
				case CONVTYPE_BOOL:
					write(dst, var_t(sstr.size ? TYPE_TRUE : TYPE_FALSE));
					break;
				case CONVTYPE_UINT: {
					int base = 10;
					switch(sflags & 0xFF) {
						case CONVTYPE_DEFAULT: break;
						case CONVTYPE_BASE_2: base = 2; break;
						case CONVTYPE_BASE_8: base = 8; break;
						case CONVTYPE_BASE_10: base = 10; break;
						case CONVTYPE_BASE_16: base = 16; break;
						default: throw std::logic_error("invalid conversion");
					}
					write(dst, uint_t(uint256_t((const char*)sstr.data(), base)));
					break;
				}
				case CONVTYPE_BINARY:
					assign(dst, binary_t::alloc(sstr, TYPE_BINARY));
					break;
				default:
					throw std::logic_error("invalid conversion");
			}
			break;
		}
		case TYPE_BINARY: {
			const auto& sbin = (const binary_t&)svar;
			switch(dflags & 0xFF) {
				case CONVTYPE_BOOL:
					write(dst, var_t(sbin.size ? TYPE_TRUE : TYPE_FALSE));
					break;
				case CONVTYPE_STRING: {
					assign(dst, binary_t::alloc(vnx::to_hex_string(sbin.data(), sbin.size)));
					break;
				}
				default:
					throw std::logic_error("invalid conversion");
			}
			break;
		}
		default:
			switch(dflags & 0xFF) {
				case CONVTYPE_BOOL: write(dst, var_t(TYPE_TRUE)); break;
				default: throw std::logic_error("invalid conversion");
			}
	}
}

void Engine::log(const uint64_t level, const uint64_t msg)
{
	const auto entry = alloc();
	write(entry, array_t());
	push_back(entry, MEM_EXTERN + EXTERN_TXID);
	push_back(entry, uint_t(level));
	push_back(entry, msg);
	push_back(MEM_HEAP + GLOBAL_LOG_HISTORY, ref_t(entry));
}

void Engine::event(const uint64_t name, const uint64_t data)
{
	const auto entry = alloc();
	write(entry, array_t());
	push_back(entry, MEM_EXTERN + EXTERN_TXID);
	push_back(entry, name);
	push_back(entry, data);
	push_back(MEM_HEAP + GLOBAL_EVENT_HISTORY, ref_t(entry));
}

void Engine::send(const uint64_t address, const uint64_t amount, const uint64_t currency)
{
	const auto value = read_fail<uint_t>(amount, TYPE_UINT).value;
	if(value >> 64) {
		throw std::runtime_error("amount too large");
	}
	auto balance = read_key<uint_t>(MEM_EXTERN + EXTERN_BALANCE, currency, TYPE_UINT);
	if(!balance || balance->value < value) {
		throw std::runtime_error("insufficient funds");
	}
	balance->value -= value;

	txout_t out;
	out.contract = read_fail<uint_t>(currency, TYPE_UINT).value;
	out.address = read_fail<uint_t>(address, TYPE_UINT).value;
	out.amount = value;
	out.sender = contract;
	outputs.push_back(out);
}

void Engine::mint(const uint64_t address, const uint64_t amount)
{
	const auto value = read_fail<uint_t>(amount, TYPE_UINT).value;
	if(value >> 64) {
		throw std::runtime_error("amount too large");
	}
	txout_t out;
	out.contract = contract;
	out.address = read_fail<uint_t>(address, TYPE_UINT).value;
	out.amount = value;
	out.sender = contract;
	mint_outputs.push_back(out);
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
	num_calls++;
}

bool Engine::ret()
{
	clear_stack(get_frame().stack_ptr + 1);
	call_stack.pop_back();
	return call_stack.empty();
}

Engine::frame_t& Engine::get_frame()
{
	if(call_stack.empty()) {
		throw std::logic_error("empty call stack");
	}
	return call_stack.back();
}

uint64_t Engine::deref(const uint64_t src)
{
	const auto& var = read_fail(src);
	if(var.type != TYPE_REF) {
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
		return read_fail<uint_t>(deref_addr(src, false), TYPE_UINT).value;
	}
	return src;
}

void Engine::exec(const instr_t& instr)
{
	num_instr++;
	switch(instr.code) {
	case OP_NOP:
		break;
	case OP_CLR:
		if(instr.flags & OPFLAG_REF_A) {
			throw std::logic_error("OPFLAG_REF_A not supported");
		}
		erase(instr.a);
		break;
	case OP_COPY:
		copy(	deref_addr(instr.a, instr.flags & OPFLAG_REF_A),
				deref_addr(instr.b, instr.flags & OPFLAG_REF_B));
		break;
	case OP_CLONE:
		clone(	deref_addr(instr.a, instr.flags & OPFLAG_REF_A),
				deref_addr(instr.b, instr.flags & OPFLAG_REF_B));
		break;
	case OP_JUMP: {
		jump(	deref_value(instr.a, instr.flags & OPFLAG_REF_A));
		return;
	}
	case OP_JUMPI: {
		const auto dst = deref_value(instr.a, instr.flags & OPFLAG_REF_A);
		const auto cond = deref_addr(instr.b, instr.flags & OPFLAG_REF_B);
		const auto& var = read_fail(cond);
		if(var.type == TYPE_TRUE) {
			jump(dst);
			return;
		}
		break;
	}
	case OP_JUMPN: {
		const auto dst = deref_value(instr.a, instr.flags & OPFLAG_REF_A);
		const auto cond = deref_addr(instr.b, instr.flags & OPFLAG_REF_B);
		const auto& var = read_fail(cond);
		if(var.type != TYPE_TRUE) {
			jump(dst);
			return;
		}
		break;
	}
	case OP_CALL:
		if(instr.flags & OPFLAG_REF_B) {
			throw std::logic_error("OPFLAG_REF_B not supported");
		}
		call(	deref_value(instr.a, instr.flags & OPFLAG_REF_A),
				instr.b);
		return;
	case OP_RET:
		if(ret()) {
			return;
		}
		break;
	case OP_ADD: {
		const auto dst = deref_addr(instr.a, instr.flags & OPFLAG_REF_A);
		const auto lhs = deref_addr(instr.b, instr.flags & OPFLAG_REF_B);
		const auto rhs = deref_addr(instr.c, instr.flags & OPFLAG_REF_C);
		const auto& L = read_fail<uint_t>(lhs, TYPE_UINT).value;
		const auto& R = read_fail<uint_t>(rhs, TYPE_UINT).value;
		const uint256_t D = L + R;
		if((instr.flags & OPFLAG_CATCH_OVERFLOW) && D < L) {
			throw std::runtime_error("integer overflow");
		}
		write(dst, uint_t(D));
		break;
	}
	case OP_SUB: {
		const auto dst = deref_addr(instr.a, instr.flags & OPFLAG_REF_A);
		const auto lhs = deref_addr(instr.b, instr.flags & OPFLAG_REF_B);
		const auto rhs = deref_addr(instr.c, instr.flags & OPFLAG_REF_C);
		const auto& L = read_fail<uint_t>(lhs, TYPE_UINT).value;
		const auto& R = read_fail<uint_t>(rhs, TYPE_UINT).value;
		const uint256_t D = L - R;
		if((instr.flags & OPFLAG_CATCH_OVERFLOW) && D > L) {
			throw std::runtime_error("integer overflow");
		}
		write(dst, uint_t(D));
		break;
	}
	case OP_MUL: {
		const auto dst = deref_addr(instr.a, instr.flags & OPFLAG_REF_A);
		const auto lhs = deref_addr(instr.b, instr.flags & OPFLAG_REF_B);
		const auto rhs = deref_addr(instr.c, instr.flags & OPFLAG_REF_C);
		const auto& L = read_fail<uint_t>(lhs, TYPE_UINT).value;
		const auto& R = read_fail<uint_t>(rhs, TYPE_UINT).value;
		const uint256_t D = L * R;
		if((instr.flags & OPFLAG_CATCH_OVERFLOW) && (D < L && D < R)) {
			throw std::runtime_error("integer overflow");
		}
		write(dst, uint_t(D));
		break;
	}
	case OP_DIV: {
		const auto dst = deref_addr(instr.a, instr.flags & OPFLAG_REF_A);
		const auto lhs = deref_addr(instr.b, instr.flags & OPFLAG_REF_B);
		const auto rhs = deref_addr(instr.c, instr.flags & OPFLAG_REF_C);
		const auto& L = read_fail<uint_t>(lhs, TYPE_UINT).value;
		const auto& R = read_fail<uint_t>(rhs, TYPE_UINT).value;
		if(R == uint256_0) {
			throw std::runtime_error("division by zero");
		}
		write(dst, uint_t(L / R));
		break;
	}
	case OP_MOD: {
		const auto dst = deref_addr(instr.a, instr.flags & OPFLAG_REF_A);
		const auto lhs = deref_addr(instr.b, instr.flags & OPFLAG_REF_B);
		const auto rhs = deref_addr(instr.c, instr.flags & OPFLAG_REF_C);
		const auto& L = read_fail<uint_t>(lhs, TYPE_UINT).value;
		const auto& R = read_fail<uint_t>(rhs, TYPE_UINT).value;
		if(R == uint256_0) {
			throw std::runtime_error("division by zero");
		}
		write(dst, uint_t(L % R));
		break;
	}
	case OP_NOT: {
		const auto dst = deref_addr(instr.a, instr.flags & OPFLAG_REF_A);
		const auto src = deref_addr(instr.b, instr.flags & OPFLAG_REF_B);
		const auto& var = read_fail(src);
		switch(var.type) {
			case TYPE_NIL: write(dst, var_t(TYPE_TRUE)); break;
			case TYPE_TRUE: write(dst, var_t(TYPE_FALSE)); break;
			case TYPE_FALSE: write(dst, var_t(TYPE_TRUE)); break;
			case TYPE_UINT:
				write(dst, uint_t(~((const uint_t&)var).value));
				break;
			default: throw std::runtime_error("invalid type");
		}
		break;
	}
	case OP_XOR: {
		const auto dst = deref_addr(instr.a, instr.flags & OPFLAG_REF_A);
		const auto lhs = deref_addr(instr.b, instr.flags & OPFLAG_REF_B);
		const auto rhs = deref_addr(instr.c, instr.flags & OPFLAG_REF_C);
		const auto& L = read_fail(lhs);
		const auto& R = read_fail(rhs);
		switch(L.type) {
			case TYPE_TRUE:
				switch(R.type) {
					case TYPE_TRUE: write(dst, var_t(TYPE_FALSE)); break;
					case TYPE_FALSE: write(dst, var_t(TYPE_TRUE)); break;
					default: throw std::runtime_error("type mismatch");
				}
				break;
			case TYPE_FALSE:
				switch(R.type) {
					case TYPE_TRUE: write(dst, var_t(TYPE_TRUE)); break;
					case TYPE_FALSE: write(dst, var_t(TYPE_FALSE)); break;
					default: throw std::runtime_error("type mismatch");
				}
				break;
			case TYPE_UINT:
				if(R.type == TYPE_UINT) {
					write(dst, uint_t(((const uint_t&)L).value ^ ((const uint_t&)R).value));
				} else {
					throw std::runtime_error("type mismatch");
				}
				break;
			default: throw std::runtime_error("invalid type");
		}
		break;
	}
	case OP_AND: {
		const auto dst = deref_addr(instr.a, instr.flags & OPFLAG_REF_A);
		const auto lhs = deref_addr(instr.b, instr.flags & OPFLAG_REF_B);
		const auto rhs = deref_addr(instr.c, instr.flags & OPFLAG_REF_C);
		const auto& L = read_fail(lhs);
		const auto& R = read_fail(rhs);
		switch(L.type) {
			case TYPE_TRUE:
				switch(R.type) {
					case TYPE_TRUE: write(dst, var_t(TYPE_TRUE)); break;
					case TYPE_FALSE: write(dst, var_t(TYPE_FALSE)); break;
					default: throw std::runtime_error("type mismatch");
				}
				break;
			case TYPE_FALSE:
				switch(R.type) {
					case TYPE_TRUE:
					case TYPE_FALSE: write(dst, var_t(TYPE_FALSE)); break;
					default: throw std::runtime_error("type mismatch");
				}
				break;
			case TYPE_UINT:
				if(R.type == TYPE_UINT) {
					write(dst, uint_t(((const uint_t&)L).value & ((const uint_t&)R).value));
				} else {
					throw std::runtime_error("type mismatch");
				}
				break;
			default: throw std::runtime_error("invalid type");
		}
		break;
	}
	case OP_OR: {
		const auto dst = deref_addr(instr.a, instr.flags & OPFLAG_REF_A);
		const auto lhs = deref_addr(instr.b, instr.flags & OPFLAG_REF_B);
		const auto rhs = deref_addr(instr.c, instr.flags & OPFLAG_REF_C);
		const auto& L = read_fail(lhs);
		const auto& R = read_fail(rhs);
		switch(L.type) {
			case TYPE_TRUE:
				switch(R.type) {
					case TYPE_TRUE:
					case TYPE_FALSE: write(dst, var_t(TYPE_TRUE)); break;
					default: throw std::runtime_error("type mismatch");
				}
				break;
			case TYPE_FALSE:
				switch(R.type) {
					case TYPE_TRUE: write(dst, var_t(TYPE_TRUE)); break;
					case TYPE_FALSE: write(dst, var_t(TYPE_FALSE)); break;
					default: throw std::runtime_error("type mismatch");
				}
				break;
			case TYPE_UINT:
				if(R.type == TYPE_UINT) {
					write(dst, uint_t(((const uint_t&)L).value | ((const uint_t&)R).value));
				} else {
					throw std::runtime_error("type mismatch");
				}
				break;
			default: throw std::runtime_error("invalid type");
		}
		break;
	}
	case OP_MIN: {
		const auto dst = deref_addr(instr.a, instr.flags & OPFLAG_REF_A);
		const auto lhs = deref_addr(instr.b, instr.flags & OPFLAG_REF_B);
		const auto rhs = deref_addr(instr.c, instr.flags & OPFLAG_REF_C);
		const auto& L = read_fail<uint_t>(lhs, TYPE_UINT).value;
		const auto& R = read_fail<uint_t>(rhs, TYPE_UINT).value;
		write(dst, uint_t(L < R ? L : R));
		break;
	}
	case OP_MAX: {
		const auto dst = deref_addr(instr.a, instr.flags & OPFLAG_REF_A);
		const auto lhs = deref_addr(instr.b, instr.flags & OPFLAG_REF_B);
		const auto rhs = deref_addr(instr.c, instr.flags & OPFLAG_REF_C);
		const auto& L = read_fail<uint_t>(lhs, TYPE_UINT).value;
		const auto& R = read_fail<uint_t>(rhs, TYPE_UINT).value;
		write(dst, uint_t(L > R ? L : R));
		break;
	}
	case OP_SHL: {
		const auto dst = deref_addr(instr.a, instr.flags & OPFLAG_REF_A);
		const auto src = deref_addr(instr.b, instr.flags & OPFLAG_REF_B);
		const auto count = deref_value(instr.c, instr.flags & OPFLAG_REF_C);
		const auto& sint = read_fail<uint_t>(src, TYPE_UINT);
		write(dst, uint_t(sint.value << count));
		break;
	}
	case OP_SHR: {
		const auto dst = deref_addr(instr.a, instr.flags & OPFLAG_REF_A);
		const auto src = deref_addr(instr.b, instr.flags & OPFLAG_REF_B);
		const auto count = deref_value(instr.c, instr.flags & OPFLAG_REF_C);
		const auto& sint = read_fail<uint_t>(src, TYPE_UINT);
		write(dst, uint_t(sint.value >> count));
		break;
	}
	case OP_CMP_EQ:
	case OP_CMP_NEQ:
	case OP_CMP_LT:
	case OP_CMP_GT:
	case OP_CMP_LTE:
	case OP_CMP_GTE: {
		const auto dst = deref_addr(instr.a, instr.flags & OPFLAG_REF_A);
		const auto lhs = deref_addr(instr.b, instr.flags & OPFLAG_REF_B);
		const auto rhs = deref_addr(instr.c, instr.flags & OPFLAG_REF_C);
		const auto cmp = compare(read_fail(lhs), read_fail(rhs));
		bool res = false;
		switch(instr.code) {
			case OP_CMP_EQ: res = (cmp == 0); break;
			case OP_CMP_NEQ: res = (cmp != 0); break;
			case OP_CMP_LT: res = (cmp < 0); break;
			case OP_CMP_GT: res = (cmp > 0); break;
			case OP_CMP_LTE: res = (cmp <= 0); break;
			case OP_CMP_GTE: res = (cmp >= 0); break;
			default: break;
		}
		write(dst, var_t(res ? TYPE_TRUE : TYPE_FALSE));
		break;
	}
	case OP_TYPE: {
		const auto dst = deref_addr(instr.a, instr.flags & OPFLAG_REF_A);
		const auto addr = deref_addr(instr.b, instr.flags & OPFLAG_REF_B);
		const auto& var = read_fail(addr);
		write(dst, uint_t(uint32_t(var.type)));
		break;
	}
	case OP_SIZE: {
		const auto dst = deref_addr(instr.a, instr.flags & OPFLAG_REF_A);
		const auto addr = deref_addr(instr.b, instr.flags & OPFLAG_REF_B);
		const auto& var = read_fail(addr);
		switch(var.type) {
			case TYPE_STRING:
			case TYPE_BINARY:
				write(dst, uint_t(((const binary_t&)var).size));
				break;
			case TYPE_ARRAY:
				write(dst, uint_t(((const array_t&)var).size));
				break;
			default:
				write(dst, var_t());
		}
		break;
	}
	case OP_GET:
		get(	deref_addr(instr.a, instr.flags & OPFLAG_REF_A),
				deref_addr(instr.b, instr.flags & OPFLAG_REF_B),
				deref_addr(instr.c, instr.flags & OPFLAG_REF_C), instr.flags);
		break;
	case OP_SET:
		set(	deref_addr(instr.a, instr.flags & OPFLAG_REF_A),
				deref_addr(instr.b, instr.flags & OPFLAG_REF_B),
				deref_addr(instr.c, instr.flags & OPFLAG_REF_C), instr.flags);
		break;
	case OP_ERASE:
		erase(	deref_addr(instr.a, instr.flags & OPFLAG_REF_A),
				deref_addr(instr.b, instr.flags & OPFLAG_REF_B), instr.flags);
		break;
	case OP_PUSH_BACK:
		push_back(	deref_addr(instr.a, instr.flags & OPFLAG_REF_A),
					deref_addr(instr.b, instr.flags & OPFLAG_REF_B));
		break;
	case OP_POP_BACK:
		pop_back(	deref_addr(instr.a, instr.flags & OPFLAG_REF_A),
					deref_addr(instr.b, instr.flags & OPFLAG_REF_B));
		break;
	case OP_CONV:
		conv(	deref_addr(instr.a, instr.flags & OPFLAG_REF_A),
				deref_addr(instr.b, instr.flags & OPFLAG_REF_B),
				deref_value(instr.c, instr.flags & OPFLAG_REF_C),
				deref_value(instr.d, instr.flags & OPFLAG_REF_D));
		break;
	case OP_CONCAT:
		concat(	deref_addr(instr.a, instr.flags & OPFLAG_REF_A),
				deref_addr(instr.b, instr.flags & OPFLAG_REF_B),
				deref_addr(instr.c, instr.flags & OPFLAG_REF_C));
		break;
	case OP_MEMCPY:
		memcpy(	deref_addr(instr.a, instr.flags & OPFLAG_REF_A),
				deref_addr(instr.b, instr.flags & OPFLAG_REF_B),
				deref_value(instr.c, instr.flags & OPFLAG_REF_C),
				deref_value(instr.d, instr.flags & OPFLAG_REF_D));
		break;
	case OP_SHA256:
		sha256(	deref_addr(instr.a, instr.flags & OPFLAG_REF_A),
				deref_addr(instr.b, instr.flags & OPFLAG_REF_B));
		break;
	case OP_LOG:
		log(	deref_value(instr.a, instr.flags & OPFLAG_REF_A),
				deref_addr(instr.b, instr.flags & OPFLAG_REF_B));
		break;
	case OP_SEND:
		send(	deref_addr(instr.a, instr.flags & OPFLAG_REF_A),
				deref_addr(instr.b, instr.flags & OPFLAG_REF_B),
				deref_addr(instr.c, instr.flags & OPFLAG_REF_C));
		break;
	case OP_MINT:
		mint(	deref_addr(instr.a, instr.flags & OPFLAG_REF_A),
				deref_addr(instr.b, instr.flags & OPFLAG_REF_B));
		break;
	case OP_EVENT:
		event(	deref_addr(instr.a, instr.flags & OPFLAG_REF_A),
				deref_addr(instr.b, instr.flags & OPFLAG_REF_B));
		break;
	case OP_FAIL:
		throw std::runtime_error("failed with: " + to_string(read(
				deref_addr(instr.a, instr.flags & OPFLAG_REF_A))));
	default:
		throw std::logic_error("invalid op_code: 0x" + vnx::to_hex_string(uint8_t(instr.code)));
	}
	get_frame().instr_ptr++;
}

void Engine::clear_extern(const uint32_t offset)
{
	for(auto iter = memory.lower_bound(MEM_EXTERN + offset); iter != memory.lower_bound(MEM_STACK);) {
		clear(iter->second);
		delete iter->second;
		iter = memory.erase(iter);
	}
}

void Engine::clear_stack(const uint32_t offset)
{
	for(auto iter = memory.lower_bound(MEM_STACK + offset); iter != memory.lower_bound(MEM_STATIC);) {
		clear(iter->second);
		delete iter->second;
		iter = memory.erase(iter);
	}
}

void Engine::reset()
{
	clear_stack();
	call_stack.clear();
}

void Engine::commit()
{
	for(auto iter = memory.lower_bound(MEM_STATIC); iter != memory.end(); ++iter)
	{
		if(auto var = iter->second) {
			if(var->flags & FLAG_DIRTY) {
				storage->write(contract, iter->first, *var);
				var->flags &= ~FLAG_DIRTY;
			}
		}
	}
	for(auto iter = entries.lower_bound(std::make_pair(MEM_STATIC, 0)); iter != entries.end(); ++iter)
	{
		if(auto var = iter->second) {
			if(var->flags & FLAG_DIRTY) {
				storage->write(contract, iter->first.first, iter->first.second, *var);
				var->flags &= ~FLAG_DIRTY;
			}
		}
	}
	total_cost += storage->num_write * STOR_WRITE_COST + storage->num_bytes_write * STOR_WRITE_BYTE_COST;
}

void Engine::dump_memory(const uint64_t begin, const uint64_t end)
{
	for(auto iter = memory.lower_bound(begin); iter != memory.lower_bound(end); ++iter) {
		std::cout << "[0x" << std::hex << iter->first << std::dec << "] " << to_string(iter->second);
		if(auto var = iter->second) {
			std::cout << "\t\t(vf: 0x" << std::hex << int(var->flags) << std::dec << ") (rc: " << var->ref_count << ")";
		}
		std::cout << std::endl;
	}
	for(auto iter = entries.lower_bound(std::make_pair(begin, 0)); iter != entries.lower_bound(std::make_pair(end, 0)); ++iter) {
		std::cout << "[0x" << std::hex << iter->first.first << std::dec << "]"
				<< "[0x" << std::hex << iter->first.second << std::dec << "] " << to_string(iter->second);
		if(auto var = iter->second) {
			std::cout << "\t\t(vf: 0x" << std::hex << int(var->flags) << std::dec << ")";
		}
		std::cout << std::endl;
	}
}


} // vm
} // mmx
