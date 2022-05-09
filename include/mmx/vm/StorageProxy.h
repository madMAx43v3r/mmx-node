/*
 * StorageProxy.h
 *
 *  Created on: Apr 24, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_VM_STORAGEPROXY_H_
#define INCLUDE_MMX_VM_STORAGEPROXY_H_

#include <mmx/vm/Storage.h>


namespace mmx {
namespace vm {

class StorageProxy : public Storage {
public:
	const std::shared_ptr<Storage> backend;
	const bool read_only;

	mutable uint64_t num_read = 0;
	mutable uint64_t num_write = 0;
	mutable uint64_t num_bytes_read = 0;
	mutable uint64_t num_bytes_write = 0;

	StorageProxy(std::shared_ptr<Storage> backend, bool read_only);

	var_t* read(const addr_t& contract, const uint64_t src) const override;

	var_t* read(const addr_t& contract, const uint64_t src, const uint64_t key) const override;

	void write(const addr_t& contract, const uint64_t dst, const var_t& value) override;

	void write(const addr_t& contract, const uint64_t dst, const uint64_t key, const var_t& value) override;

	uint64_t lookup(const addr_t& contract, const var_t& value) const override;

private:
	var_t* read_ex(var_t* var) const;

};


} // vm
} // mmx



#endif /* INCLUDE_MMX_VM_STORAGEPROXY_H_ */
