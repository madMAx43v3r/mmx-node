/*
 * Engine.cpp
 *
 *  Created on: Apr 22, 2022
 *      Author: mad
 */

#include <mmx/vm/Engine.h>
#include <mmx/vm/StorageProxy.h>
#include <mmx/vm_interface.h>

#include <iostream>


namespace mmx {
namespace vm {

std::string to_hex(const uint64_t addr) {
	std::stringstream ss;
	ss << "0x" << std::uppercase << std::hex << addr;
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

var_t* Engine::assign(const uint64_t dst, std::unique_ptr<var_t> value)
{
	if(!value) {
		return nullptr;
	}
	if(have_init && dst < MEM_EXTERN) {
		throw std::logic_error("already initialized");
	}
	switch(value->type) {
		case TYPE_ARRAY:
			((array_t*)value.get())->address = dst;
			break;
		case TYPE_MAP:
			((map_t*)value.get())->address = dst;
			break;
		default:
			break;
	}
	auto& var = memory[dst];
	if(!var && dst >= MEM_STATIC) {
		var = storage->read(contract, dst);
		total_cost += STOR_READ_COST + num_bytes(var.get()) * STOR_READ_BYTE_COST;
	}
	return assign(var, std::move(value));
}

var_t* Engine::assign_entry(const uint64_t dst, const uint64_t key, std::unique_ptr<var_t> value)
{
	if(!value) {
		return nullptr;
	}
	if(have_init && dst < MEM_EXTERN) {
		throw std::logic_error("already initialized");
	}
	switch(value->type) {
		case TYPE_ARRAY:
		case TYPE_MAP:
			throw std::logic_error("cannot assign array or map as entry");
		default:
			break;
	}
	auto& var = entries[std::make_pair(dst, key)];
	if(!var && dst >= MEM_STATIC) {
		var = storage->read(contract, dst, key);
		total_cost += STOR_READ_COST + num_bytes(var.get()) * STOR_READ_BYTE_COST;
	}
	return assign(var, std::move(value));
}

var_t* Engine::assign_key(const uint64_t dst, const uint64_t key, std::unique_ptr<var_t> value)
{
	return assign_entry(dst, lookup(key, false), std::move(value));
}

var_t* Engine::assign(std::unique_ptr<var_t>& var, std::unique_ptr<var_t> value)
{
	if(var) {
		if(var->flags & FLAG_CONST) {
			throw std::logic_error("read-only memory");
		}
		value->flags = var->flags;
		value->ref_count = var->ref_count;
		clear(var.get());
	}
	var = std::move(value);
	var->flags |= FLAG_DIRTY;
	var->flags &= ~FLAG_DELETED;
	total_cost += WRITE_COST + num_bytes(var.get()) * WRITE_BYTE_COST;
	return var.get();
}

uint64_t Engine::lookup(const uint64_t src, const bool read_only)
{
	return lookup(read_fail(src), read_only);
}

uint64_t Engine::lookup(const var_t* var, const bool read_only)
{
	return var ? lookup(*var, read_only) : 0;
}

uint64_t Engine::lookup(const varptr_t& var, const bool read_only)
{
	return lookup(var.get(), read_only);
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
	total_cost += STOR_READ_COST + num_bytes(var) * STOR_READ_BYTE_COST;

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
		total_cost += STOR_READ_COST + num_bytes(var.get()) * STOR_READ_BYTE_COST;
	}
	return write(var, &dst, src);
}

var_t* Engine::write(const uint64_t dst, const varptr_t& var)
{
	return write(dst, var.get());
}

var_t* Engine::write(const uint64_t dst, const std::vector<varptr_t>& var)
{
	if(var.size() >= MEM_HEAP) {
		throw std::logic_error("array too large");
	}
	auto array = assign(dst, std::make_unique<array_t>());
	for(const auto& entry : var) {
		push_back(dst, entry);
		check_gas();
	}
	return array;
}

var_t* Engine::write(const uint64_t dst, const std::map<varptr_t, varptr_t>& var)
{
	auto map = assign(dst, std::make_unique<map_t>());
	for(const auto& entry : var) {
		write_key(dst, entry.first, entry.second);
		check_gas();
	}
	return map;
}

var_t* Engine::write(std::unique_ptr<var_t>& var, const uint64_t* dst, const var_t& src)
{
	if(var.get() == &src) {
		return var.get();
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
						return var.get();
					default:
						break;
				}
				break;
			case TYPE_REF:
				if(var->type == TYPE_REF) {
					auto ref = (ref_t*)var.get();
					unref(ref->address);
					ref->address = ((const ref_t&)src).address;
					addref(ref->address);
					return ref;
				}
				break;
			case TYPE_UINT:
				if(var->type == TYPE_UINT) {
					auto uint = (uint_t*)var.get();
					uint->value = ((const uint_t&)src).value;
					return uint;
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
			assign(var, std::make_unique<var_t>(src.type));
			break;
		case TYPE_REF: {
			const auto address = ((const ref_t&)src).address;
			addref(address);
			assign(var, std::make_unique<ref_t>(address));
			break;
		}
		case TYPE_UINT:
			assign(var, std::make_unique<uint_t>(((const uint_t&)src).value));
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
			throw invalid_type(src);
	}
	return var.get();
}

void Engine::push_back(const uint64_t dst, const var_t& var)
{
	if(var.type == TYPE_ARRAY) {
		const auto& array = (const array_t&)var;
		if(dst == array.address) {
			throw std::logic_error("dst == src");
		}
		for(uint64_t i = 0; i < array.size; ++i) {
			push_back(dst, read_entry_fail(array.address, i));
			check_gas();
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
	if(auto value = var.get()) {
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

std::unique_ptr<array_t> Engine::clone_array(const uint64_t dst, const array_t& src)
{
	if(dst == src.address) {
		throw std::logic_error("dst == src");
	}
	for(uint64_t i = 0; i < src.size; ++i) {
		write_entry(dst, i, read_entry_fail(src.address, i));
		check_gas();
	}
	auto var = std::make_unique<array_t>(src.size);
	var->address = dst;
	return var;
}

std::unique_ptr<map_t> Engine::clone_map(const uint64_t dst, const map_t& src)
{
	if(dst == src.address) {
		throw std::logic_error("dst == src");
	}
	if(src.address >= MEM_STATIC) {
		throw std::logic_error("cannot clone map from storage");
	}
	const auto begin = entries.lower_bound(std::make_pair(src.address, 0));
	for(auto iter = begin; iter != entries.end() && iter->first.first == src.address; ++iter) {
		if(auto value = iter->second.get()) {
			write_entry(dst, iter->first.second, *value);
			check_gas();
		}
	}
	auto var = std::make_unique<map_t>();
	var->address = dst;
	return var;
}

var_t* Engine::write_entry(const uint64_t dst, const uint64_t key, const var_t& src)
{
	if(have_init && dst < MEM_EXTERN) {
		throw std::logic_error("already initialized");
	}
	auto& var = entries[std::make_pair(dst, key)];
	if(!var && dst >= MEM_STATIC) {
		var = storage->read(contract, dst, key);
		total_cost += STOR_READ_COST + num_bytes(var.get()) * STOR_READ_BYTE_COST;
	}
	return write(var, nullptr, src);
}

var_t* Engine::write_entry(const uint64_t dst, const uint64_t key, const varptr_t& var)
{
	if(auto value = var.get()) {
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
	const auto mapkey = std::make_pair(dst, key);
	auto iter = entries.find(mapkey);
	if(iter == entries.end() && dst >= MEM_STATIC) {
		if(auto var = storage->read(contract, dst, key)) {
			total_cost += num_bytes(var.get()) * STOR_READ_BYTE_COST;
			iter = entries.emplace(mapkey, std::move(var)).first;
		}
		total_cost += STOR_READ_COST;
	}
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
		check_gas();
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
			total_cost += num_bytes(var.get()) * STOR_READ_BYTE_COST;
			erase(memory[dst] = std::move(var));
		}
		total_cost += STOR_READ_COST;
	}
}

void Engine::erase(std::unique_ptr<var_t>& var)
{
	if(!var) {
		return;
	}
	if(var->flags & FLAG_CONST) {
		throw std::logic_error("erase on read-only memory");
	}
	if(var->ref_count) {
		throw std::runtime_error("erase with ref_count " + std::to_string(var->ref_count));
	}
	clear(var.get());

	const auto prev = std::move(var);
	var = std::make_unique<var_t>();
	if(prev->flags & FLAG_STORED) {
		var->flags = (prev->flags | FLAG_DIRTY | FLAG_DELETED);
	} else {
		var->flags = FLAG_DELETED;
	}
	total_cost += WRITE_COST;
}

void Engine::clear(var_t* var)
{
	switch(var->type) {
		case TYPE_REF:
			unref(((const ref_t*)var)->address);
			break;
		case TYPE_ARRAY: {
			if(var->flags & FLAG_STORED) {
				throw std::logic_error("cannot erase array in storage");
			}
			erase_entries(((const array_t*)var)->address);
			break;
		}
		case TYPE_MAP: {
			if(var->flags & FLAG_STORED) {
				throw std::logic_error("cannot erase map in storage");
			}
			erase_entries(((const map_t*)var)->address);
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
		auto var = iter->second.get();
		if(var) {
			if(var->flags & FLAG_DELETED) {
				return nullptr;
			}
		}
		return var;
	}
	if(src >= MEM_STATIC) {
		total_cost += STOR_READ_COST;
		if(auto var = storage->read(contract, src)) {
			total_cost += num_bytes(var.get()) * STOR_READ_BYTE_COST;
			return (memory[src] = std::move(var)).get();
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
		auto var = iter->second.get();
		if(var) {
			if(var->flags & FLAG_DELETED) {
				return nullptr;
			}
		}
		return var;
	}
	if(src >= MEM_STATIC) {
		total_cost += STOR_READ_COST;
		if(auto var = storage->read(contract, src, key)) {
			total_cost += num_bytes(var.get()) * STOR_READ_BYTE_COST;
			return (entries[mapkey] = std::move(var)).get();
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
	if(offset.value == 0) {
		throw std::runtime_error("out of memory");
	}
	return offset.value++;
}

void Engine::init()
{
	if(have_init) {
		throw std::logic_error("already initialized");
	}
	// Note: address 0 is not a valid key (used to denote "key not found")
	for(auto iter = memory.lower_bound(1); iter != memory.lower_bound(MEM_EXTERN); ++iter) {
		key_map.emplace(iter->second.get(), iter->first);
	}
	if(!read(MEM_HEAP + GLOBAL_HAVE_INIT)) {
		assign(MEM_HEAP + GLOBAL_HAVE_INIT, std::make_unique<var_t>(TYPE_TRUE))->pin();
		assign(MEM_HEAP + GLOBAL_NEXT_ALLOC, std::make_unique<uint_t>(MEM_HEAP + GLOBAL_DYNAMIC_START))->pin();
		assign(MEM_HEAP + GLOBAL_LOG_HISTORY, std::make_unique<array_t>())->pin();
		assign(MEM_HEAP + GLOBAL_SEND_HISTORY, std::make_unique<array_t>())->pin();
		assign(MEM_HEAP + GLOBAL_MINT_HISTORY, std::make_unique<array_t>())->pin();
		assign(MEM_HEAP + GLOBAL_EVENT_HISTORY, std::make_unique<array_t>())->pin();
	}
	have_init = true;
}

void Engine::begin(const uint64_t instr_ptr)
{
	call_stack.clear();

	for(auto iter = memory.begin(); iter != memory.lower_bound(MEM_STACK); ++iter) {
		if(auto var = iter->second.get()) {
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
		check_gas();
	} catch(const std::exception& ex) {
		throw std::runtime_error("exception at " + to_hex(instr_ptr) + ": " + ex.what());
	}
}

void Engine::check_gas() const
{
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
			const auto& index = read_fail<uint_t>(key, TYPE_UINT);
			if(index.value < ((const array_t&)var).size) {
				write(dst, read_entry_fail(addr, uint64_t(index.value)));
			} else if(flags & OPFLAG_HARD_FAIL) {
				throw std::runtime_error("index out of bounds");
			} else {
				write(dst, var_t());
			}
			break;
		}
		case TYPE_MAP:
			write(dst, read_key(addr, key));
			break;
		default:
			if(flags & OPFLAG_HARD_FAIL) {
				throw invalid_type(var);
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
			const auto& index = read_fail<uint_t>(key, TYPE_UINT);
			if(index.value >= ((const array_t&)var).size) {
				throw std::runtime_error("index out of bounds");
			}
			write_entry(addr, uint64_t(index.value), read_fail(src));
			break;
		}
		case TYPE_MAP:
			write_key(addr, key, read_fail(src));
			break;
		default:
			if(flags & OPFLAG_HARD_FAIL) {
				throw invalid_type(var);
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
			throw invalid_type(var);
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
			res->size = size;
			assign(dst, std::move(res));
			break;
		}
		default:
			throw invalid_type(lvar);
	}
}

void Engine::memcpy(const uint64_t dst, const uint64_t src, const uint64_t count, const uint64_t offset)
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
			assign(dst, std::move(res));
			break;
		}
		default:
			throw invalid_type(svar);
	}
}

void Engine::sha256(const uint64_t dst, const uint64_t src)
{
	const auto& svar = read_fail(src);
	switch(svar.type) {
		case TYPE_STRING:
		case TYPE_BINARY: {
			const auto& sbin = (const binary_t&)svar;
			total_cost += std::max<uint32_t>(sbin.size + (64 - sbin.size % 64) % 64, 64) * SHA256_BYTE_COST;
			check_gas();
			write(dst, uint_t(hash_t(sbin.data(), sbin.size).to_uint256()));
			break;
		}
		default:
			throw invalid_type(svar);
	}
}

void Engine::conv(const uint64_t dst, const uint64_t src, const uint64_t dflags, const uint64_t sflags)
{
	const auto& svar = read_fail(src);
	switch(svar.type) {
		case TYPE_NIL:
			switch(dflags & 0xFF) {
				case CONVTYPE_BOOL: write(dst, var_t(TYPE_FALSE)); break;
				case CONVTYPE_UINT: write(dst, uint_t()); break;
				default: throw std::logic_error("invalid conversion: NIL to " + to_hex(dflags));
			}
			break;
		case TYPE_TRUE:
			switch(dflags & 0xFF) {
				case CONVTYPE_BOOL: write(dst, var_t(TYPE_TRUE)); break;
				case CONVTYPE_UINT: write(dst, uint_t(1)); break;
				default: throw std::logic_error("invalid conversion: TRUE to " + to_hex(dflags));
			}
			break;
		case TYPE_FALSE:
			switch(dflags & 0xFF) {
				case CONVTYPE_BOOL: write(dst, var_t(TYPE_FALSE)); break;
				case CONVTYPE_UINT: write(dst, uint_t()); break;
				default: throw std::logic_error("invalid conversion: FALSE to " + to_hex(dflags));
			}
			break;
		case TYPE_UINT: {
			const auto& sint = (const uint_t&)svar;
			switch(dflags & 0xFF) {
				case CONVTYPE_BOOL:
					write(dst, var_t(sint.value != uint256_0));
					break;
				case CONVTYPE_UINT:
					write(dst, svar);
					break;
				case CONVTYPE_STRING: {
					int base = 10;
					const auto bflags = (dflags >> 8) & 0xFF;
					switch(bflags) {
						case CONVTYPE_DEFAULT: break;
						case CONVTYPE_BASE_2: base = 2; break;
						case CONVTYPE_BASE_8: base = 8; break;
						case CONVTYPE_BASE_10: base = 10; break;
						case CONVTYPE_BASE_16: base = 16; break;
						default: throw std::logic_error("invalid conversion: UINT to STRING with base " + to_hex(bflags));
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
					throw std::logic_error("invalid conversion: UINT to " + to_hex(dflags));
			}
			break;
		}
		case TYPE_STRING: {
			const auto& sstr = (const binary_t&)svar;
			switch(dflags & 0xFF) {
				case CONVTYPE_BOOL:
					write(dst, var_t(bool(sstr.size)));
					break;
				case CONVTYPE_UINT: {
					int base = 10;
					switch(sflags & 0xFF) {
						case CONVTYPE_DEFAULT: break;
						case CONVTYPE_BASE_2: base = 2; break;
						case CONVTYPE_BASE_8: base = 8; break;
						case CONVTYPE_BASE_10: base = 10; break;
						case CONVTYPE_BASE_16: base = 16; break;
						case CONVTYPE_ADDRESS: base = 32; break;
						default: throw std::logic_error("invalid conversion: STRING to UINT with base " + to_hex(sflags));
					}
					if(base == 32) {
						write(dst, uint_t(addr_t(sstr.to_string()).to_uint256()));
					} else {
						write(dst, uint_t(uint256_t(sstr.c_str(), base)));
					}
					break;
				}
				case CONVTYPE_STRING:
					write(dst, svar);
					break;
				case CONVTYPE_BINARY:
					assign(dst, binary_t::alloc(sstr, TYPE_BINARY));
					break;
				default:
					throw std::logic_error("invalid conversion: STRING to " + to_hex(dflags));
			}
			break;
		}
		case TYPE_BINARY: {
			const auto& sbin = (const binary_t&)svar;
			switch(dflags & 0xFF) {
				case CONVTYPE_BOOL:
					write(dst, var_t(bool(sbin.size)));
					break;
				case CONVTYPE_UINT: {
					if(sbin.size > 32) {
						throw std::runtime_error("invalid conversion: BINARY to UINT: source size > 32");
					}
					uint_t var;
					::memcpy(&var.value, sbin.data(), sbin.size);
					write(dst, var);
					break;
				}
				case CONVTYPE_STRING:
					assign(dst, binary_t::alloc(vnx::to_hex_string(sbin.data(), sbin.size, false)));
					break;
				case CONVTYPE_BINARY:
					write(dst, svar);
					break;
				default:
					throw std::logic_error("invalid conversion: BINARY to " + to_hex(dflags));
			}
			break;
		}
		default:
			switch(dflags & 0xFF) {
				case CONVTYPE_BOOL: write(dst, var_t(TYPE_TRUE)); break;
				default: throw std::logic_error("invalid conversion: " + to_hex(svar.type) + " to " + to_hex(dflags));
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
	if(value == 0) {
		return;
	}
	if(value >> 64) {
		throw std::runtime_error("amount too large");
	}
	auto balance = read_key<uint_t>(MEM_EXTERN + EXTERN_BALANCE, currency, TYPE_UINT);
	if(!balance || balance->value < value) {
		throw std::runtime_error("insufficient funds");
	}
	balance->value -= value;

	txout_t out;
	out.contract = addr_t(read_fail<uint_t>(currency, TYPE_UINT).value);
	out.address = addr_t(read_fail<uint_t>(address, TYPE_UINT).value);
	out.amount = value;
	outputs.push_back(out);
	total_cost += SEND_COST;
}

void Engine::mint(const uint64_t address, const uint64_t amount)
{
	const auto value = read_fail<uint_t>(amount, TYPE_UINT).value;
	if(value == 0) {
		return;
	}
	if(value >> 64) {
		throw std::runtime_error("amount too large");
	}
	txout_t out;
	out.contract = contract;
	out.address = addr_t(read_fail<uint_t>(address, TYPE_UINT).value);
	out.amount = value;
	mint_outputs.push_back(out);
	total_cost += MINT_COST;
}

void Engine::rcall(const uint64_t name, const uint64_t method, const uint64_t stack_ptr, const uint64_t nargs)
{
	if(stack_ptr >= STACK_SIZE) {
		throw std::logic_error("stack overflow");
	}
	if(nargs > 4096) {
		throw std::logic_error("nargs > 4096");
	}
	if(!remote) {
		throw std::logic_error("unable to make remote calls");
	}
	auto frame = get_frame();
	frame.stack_ptr += stack_ptr;
	call_stack.push_back(frame);

	remote(	read_fail<binary_t>(name, TYPE_STRING).to_string(),
			read_fail<binary_t>(method, TYPE_STRING).to_string(), nargs);
	ret();
}

void Engine::jump(const uint64_t instr_ptr)
{
	get_frame().instr_ptr = instr_ptr;
}

void Engine::call(const uint64_t instr_ptr, const uint64_t stack_ptr)
{
	if(stack_ptr >= STACK_SIZE) {
		throw std::logic_error("stack overflow");
	}
	auto frame = get_frame();
	frame.instr_ptr = instr_ptr;
	frame.stack_ptr += stack_ptr;
	call_stack.push_back(frame);
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
		if(uint64_t(src) + frame.stack_ptr >= MEM_STATIC) {
			throw std::runtime_error("stack overflow");
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
	total_cost += INSTR_COST;

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
			default:
				throw invalid_type(var);
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
			default:
				throw invalid_type(L);
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
			default:
				throw invalid_type(L);
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
			default:
				throw invalid_type(L);
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
		const auto& value = read_fail<uint_t>(src, TYPE_UINT).value;
		write(dst, uint_t(value << count));
		break;
	}
	case OP_SHR: {
		const auto dst = deref_addr(instr.a, instr.flags & OPFLAG_REF_A);
		const auto src = deref_addr(instr.b, instr.flags & OPFLAG_REF_B);
		const auto count = deref_value(instr.c, instr.flags & OPFLAG_REF_C);
		const auto& value = read_fail<uint_t>(src, TYPE_UINT).value;
		write(dst, uint_t(value >> count));
		break;
	}
	case OP_SAR: {
		const auto dst = deref_addr(instr.a, instr.flags & OPFLAG_REF_A);
		const auto src = deref_addr(instr.b, instr.flags & OPFLAG_REF_B);
		const auto count = deref_value(instr.c, instr.flags & OPFLAG_REF_C);
		const auto& value = read_fail<uint_t>(src, TYPE_UINT).value;
		auto tmp = value >> count;
		if(value >> 255) {
			tmp |= uint256_max << (256 - count);
		}
		write(dst, uint_t(tmp));
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
		write(dst, var_t(res));
		break;
	}
	case OP_CMP_SLT:
	case OP_CMP_SGT:
	case OP_CMP_SLTE:
	case OP_CMP_SGTE: {
		const auto dst = deref_addr(instr.a, instr.flags & OPFLAG_REF_A);
		const auto lhs = deref_addr(instr.b, instr.flags & OPFLAG_REF_B);
		const auto rhs = deref_addr(instr.c, instr.flags & OPFLAG_REF_C);
		const auto& L = read_fail<uint_t>(lhs, TYPE_UINT).value;
		const auto& R = read_fail<uint_t>(rhs, TYPE_UINT).value;
		const bool lneg = L >> 255;
		const bool rneg = R >> 255;
		bool res = false;
		switch(instr.code) {
			case OP_CMP_SLT: res = (lneg == rneg ? L < R : lneg); break;
			case OP_CMP_SGT: res = (lneg == rneg ? L > R : rneg); break;
			case OP_CMP_SLTE: res = (lneg == rneg ? L <= R : lneg); break;
			case OP_CMP_SGTE: res = (lneg == rneg ? L >= R : rneg); break;
			default: break;
		}
		write(dst, var_t(res));
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
		throw std::runtime_error("failed with: " + to_string_value(read(
				deref_addr(instr.a, instr.flags & OPFLAG_REF_A))));
	case OP_RCALL:
		rcall(	deref_addr(instr.a, instr.flags & OPFLAG_REF_A),
				deref_addr(instr.b, instr.flags & OPFLAG_REF_B),
				deref_value(instr.c, instr.flags & OPFLAG_REF_C),
				deref_value(instr.d, instr.flags & OPFLAG_REF_D));
		break;
	default:
		throw std::logic_error("invalid op_code: 0x" + vnx::to_hex_string(uint32_t(instr.code)));
	}
	get_frame().instr_ptr++;
}

void Engine::clear_stack(const uint64_t offset)
{
	for(auto iter = memory.lower_bound(MEM_STACK + offset); iter != memory.lower_bound(MEM_STATIC);) {
		clear(iter->second.get());
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
		if(auto var = iter->second.get()) {
			if(var->flags & FLAG_DIRTY) {
				storage->write(contract, iter->first, *var);
				total_cost += (STOR_WRITE_COST + num_bytes(var) * STOR_WRITE_BYTE_COST) * (var->flags & FLAG_KEY ? 2 : 1);
				var->flags &= ~FLAG_DIRTY;
			}
		}
	}
	for(auto iter = entries.lower_bound(std::make_pair(MEM_STATIC, 0)); iter != entries.end(); ++iter)
	{
		if(auto var = iter->second.get()) {
			if(var->flags & FLAG_DIRTY) {
				storage->write(contract, iter->first.first, iter->first.second, *var);
				total_cost += STOR_WRITE_COST + num_bytes(var) * STOR_WRITE_BYTE_COST;
				var->flags &= ~FLAG_DIRTY;
			}
		}
	}
	check_gas();	// check at the end is fine since writing to cache only at this point
}

std::map<uint64_t, const var_t*> Engine::find_entries(const uint64_t dst) const
{
	std::map<uint64_t, const var_t*> out;
	const auto begin = entries.lower_bound(std::make_pair(dst, 0));
	for(auto iter = begin; iter != entries.end() && iter->first.first == dst; ++iter) {
		out[iter->first.second] = iter->second.get();
	}
	return out;
}

void Engine::dump_memory(const uint64_t begin, const uint64_t end)
{
	for(auto iter = memory.lower_bound(begin); iter != memory.lower_bound(end); ++iter) {
		std::cout << "[" << to_hex(iter->first) << "] " << to_string(iter->second.get());
		if(auto var = iter->second.get()) {
			std::cout << "\t\t(vf: " << to_hex(int(var->flags)) << ") (rc: " << var->ref_count << ")";
		}
		std::cout << std::endl;
	}
	for(auto iter = entries.lower_bound(std::make_pair(begin, 0)); iter != entries.lower_bound(std::make_pair(end, 0)); ++iter) {
		std::cout << "[" << to_hex(iter->first.first) << "]"
				<< "[" << to_hex(iter->first.second) << "] " << to_string(iter->second.get());
		if(auto var = iter->second.get()) {
			std::cout << "\t\t(vf: " << to_hex(int(var->flags)) << ")";
		}
		std::cout << std::endl;
	}
}


} // vm
} // mmx
