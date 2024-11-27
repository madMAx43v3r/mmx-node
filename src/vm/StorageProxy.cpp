/*
 * StorageProxy.cpp
 *
 *  Created on: Apr 24, 2022
 *      Author: mad
 */

#include <mmx/vm/StorageProxy.h>
#include <mmx/vm/Engine.h>


namespace mmx {
namespace vm {

StorageProxy::StorageProxy(Engine* engine, std::shared_ptr<Storage> backend, bool read_only)
	:	engine(engine),
		backend(backend),
		read_only(read_only)
{
}

std::unique_ptr<var_t> StorageProxy::read_ex(std::unique_ptr<var_t> var) const
{
	if(var) {
		if(var->flags & FLAG_DELETED) {
			var.reset();
		}
	}
	if(var) {
		var->flags &= ~FLAG_DIRTY;
		var->flags |= FLAG_STORED;
		if(read_only) {
			var->flags |= FLAG_CONST;
		}
	}
	return std::move(var);
}

std::unique_ptr<var_t> StorageProxy::read(const addr_t& contract, const uint64_t src) const
{
	auto var = read_ex(backend->read(contract, src));

	const auto size = num_bytes(var.get());
	const auto blocks = size / 32;

	if(engine->do_profile) {
		engine->cost_map["STOR_READ_COST"]++;
		engine->cost_map["STOR_READ_32_BYTE_COST"] += blocks;
	}
	engine->gas_used += STOR_READ_COST + blocks * STOR_READ_32_BYTE_COST;
	engine->check_gas();

	if(engine->do_trace) {
		trace_t t;
		t.type = "READ";
		t.contract = contract;
		t.addr = src;
		t.value = clone(var.get());
		trace.push_back(t);
	}
	return var;
}

std::unique_ptr<var_t> StorageProxy::read(const addr_t& contract, const uint64_t src, const uint64_t key) const
{
	auto var = read_ex(backend->read(contract, src, key));

	const auto size = num_bytes(var.get());
	const auto blocks = size / 32;

	if(engine->do_profile) {
		engine->cost_map["STOR_READ_COST"]++;
		engine->cost_map["STOR_READ_32_BYTE_COST"] += blocks;
	}
	engine->gas_used += STOR_READ_COST + blocks * STOR_READ_32_BYTE_COST;
	engine->check_gas();

	if(engine->do_trace) {
		trace_t t;
		t.type = "READ_KEY";
		t.contract = contract;
		t.addr = src;
		t.key = key;
		t.value = clone(var.get());
		trace.push_back(t);
	}
	return var;
}

void StorageProxy::write(const addr_t& contract, const uint64_t dst, const var_t& value)
{
	if(read_only) {
		throw std::logic_error("read-only storage");
	}
	if(engine->do_trace) {
		trace_t t;
		t.type = "WRITE";
		t.contract = contract;
		t.addr = dst;
		t.value = clone(value);
		trace.push_back(t);
	}
	if(value.type == TYPE_REF) {
		const auto address = ((const ref_t&)value).address;
		if(address < MEM_STATIC) {
			throw std::logic_error("cannot store reference to non-static memory");
		}
	}
	if((value.flags & FLAG_KEY) && !(value.flags & FLAG_CONST)) {
		throw std::logic_error("keys need to be const");
	}
	const int cost_factor = (value.flags & FLAG_KEY ? 2 : 1);

	if(engine->do_profile) {
		engine->cost_map["STOR_WRITE_COST"] += cost_factor;
		engine->cost_map["STOR_WRITE_BYTE_COST"] += num_bytes(value) * cost_factor;
	}
	engine->gas_used += (STOR_WRITE_COST + num_bytes(value) * STOR_WRITE_BYTE_COST) * cost_factor;
	engine->check_gas();

	backend->write(contract, dst, value);
}

void StorageProxy::write(const addr_t& contract, const uint64_t dst, const uint64_t key, const var_t& value)
{
	if(read_only) {
		throw std::logic_error("read-only storage");
	}
	if(engine->do_trace) {
		trace_t t;
		t.type = "WRITE_KEY";
		t.contract = contract;
		t.addr = dst;
		t.key = key;
		t.value = clone(value);
		trace.push_back(t);
	}
	if(value.type == TYPE_REF) {
		const auto address = ((const ref_t&)value).address;
		if(address < MEM_STATIC) {
			throw std::logic_error("cannot store reference to non-static memory");
		}
	}
	if(value.ref_count) {
		throw std::logic_error("entries cannot have ref_count > 0");
	}
	if(engine->do_profile) {
		engine->cost_map["STOR_WRITE_COST"]++;
		engine->cost_map["STOR_WRITE_BYTE_COST"] += num_bytes(value);
	}
	engine->gas_used += STOR_WRITE_COST + num_bytes(value) * STOR_WRITE_BYTE_COST;
	engine->check_gas();

	backend->write(contract, dst, key, value);
}

uint64_t StorageProxy::lookup(const addr_t& contract, const var_t& value) const
{
	if(engine->do_trace) {
		trace_t t;
		t.type = "LOOKUP";
		t.contract = contract;
		t.value = clone(value);
		trace.push_back(t);
	}
	const auto size = num_bytes(value);
	const auto blocks = size / 32;

	if(engine->do_profile) {
		engine->cost_map["STOR_READ_COST"]++;
		engine->cost_map["STOR_READ_32_BYTE_COST"] += blocks;
	}
	engine->gas_used += STOR_READ_COST + blocks * STOR_READ_32_BYTE_COST;
	engine->check_gas();

	return backend->lookup(contract, value);
}

std::unique_ptr<uint128> StorageProxy::get_balance(const addr_t& contract, const addr_t& currency)
{
	if(engine->do_trace) {
		trace_t t;
		t.type = "GET_BALANCE";
		t.contract = contract;
		t.value = to_binary(currency);
		trace.push_back(t);
	}
	if(engine->do_profile) {
		engine->cost_map["STOR_READ_COST"]++;
	}
	engine->gas_used += STOR_READ_COST;
	engine->check_gas();

	return backend->get_balance(contract, currency);
}


} // vm
} // mmx
