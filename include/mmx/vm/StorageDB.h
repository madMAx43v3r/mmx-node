/*
 * StorageDB.h
 *
 *  Created on: Apr 22, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_VM_STORAGEDB_H_
#define INCLUDE_MMX_VM_STORAGEDB_H_

#include <mmx/vm/Storage.h>
#include <mmx/vm/varptr_t.hpp>
#include <mmx/DataBase.h>


namespace mmx {
namespace vm {

class StorageDB : public Storage {
public:
	StorageDB(const std::string& database_path, DataBase& db);

	~StorageDB();

	var_t* read(const addr_t& contract, const uint64_t src) const override;

	var_t* read_ex(const addr_t& contract, const uint64_t src, const uint32_t height) const;

	var_t* read(const addr_t& contract, const uint64_t src, const uint64_t key) const override;

	void write(const addr_t& contract, const uint64_t dst, const var_t& value) override;

	void write(const addr_t& contract, const uint64_t dst, const uint64_t key, const var_t& value) override;

	uint64_t lookup(const addr_t& contract, const var_t& value) const override;

	std::vector<std::pair<uint64_t, varptr_t>> find_range(
			const addr_t& contract, const uint64_t begin, const uint64_t end, const uint32_t height = -1) const;

	std::vector<std::pair<uint64_t, varptr_t>> find_entries(
			const addr_t& contract, const uint64_t address, const uint32_t height = -1) const;

	std::vector<varptr_t> read_array(
			const addr_t& contract, const uint64_t address, const uint32_t height = -1) const;

private:
	std::shared_ptr<Table> table;
	std::shared_ptr<Table> table_entries;
	std::shared_ptr<Table> table_index;

};


} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_STORAGEDB_H_ */
