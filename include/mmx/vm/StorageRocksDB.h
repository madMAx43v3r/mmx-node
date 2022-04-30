/*
 * StorageRocksDB.h
 *
 *  Created on: Apr 22, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_VM_STORAGEROCKSDB_H_
#define INCLUDE_MMX_VM_STORAGEROCKSDB_H_

#include <mmx/vm/Storage.h>


namespace mmx {
namespace vm {

class StorageRocksDB : public Storage {
public:
	var_t* read(const addr_t& contract, const uint64_t src) const override;

	var_t* read(const addr_t& contract, const uint64_t src, const uint64_t key) const override;

	void write(const addr_t& contract, const uint64_t dst, const var_t& value) override;

	void write(const addr_t& contract, const uint64_t dst, const uint64_t key, const var_t& value) override;

	uint64_t lookup(const addr_t& contract, const var_t& value) const override;

private:


};


} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_STORAGEROCKSDB_H_ */
