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

	std::unique_ptr<var_t> read(const addr_t& contract, const uint64_t src) const override;

	std::unique_ptr<var_t> read_ex(const addr_t& contract, const uint64_t src, const uint32_t height) const;

	std::unique_ptr<var_t> read(const addr_t& contract, const uint64_t src, const uint64_t key) const override;

	void write(const addr_t& contract, const uint64_t dst, const var_t& value) override;

	void write(const addr_t& contract, const uint64_t dst, const uint64_t key, const var_t& value) override;

	uint64_t lookup(const addr_t& contract, const var_t& value) const override;

	std::vector<std::pair<uint64_t, varptr_t>> find_range(
			const addr_t& contract, const uint64_t begin, const uint64_t end, const uint32_t height = -1) const;

	std::vector<std::pair<uint64_t, varptr_t>> find_entries(
			const addr_t& contract, const uint64_t address, const uint32_t height = -1) const;

	std::vector<varptr_t> read_array(
			const addr_t& contract, const uint64_t address, const uint32_t height = -1) const;

	void set_balance(const addr_t& contract, const addr_t& currency, const uint128& amount) override {}

	std::unique_ptr<uint128> get_balance(const addr_t& contract, const addr_t& currency) const override {
		return nullptr;
	}

	std::map<addr_t, uint128> get_balances(const addr_t& contract) const override {
		return {};
	}

private:
	std::shared_ptr<Table> table;
	std::shared_ptr<Table> table_entries;
	std::shared_ptr<Table> table_index;

};


} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_STORAGEDB_H_ */
