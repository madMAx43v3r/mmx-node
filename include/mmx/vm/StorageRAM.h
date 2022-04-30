/*
 * StorageRAM.h
 *
 *  Created on: Apr 30, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_VM_STORAGERAM_H_
#define INCLUDE_MMX_VM_STORAGERAM_H_

#include <mmx/vm/Storage.h>


namespace mmx {
namespace vm {

class StorageRAM : public Storage {
public:
	~StorageRAM();

	var_t* read(const addr_t& contract, const uint64_t src) const override;

	var_t* read(const addr_t& contract, const uint64_t src, const uint64_t key) const override;

	void write(const addr_t& contract, const uint64_t dst, const var_t& value) override;

	void write(const addr_t& contract, const uint64_t dst, const uint64_t key, const var_t& value) override;

	uint64_t lookup(const addr_t& contract, const var_t& value) const override;

private:
	void erase_entries(const addr_t& contract, const uint64_t dst);

	void erase(const addr_t& contract, var_t* var);

private:
	std::map<std::pair<addr_t, uint64_t>, var_t*> memory;
	std::map<std::pair<std::pair<addr_t, uint64_t>, uint64_t>, var_t*> entries;
	std::map<std::pair<addr_t, const var_t*>, uint64_t, varptr_less_t> key_map;

};


} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_STORAGERAM_H_ */
