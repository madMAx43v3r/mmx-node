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

	virtual var_t* read(const uint256_t& contract, const uint32_t address) const = 0;

	virtual var_t* read(const uint256_t& contract, const uint32_t address, const uint32_t key) const = 0;

	virtual void write(const uint256_t& contract, const uint32_t address, const var_t& value) = 0;

	virtual void write(const uint256_t& contract, const uint32_t address, const uint32_t key, const var_t& value) = 0;

	virtual void erase(const uint256_t& contract, const uint32_t address) = 0;

	virtual void erase(const uint256_t& contract, const uint32_t address, const uint32_t key) = 0;

	virtual uint32_t lookup(const uint256_t& contract, const var_t& value) const = 0;

	virtual std::vector<uint32_t> get_free_cells(const uint32_t offset) const = 0;


};


} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_STORAGE_H_ */
