/*
 * Engine.cpp
 *
 *  Created on: Apr 22, 2022
 *      Author: mad
 */

#include <mmx/vm/Engine.h>
#include <mmx/vm/StorageProxy.h>


namespace mmx {
namespace vm {

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
	if(read_fail(dst).unref()) {
		erase(dst);
	}
}

var_t* Engine::assign(const uint64_t dst, var_t* value)
{
	if(!value) {
		return nullptr;
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
	switch(value->type) {
		case TYPE_ARRAY:
		case TYPE_MAP:
			delete value;
			throw std::logic_error("cannot assign array / map here");
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
		if(var->flags & varflags_e::CONST) {
			delete value;
			throw std::logic_error("read-only memory");
		}
		value->flags = (var->flags & ~varflags_e::DELETED) | varflags_e::DIRTY;
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
	bytes_write += num_bytes(value);
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
	return 0;
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
		if(key < MEM_STATIC) {
			throw std::logic_error("lookup(): key < MEM_STATIC");
		}
		auto& value = read_fail(key);
		if(value.ref_count == 0) {
			throw std::logic_error("lookup(): key with ref_count == 0");
		}
		if(!(value.flags & varflags_e::CONST) || !(value.flags & varflags_e::KEY)) {
			throw std::logic_error("lookup(): key missing flag CONST / KEY");
		}
		key_map[&value] = key;
		return key;
	}
	const auto key = alloc();
	const auto value = write(key, var);
	value->flags |= varflags_e::CONST | varflags_e::KEY;
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
	if(var) {
		if(var->flags & varflags_e::CONST) {
			if(dst) {
				throw std::logic_error("read-only memory at " + to_hex(*dst));
			} else {
				throw std::logic_error("read-only memory");
			}
		}
		var->flags |= varflags_e::DIRTY;
		var->flags &= ~varflags_e::DELETED;

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
			return assign(var, new var_t(src.type));
		case TYPE_REF: {
			const auto address = ((const ref_t&)src).address;
			addref(address);
			return assign(var, new ref_t(address));
		}
		case TYPE_UINT:
			return assign(var, new uint_t((const uint_t&)src));
		case TYPE_STRING:
		case TYPE_BINARY:
			return assign(var, binary_t::alloc((const binary_t&)src));
		case TYPE_ARRAY:
			if(!dst) {
				throw std::logic_error("cannot assign array here");
			}
			return assign(var, clone_array(*dst, (const array_t&)src));
		case TYPE_MAP:
			if(!dst) {
				throw std::logic_error("cannot assign map here");
			}
			return assign(var, clone_map(*dst, (const map_t&)src));
		default:
			throw std::logic_error("invalid type");
	}
	return var;
}

void Engine::push_back(const uint64_t dst, const var_t& src)
{
	auto& array = read_fail<array_t>(dst, TYPE_ARRAY);
	if(array.size == std::min<uint32_t>(MEM_HEAP - 1, std::numeric_limits<uint32_t>::max())) {
		throw std::runtime_error("push_back overflow at " + to_hex(dst));
	}
	write_entry(dst, array.size++, src);
	array.flags |= varflags_e::DIRTY;
}

void Engine::push_back(const uint64_t dst, const uint64_t src)
{
	push_back(dst, read_fail(src));
}

void Engine::pop_back(const uint64_t dst, const uint64_t& src)
{
	auto& array = read_fail<array_t>(src, TYPE_ARRAY);
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
	for(uint64_t i = 0; i < src.size; ++i) {
		write_entry(dst, i, read_entry_fail(src.address, i));
	}
	auto var = new array_t();
	var->address = dst;
	var->size = src.size;
	return var;
}

map_t* Engine::clone_map(const uint64_t dst, const map_t& src)
{
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
	auto& var = entries[std::make_pair(dst, key)];
	if(!var && dst >= MEM_STATIC) {
		var = storage->read(contract, dst, key);
	}
	return write(var, nullptr, src);
}

var_t* Engine::write_entry(const uint64_t dst, const uint64_t key, const varptr_t& var)
{
	if(auto value = var.ptr) {
		switch(value->type) {
			case TYPE_ARRAY:
			case TYPE_MAP: {
				const auto heap = alloc();
				write(heap, value);
				return write_entry(dst, key, ref_t(heap));
			}
			default:
				break;
		}
		return write_entry(dst, key, *value);
	}
	return write_entry(dst, key, var_t());
}

var_t* Engine::write_key(const uint64_t dst, const uint64_t key, const var_t& src)
{
	return write_entry(dst, lookup(key), src);
}

var_t* Engine::write_key(const uint64_t dst, const var_t& key, const var_t& src)
{
	return write_entry(dst, lookup(key), src);
}

var_t* Engine::write_key(const uint64_t dst, const varptr_t& key, const varptr_t& var)
{
	return write_entry(dst, lookup(key), var);
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
	erase_entry(dst, lookup(key));
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
	if(var->flags & varflags_e::CONST) {
		throw std::logic_error("erase on read-only memory");
	}
	if(var->ref_count) {
		throw std::runtime_error("erase with ref_count " + std::to_string(var->ref_count));
	}
	clear(var);

	auto prev = var;
	var = new var_t();
	if(var->flags & varflags_e::STORED) {
		var->flags = (prev->flags | varflags_e::DIRTY | varflags_e::DELETED);
	} else {
		var->flags = varflags_e::DELETED;
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
			if(dst >= MEM_STATIC && (var->flags & varflags_e::STORED)) {
				throw std::logic_error("cannot erase array in storage");
			}
			erase_entries(dst);
			break;
		}
		case TYPE_MAP: {
			const auto dst = ((const map_t*)var)->address;
			if(dst >= MEM_STATIC && (var->flags & varflags_e::STORED)) {
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
	return read_entry(src, lookup(key));
}

var_t& Engine::read_key_fail(const uint64_t src, const uint64_t key)
{
	return read_entry_fail(src, lookup(key));
}

uint64_t Engine::alloc()
{
	auto& offset = read_fail<uint_t>(MEM_HEAP + NEXT_ALLOC, TYPE_UINT);
	offset.flags |= varflags_e::DIRTY;
	if(offset.value == std::numeric_limits<uint64_t>::max()) {
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
		write(MEM_HEAP + HAVE_INIT, var_t(TYPE_TRUE))->pin();
		write(MEM_HEAP + NEXT_ALLOC, uint_t(MEM_HEAP + DYNAMIC_START))->pin();
		write(MEM_HEAP + BALANCE, map_t())->pin();
		write(MEM_HEAP + LOG_HISTORY, array_t())->pin();
		write(MEM_HEAP + SEND_HISTORY, array_t())->pin();
		write(MEM_HEAP + MINT_HISTORY, array_t())->pin();
		write(MEM_HEAP + EVENT_HISTORY, array_t())->pin();
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
	cost = num_instr * INSTR_COST + num_write * WRITE_COST + bytes_write * WRITE_BYTE_COST
			+ (storage->num_read + storage->num_lookup) * STOR_READ_COST
			+ (storage->bytes_read + storage->bytes_lookup) * STOR_READ_BYTE_COST;
	if(cost >= credits) {
		throw std::runtime_error("out of credits: " + std::to_string(cost) + " > " + std::to_string(credits));
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

void Engine::get(const uint64_t dst, const uint64_t addr, const uint64_t key, const uint8_t flags)
{
	const auto& var = read_fail(addr);
	switch(var.type) {
		case TYPE_ARRAY: {
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
		case TYPE_MAP:
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

void Engine::set(const uint64_t addr, const uint64_t key, const uint64_t src, const uint8_t flags)
{
	const auto& var = read_fail(addr);
	switch(var.type) {
		case TYPE_ARRAY: {
			const auto index = key;
			if(index < ((const array_t&)var).size) {
				write_entry(addr, index, read_fail(src));
			}
			else if(flags & opflags_e::HARD_FAIL) {
				throw std::runtime_error("out of bounds");
			}
			break;
		}
		case TYPE_MAP:
			write_key(addr, key, read_fail(src));
			break;
		default:
			if(flags & opflags_e::HARD_FAIL) {
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
		case TYPE_STRING:
		case TYPE_BINARY: {
			const auto& L = (const binary_t&)dvar;
			const auto& R = (const binary_t&)svar;
			auto res = binary_t::unsafe_alloc(L.size + R.size, dvar.type);
			::memcpy(res->data(), L.data(), L.size);
			::memcpy(res->data(L.size), R.data(), R.size);
			assign(dst, res);
			break;
		}
		case TYPE_ARRAY: {
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

void Engine::conv(const uint64_t dst, const uint64_t src, const uint32_t dflags, const uint32_t sflags)
{
	const auto& svar = read_fail(src);
	switch(svar.type) {
		case TYPE_UINT: {
			const auto& sint = (const uint_t&)svar;
			switch(dflags & 0xFF) {
				case CONVTYPE_STRING: {
					int base = 10;
					switch((dflags >> 8) & 0xFF) {
						case CONVTYPE_DEFAULT: break;
						case CONVTYPE_BIN: base = 2; break;
						case CONVTYPE_OCT: base = 8; break;
						case CONVTYPE_DEC: base = 10; break;
						case CONVTYPE_HEX: base = 16; break;
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
				case CONVTYPE_UINT: {
					int base = 10;
					switch((sflags >> 8) & 0xFF) {
						case CONVTYPE_DEFAULT: break;
						case CONVTYPE_BIN: base = 2; break;
						case CONVTYPE_OCT: base = 8; break;
						case CONVTYPE_DEC: base = 10; break;
						case CONVTYPE_HEX: base = 16; break;
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
			throw std::logic_error("invalid conversion");
	}
}

void Engine::log(const uint64_t level, const uint64_t msg)
{
	const auto entry = alloc();
	write(entry, array_t());
	push_back(entry, externvar_e::TXID);
	push_back(entry, uint_t(level));
	push_back(entry, msg);
	push_back(globalvar_e::LOG_HISTORY, ref_t(entry));
}

void Engine::event(const uint64_t name, const uint64_t data)
{
	const auto entry = alloc();
	write(entry, array_t());
	push_back(entry, externvar_e::TXID);
	push_back(entry, name);
	push_back(entry, data);
	push_back(globalvar_e::EVENT_HISTORY, ref_t(entry));
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
		if(instr.flags & opflags_e::REF_A) {
			throw std::logic_error("de-referencing instr.a not supported");
		}
		erase(instr.a);
		break;
	case OP_COPY:
		copy(	deref_addr(instr.a, instr.flags & opflags_e::REF_A),
				deref_addr(instr.b, instr.flags & opflags_e::REF_B));
		break;
	case OP_CLONE:
		clone(	deref_addr(instr.a, instr.flags & opflags_e::REF_A),
				deref_addr(instr.b, instr.flags & opflags_e::REF_B));
		break;
	case OP_JUMP: {
		jump(	deref_value(instr.a, instr.flags & opflags_e::REF_A));
		return;
	}
	case OP_JUMPI: {
		const auto dst = deref_value(instr.a, instr.flags & opflags_e::REF_A);
		const auto cond = deref_addr(instr.b, instr.flags & opflags_e::REF_B);
		const auto& var = read_fail(cond);
		if(var.type == TYPE_TRUE) {
			jump(dst);
			return;
		}
		break;
	}
	case OP_CALL:
		if(instr.flags & opflags_e::REF_B) {
			throw std::logic_error("de-referencing instr.b not supported");
		}
		call(	deref_value(instr.a, instr.flags & opflags_e::REF_A),
				instr.b);
		return;
	case OP_RET:
		if(ret()) {
			return;
		}
		break;
	case OP_ADD: {
		const auto dst = deref_addr(instr.a, instr.flags & opflags_e::REF_A);
		const auto lhs = deref_addr(instr.b, instr.flags & opflags_e::REF_B);
		const auto rhs = deref_addr(instr.c, instr.flags & opflags_e::REF_C);
		const auto& L = read_fail<uint_t>(lhs, TYPE_UINT).value;
		const auto& R = read_fail<uint_t>(rhs, TYPE_UINT).value;
		const uint256_t D = L + R;
		if((instr.flags & opflags_e::CATCH_OVERFLOW) && D < L) {
			throw std::runtime_error("integer overflow");
		}
		write(dst, uint_t(D));
		break;
	}
	case OP_SUB: {
		const auto dst = deref_addr(instr.a, instr.flags & opflags_e::REF_A);
		const auto lhs = deref_addr(instr.b, instr.flags & opflags_e::REF_B);
		const auto rhs = deref_addr(instr.c, instr.flags & opflags_e::REF_C);
		const auto& L = read_fail<uint_t>(lhs, TYPE_UINT).value;
		const auto& R = read_fail<uint_t>(rhs, TYPE_UINT).value;
		const uint256_t D = L - R;
		if((instr.flags & opflags_e::CATCH_OVERFLOW) && D > L) {
			throw std::runtime_error("integer overflow");
		}
		write(dst, uint_t(D));
		break;
	}
	case OP_TYPE: {
		const auto dst = deref_addr(instr.a, instr.flags & opflags_e::REF_A);
		const auto addr = deref_addr(instr.b, instr.flags & opflags_e::REF_B);
		const auto& var = read_fail(addr);
		write(dst, uint_t(uint32_t(var.type)));
		break;
	}
	case OP_SIZE: {
		const auto dst = deref_addr(instr.a, instr.flags & opflags_e::REF_A);
		const auto addr = deref_addr(instr.b, instr.flags & opflags_e::REF_B);
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
		get(	deref_addr(instr.a, instr.flags & opflags_e::REF_A),
				deref_addr(instr.b, instr.flags & opflags_e::REF_B),
				deref_addr(instr.c, instr.flags & opflags_e::REF_C), instr.flags);
		break;
	case OP_SET:
		set(	deref_addr(instr.a, instr.flags & opflags_e::REF_A),
				deref_addr(instr.b, instr.flags & opflags_e::REF_B),
				deref_addr(instr.c, instr.flags & opflags_e::REF_C), instr.flags);
		break;
	case OP_ERASE:
		erase(	deref_addr(instr.a, instr.flags & opflags_e::REF_A),
				deref_addr(instr.b, instr.flags & opflags_e::REF_B), instr.flags);
		break;
	case OP_PUSH_BACK:
		push_back(	deref_addr(instr.a, instr.flags & opflags_e::REF_A),
					deref_addr(instr.b, instr.flags & opflags_e::REF_B));
		break;
	case OP_POP_BACK:
		pop_back(	deref_addr(instr.a, instr.flags & opflags_e::REF_A),
					deref_addr(instr.b, instr.flags & opflags_e::REF_B));
		break;
	case OP_CONCAT:
		concat(	deref_addr(instr.a, instr.flags & opflags_e::REF_A),
				deref_addr(instr.b, instr.flags & opflags_e::REF_B));
		break;
	case OP_MEMCPY:
		memcpy(	deref_addr(instr.a, instr.flags & opflags_e::REF_A),
				deref_addr(instr.b, instr.flags & opflags_e::REF_B),
				deref_value(instr.c, instr.flags & opflags_e::REF_C),
				deref_value(instr.d, instr.flags & opflags_e::REF_D));
		break;
	case OP_LOG:
		log(	deref_value(instr.a, instr.flags & opflags_e::REF_A),
				deref_addr(instr.b, instr.flags & opflags_e::REF_B));
		break;
	case OP_EVENT:
		event(	deref_addr(instr.a, instr.flags & opflags_e::REF_A),
				deref_addr(instr.b, instr.flags & opflags_e::REF_B));
		break;
	}
	get_frame().instr_ptr++;
}

void Engine::clear_extern(const uint32_t offset)
{
	for(auto iter = memory.lower_bound(MEM_EXTERN + offset); iter != memory.lower_bound(MEM_STACK);)
	{
		erase(iter->second);
		iter = memory.erase(iter);
	}
}

void Engine::clear_stack(const uint32_t offset)
{
	for(auto iter = memory.lower_bound(MEM_STACK + offset); iter != memory.lower_bound(MEM_STATIC);)
	{
		erase(iter->second);
		iter = memory.erase(iter);
	}
}

void Engine::reset()
{
	clear_stack();
}

void Engine::commit()
{
	for(auto iter = memory.lower_bound(MEM_STATIC); iter != memory.end(); ++iter)
	{
		if(auto var = iter->second) {
			if(var->flags & varflags_e::DIRTY) {
				storage->write(contract, iter->first, *var);
			}
		}
	}
	for(auto iter = entries.lower_bound(std::make_pair(MEM_STATIC, 0)); iter != entries.end(); ++iter)
	{
		if(auto var = iter->second) {
			if(var->flags & varflags_e::DIRTY) {
				storage->write(contract, iter->first.first, iter->first.second, *var);
			}
		}
	}
}










} // vm
} // mmx
