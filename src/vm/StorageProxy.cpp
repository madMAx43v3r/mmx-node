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

	engine->gas_used += STOR_READ_COST + num_bytes(var.get()) * STOR_READ_BYTE_COST;
	engine->check_gas();

	return var;
}

std::unique_ptr<var_t> StorageProxy::read(const addr_t& contract, const uint64_t src, const uint64_t key) const
{
	auto var = read_ex(backend->read(contract, src, key));

	engine->gas_used += STOR_READ_COST + num_bytes(var.get()) * STOR_READ_BYTE_COST;
	engine->check_gas();

	return var;
}

void StorageProxy::write(const addr_t& contract, const uint64_t dst, const var_t& value)
{
	if(read_only) {
		throw std::logic_error("read-only storage");
	}
	if((value.flags & FLAG_KEY) && !(value.flags & FLAG_CONST)) {
		throw std::logic_error("keys need to be const");
	}
	engine->gas_used += (STOR_WRITE_COST + num_bytes(value) * STOR_WRITE_BYTE_COST) * (value.flags & FLAG_KEY ? 2 : 1);
	engine->check_gas();

	backend->write(contract, dst, value);
}

void StorageProxy::write(const addr_t& contract, const uint64_t dst, const uint64_t key, const var_t& value)
{
	if(read_only) {
		throw std::logic_error("read-only storage");
	}
	if(value.ref_count) {
		throw std::logic_error("entries cannot have ref_count > 0");
	}
	engine->gas_used += STOR_WRITE_COST + num_bytes(value) * STOR_WRITE_BYTE_COST;
	engine->check_gas();

	backend->write(contract, dst, key, value);
}

uint64_t StorageProxy::lookup(const addr_t& contract, const var_t& value) const
{
	engine->gas_used += STOR_READ_COST + num_bytes(value) * STOR_READ_BYTE_COST;
	engine->check_gas();

	return backend->lookup(contract, value);
}


} // vm
} // mmx
