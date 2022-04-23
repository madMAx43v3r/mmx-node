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
	var_t* read(const uint256_t& contract, const uint32_t address) const override;

	var_t* read(const uint256_t& contract, const uint32_t address, const uint32_t key) const override;

	void write(const uint256_t& contract, const uint32_t address, const var_t& value) override;

	void write(const uint256_t& contract, const uint32_t address, const uint32_t key, const var_t& value) override;

	void erase(const uint256_t& contract, const uint32_t address) override;

	void erase(const uint256_t& contract, const uint32_t address, const uint32_t key) override;

private:


};


} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_STORAGEROCKSDB_H_ */
