/*
 * StorageRocksDB.h
 *
 *  Created on: Apr 22, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_VM_STORAGEROCKSDB_H_
#define INCLUDE_MMX_VM_STORAGEROCKSDB_H_

#include <mmx/vm/Storage.h>
#include <mmx/vm/varptr_t.hpp>
#include <mmx/contract/height_log_t.hxx>

#include <vnx/rocksdb/table.h>
#include <vnx/rocksdb/raw_table.h>

#include <map>


namespace mmx {
namespace vm {

class StorageRocksDB : public Storage {
public:
	StorageRocksDB(const std::string& database_path);

	~StorageRocksDB();

	uint32_t height = 0;

	var_t* read(const addr_t& contract, const uint64_t src) const override;

	var_t* read_ex(const addr_t& contract, const uint64_t src, const uint32_t height) const;

	var_t* read(const addr_t& contract, const uint64_t src, const uint64_t key) const override;

	void write(const addr_t& contract, const uint64_t dst, const var_t& value) override;

	void write(const addr_t& contract, const uint64_t dst, const uint64_t key, const var_t& value) override;

	uint64_t lookup(const addr_t& contract, const var_t& value) const override;

	void commit();

	void revert(const uint32_t height);

	std::vector<std::pair<uint64_t, varptr_t>> find_range(
			const addr_t& contract, const uint64_t begin, const uint64_t end, const uint32_t height = -1) const;

	std::vector<std::pair<uint64_t, varptr_t>> find_entries(
			const addr_t& contract, const uint64_t address, const uint32_t height = -1) const;

	std::vector<varptr_t> read_array(
			const addr_t& contract, const uint64_t address, const uint32_t height = -1) const;

private:
	vnx::rocksdb::raw_table table;
	vnx::rocksdb::raw_table table_entries;
	vnx::rocksdb::raw_table table_index;

	vnx::rocksdb::table<std::pair<uint32_t, addr_t>, contract::height_log_t> table_log;

	std::map<addr_t, contract::height_log_t> log_buffer;

};


} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_STORAGEROCKSDB_H_ */
