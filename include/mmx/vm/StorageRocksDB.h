/*
 * StorageRocksDB.h
 *
 *  Created on: Apr 22, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_VM_STORAGEROCKSDB_H_
#define INCLUDE_MMX_VM_STORAGEROCKSDB_H_

#include <mmx/vm/Storage.h>
#include <mmx/contract/height_info_t.hxx>

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

	var_t* read(const addr_t& contract, const uint64_t src, const uint64_t key) const override;

	void write(const addr_t& contract, const uint64_t dst, const var_t& value) override;

	void write(const addr_t& contract, const uint64_t dst, const uint64_t key, const var_t& value) override;

	uint64_t lookup(const addr_t& contract, const var_t& value) const override;

	void commit();

	void revert();

private:
	vnx::rocksdb::raw_table table;
	vnx::rocksdb::raw_table table_entries;
	vnx::rocksdb::raw_table table_index;
	vnx::rocksdb::table<std::pair<uint32_t, addr_t>, contract::height_info_t> table_log;

	std::map<addr_t, contract::height_info_t> log_buffer;

};


} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_STORAGEROCKSDB_H_ */
