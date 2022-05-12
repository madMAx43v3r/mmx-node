/*
 * StorageRocksDB.cpp
 *
 *  Created on: May 5, 2022
 *      Author: mad
 */

#include <mmx/vm/StorageRocksDB.h>

#include <vnx/Output.hpp>


namespace mmx {
namespace vm {

typedef std::array<char, 44> key_t;
typedef std::array<char, 52> entry_key_t;

key_t get_key(const addr_t& contract, uint64_t dst, uint32_t height)
{
	key_t out;
	::memcpy(out.data(), contract.data(), contract.size());
	dst = vnx::flip_bytes(dst);
	::memcpy(out.data() + contract.size(), &dst, 8);
	height = vnx::flip_bytes(height);
	::memcpy(out.data() + contract.size() + 8, &height, 4);
	return out;
}

std::tuple<addr_t, uint64_t, uint32_t> read_key(const char* data, const size_t length)
{
	if(length < 44) {
		throw std::runtime_error("unexpected eof");
	}
	std::tuple<addr_t, uint64_t, uint32_t> out;
	::memcpy(&std::get<0>(out), data, 32);
	::memcpy(&std::get<1>(out), data + 32, 8);
	::memcpy(&std::get<2>(out), data + 40, 4);
	std::get<1>(out) = vnx::flip_bytes(std::get<1>(out));
	std::get<2>(out) = vnx::flip_bytes(std::get<2>(out));
	return out;
}

entry_key_t get_entry_key(const addr_t& contract, uint64_t dst, uint64_t key, uint32_t height)
{
	entry_key_t out;
	::memcpy(out.data(), contract.data(), contract.size());
	dst = vnx::flip_bytes(dst);
	::memcpy(out.data() + contract.size(), &dst, 8);
	key = vnx::flip_bytes(key);
	::memcpy(out.data() + contract.size() + 8, &key, 8);
	height = vnx::flip_bytes(height);
	::memcpy(out.data() + contract.size() + 16, &height, 4);
	return out;
}

std::tuple<addr_t, uint64_t, uint64_t, uint32_t> read_entry_key(const char* data, const size_t length)
{
	if(length < 52) {
		throw std::runtime_error("unexpected eof");
	}
	std::tuple<addr_t, uint64_t, uint64_t, uint32_t> out;
	::memcpy(&std::get<0>(out), data, 32);
	::memcpy(&std::get<1>(out), data + 32, 8);
	::memcpy(&std::get<2>(out), data + 40, 8);
	::memcpy(&std::get<3>(out), data + 48, 4);
	std::get<1>(out) = vnx::flip_bytes(std::get<1>(out));
	std::get<2>(out) = vnx::flip_bytes(std::get<2>(out));
	std::get<3>(out) = vnx::flip_bytes(std::get<3>(out));
	return out;
}

std::pair<uint8_t*, size_t> write_index_key(const addr_t& contract, const std::pair<uint8_t*, size_t>& value)
{
	std::pair<uint8_t*, size_t> out;
	out.second = contract.size() + value.second;
	out.first = (uint8_t*)::malloc(out.second);
	::memcpy(out.first, contract.data(), contract.size());
	::memcpy(out.first + contract.size(), value.first, value.second);
	return out;
}

StorageRocksDB::StorageRocksDB(const std::string& database_path)
{
	::rocksdb::Options options;
	options.max_open_files = 16;
	options.keep_log_file_num = 3;
	options.max_manifest_file_size = 64 * 1024 * 1024;

	table.open(database_path + "storage", options);
	table_entries.open(database_path + "storage_entries", options);
	table_index.open(database_path + "storage_index", options);

	options.max_open_files = 4;

	table_log.open(database_path + "storage_log", options);
}

StorageRocksDB::~StorageRocksDB()
{
}

var_t* StorageRocksDB::read(const addr_t& contract, const uint64_t src) const
{
	return read_ex(contract, src, -1);
}

var_t* StorageRocksDB::read_ex(const addr_t& contract, const uint64_t src, const uint32_t height) const
{
	const auto key = get_key(contract, src, height);
	vnx::rocksdb::raw_ptr_t value;
	vnx::rocksdb::raw_ptr_t found_key;
	if(table.find_prev(std::make_pair(key.data(), key.size()), value, &found_key)) {
		if(found_key.size() == key.size() && ::memcmp(found_key.data(), key.data(), 40) == 0) {
			var_t* var = nullptr;
			deserialize(var, value.data(), value.size());
			return var;
		}
	}
	return nullptr;
}

var_t* StorageRocksDB::read(const addr_t& contract, const uint64_t src, const uint64_t key) const
{
	const auto entry_key = get_entry_key(contract, src, key, -1);
	vnx::rocksdb::raw_ptr_t value;
	vnx::rocksdb::raw_ptr_t found_key;
	if(table_entries.find_prev(std::make_pair(entry_key.data(), entry_key.size()), value, &found_key)) {
		if(found_key.size() == entry_key.size() && ::memcmp(found_key.data(), entry_key.data(), 48) == 0) {
			var_t* var = nullptr;
			deserialize(var, value.data(), value.size(), false);
			return var;
		}
	}
	return nullptr;
}

void StorageRocksDB::write(const addr_t& contract, const uint64_t dst, const var_t& value)
{
	const auto key = get_key(contract, dst, height);
	const auto data = serialize(value);
	table.insert(std::make_pair(key.data(), key.size()), data);
	log_buffer[contract].keys.push_back(dst);

	if(value.flags & FLAG_KEY) {
		const auto key = write_index_key(contract, std::make_pair(data.first + 5, data.second - 5));
		table_index.insert(key, std::make_pair(&dst, sizeof(dst)));
		log_buffer[contract].index_values.push_back(dst);
		::free(key.first);
	}
	::free(data.first);
}

void StorageRocksDB::write(const addr_t& contract, const uint64_t dst, const uint64_t key, const var_t& value)
{
	const auto entry_key = get_entry_key(contract, dst, key, height);
	const auto data = serialize(value, false);
	table_entries.insert(std::make_pair(entry_key.data(), entry_key.size()), data);
	log_buffer[contract].entry_keys.emplace_back(dst, key);
	::free(data.first);
}

uint64_t StorageRocksDB::lookup(const addr_t& contract, const var_t& value) const
{
	const auto data = serialize(value, false, false);
	const auto key = write_index_key(contract, data);
	uint64_t out = 0;
	vnx::rocksdb::raw_ptr_t tmp;
	if(table_index.find(key, tmp)) {
		::memcpy(&out, tmp.data(), std::min(tmp.size(), sizeof(out)));
	}
	::free(key.first);
	::free(data.first);
	return out;
}

void StorageRocksDB::commit()
{
	for(const auto& entry : log_buffer) {
		table_log.insert(std::make_pair(height, entry.first), entry.second);
	}
	log_buffer.clear();
}

void StorageRocksDB::revert(const uint32_t height)
{
	std::vector<std::pair<std::pair<uint32_t, addr_t>, contract::height_log_t>> entries;
	table_log.find_greater_equal(std::make_pair(height, addr_t()), entries);

	// TODO: parallel for
	for(const auto& entry : entries)
	{
		const auto& height = entry.first.first;
		const auto& contract = entry.first.second;
		for(const auto& addr : entry.second.index_values) {
			if(auto var = read(contract, addr)) {
				const auto data = serialize(*var, false, false);
				const auto key = write_index_key(contract, data);
				table_index.erase(key);
				::free(key.first);
				::free(data.first);
				delete var;
			}
		}
		for(const auto& addr : entry.second.keys) {
			const auto key = get_key(contract, addr, height);
			table.erase(std::make_pair(key.data(), key.size()));
		}
		for(const auto& addr : entry.second.entry_keys) {
			const auto key = get_entry_key(contract, addr.first, addr.second, height);
			table_entries.erase(std::make_pair(key.data(), key.size()));
		}
	}
	table_log.erase_greater_equal(std::make_pair(height, addr_t()));
}

std::vector<std::pair<uint64_t, varptr_t>> StorageRocksDB::find_range(
		const addr_t& contract, const uint64_t begin, const uint64_t end, const uint32_t height) const
{
	std::vector<std::pair<uint64_t, varptr_t>> out;
	std::unique_ptr<::rocksdb::Iterator> iter(table.iterator());

	if(begin >= end) {
		return out;
	}
	uint64_t address = end - 1;
	while(address >= begin) {
		const auto key_begin = get_key(contract, address, height);
		iter->SeekForPrev(::rocksdb::Slice(key_begin.data(), key_begin.size()));
		if(!iter->Valid()) {
			break;
		}
		const auto key = read_key(iter->key().data(), iter->key().size());
		address = std::get<1>(key);
		if(std::get<0>(key) != contract || address < begin) {
			break;
		}
		var_t* var = nullptr;
		deserialize(var, iter->value().data(), iter->value().size());
		out.emplace_back(address, var);
		if(address == 0) {
			break;
		}
		address--;
	}
	return out;
}

std::vector<std::pair<uint64_t, varptr_t>> StorageRocksDB::find_entries(
		const addr_t& contract, const uint64_t address, const uint32_t height) const
{
	std::vector<std::pair<uint64_t, varptr_t>> out;
	std::unique_ptr<::rocksdb::Iterator> iter(table_entries.iterator());

	uint64_t entry = -1;
	while(true) {
		const auto key_begin = get_entry_key(contract, address, entry, height);
		iter->SeekForPrev(::rocksdb::Slice(key_begin.data(), key_begin.size()));
		if(!iter->Valid()) {
			break;
		}
		const auto key = read_entry_key(iter->key().data(), iter->key().size());
		if(std::get<0>(key) != contract || std::get<1>(key) != address) {
			break;
		}
		entry = std::get<2>(key);
		var_t* var = nullptr;
		deserialize(var, iter->value().data(), iter->value().size(), false);
		out.emplace_back(entry, var);
		if(entry == 0) {
			break;
		}
		entry--;
	}
	return out;
}

std::vector<varptr_t> StorageRocksDB::read_array(
		const addr_t& contract, const uint64_t address, const uint32_t height) const
{
	std::vector<varptr_t> out;
	for(const auto& entry : find_entries(contract, address, height)) {
		out.push_back(entry.second);
	}
	return out;
}


} // vm
} // mmx
