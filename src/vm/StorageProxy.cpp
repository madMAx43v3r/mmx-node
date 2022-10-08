/*
 * StorageProxy.cpp
 *
 *  Created on: Apr 24, 2022
 *      Author: mad
 */

#include <mmx/vm/StorageProxy.h>


namespace mmx {
namespace vm {

StorageProxy::StorageProxy(std::shared_ptr<Storage> backend, bool read_only)
	:	backend(backend),
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
	return read_ex(backend->read(contract, src));
}

std::unique_ptr<var_t> StorageProxy::read(const addr_t& contract, const uint64_t src, const uint64_t key) const
{
	return read_ex(backend->read(contract, src, key));
}

void StorageProxy::write(const addr_t& contract, const uint64_t dst, const var_t& value)
{
	if(read_only) {
		throw std::logic_error("read-only storage");
	}
	if((value.flags & FLAG_KEY) && !(value.flags & FLAG_CONST)) {
		throw std::logic_error("keys need to be const");
	}
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
	backend->write(contract, dst, key, value);
}

uint64_t StorageProxy::lookup(const addr_t& contract, const var_t& value) const
{
	return backend->lookup(contract, value);
}


} // vm
} // mmx
