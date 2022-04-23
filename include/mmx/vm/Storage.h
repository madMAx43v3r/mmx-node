/*
 * Storage.h
 *
 *  Created on: Apr 22, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_VM_STORAGE_H_
#define INCLUDE_MMX_VM_STORAGE_H_

#include <mmx/vm/var_t.h>

#include <vector>


namespace mmx {
namespace vm {

class Storage {
public:
	virtual ~Storage() {}

	virtual var_t* read(const uint256_t& contract, const uint64_t address) const = 0;

	virtual var_t* read(const uint256_t& contract, const uint64_t address, const uint64_t key) const = 0;

	virtual void write(const uint256_t& contract, const uint64_t address, const var_t& value) = 0;

	virtual void write(const uint256_t& contract, const uint64_t address, const uint64_t key, const var_t& value) = 0;

	virtual void erase(const uint256_t& contract, const uint64_t address) = 0;

	virtual void erase(const uint256_t& contract, const uint64_t address, const uint64_t key) = 0;

	virtual void add_key(const uint256_t& contract, const uint64_t address) = 0;

	virtual uint64_t lookup(const uint256_t& contract, const var_t& value) const = 0;


};


} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_STORAGE_H_ */
