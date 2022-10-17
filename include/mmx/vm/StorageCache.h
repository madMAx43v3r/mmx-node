/*
 * StorageCache.h
 *
 *  Created on: May 5, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_VM_STORAGECACHE_H_
#define INCLUDE_MMX_VM_STORAGECACHE_H_

#include <mmx/vm/StorageRAM.h>


namespace mmx {
namespace vm {

class StorageCache : public StorageRAM {
public:
	typedef StorageRAM Super;

	StorageCache(std::shared_ptr<Storage> backend);

	~StorageCache();

	std::unique_ptr<var_t> read(const addr_t& contract, const uint64_t src) const override;

	std::unique_ptr<var_t> read(const addr_t& contract, const uint64_t src, const uint64_t key) const override;

	uint64_t lookup(const addr_t& contract, const var_t& value) const override;

	void commit() const;

private:
	std::shared_ptr<Storage> backend;

};


} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_STORAGECACHE_H_ */
