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

var_t* StorageProxy::read_ex(var_t* var, const uint64_t src, const uint64_t* key) const
{
	if(var) {
		if(var->flags & FLAG_DELETED) {
			delete var;
			var = nullptr;
		}
	}
	if(var) {
		var->flags &= ~FLAG_DIRTY;
		var->flags |= FLAG_STORED;
		if(read_only) {
			var->flags |= FLAG_CONST;
		}
	}
	num_read++;
	bytes_read += num_bytes(var);
	return var;
}

var_t* StorageProxy::read(const addr_t& contract, const uint64_t src) const
{
	return read_ex(backend->read(contract, src), src);
}

var_t* StorageProxy::read(const addr_t& contract, const uint64_t src, const uint64_t key) const
{
	return read_ex(backend->read(contract, src, key), src, &key);
}

void StorageProxy::write(const addr_t& contract, const uint64_t dst, const var_t& value)
{
	if(read_only) {
		throw std::logic_error("read-only storage");
	}
	if((value.flags & FLAG_KEY) && !(value.flags & FLAG_CONST)) {
		throw std::logic_error("keys need to be const");
	}
	num_write++;
	bytes_write += num_bytes(value) * (value.flags & FLAG_KEY ? 2 : 1);
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
	num_write++;
	bytes_write += num_bytes(value);
	backend->write(contract, dst, key, value);
}

uint64_t StorageProxy::lookup(const addr_t& contract, const var_t& value) const
{
	num_lookup++;
	bytes_lookup += num_bytes(value);
	return backend->lookup(contract, value);
}


} // vm
} // mmx
