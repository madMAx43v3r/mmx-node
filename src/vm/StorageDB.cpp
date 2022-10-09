/*
 * StorageRocksDB.cpp
 *
 *  Created on: May 5, 2022
 *      Author: mad
 */

#include <mmx/vm/StorageDB.h>

#include <vnx/Output.hpp>


namespace mmx {
namespace vm {

// TODO: use 20-byte hash as keys
// TODO: use to_big_endian() function instead of flip_bytes()

std::shared_ptr<db_val_t> get_key(const addr_t& contract, uint64_t dst)
{
	auto out = std::make_shared<db_val_t>(40);
	::memcpy(out->data, contract.data(), contract.size());
	dst = vnx::flip_bytes(dst);
	::memcpy(out->data + contract.size(), &dst, 8);
	return out;
}

std::tuple<addr_t, uint64_t> read_key(std::shared_ptr<const db_val_t> key)
{
	if(key->size < 40) {
		throw std::runtime_error("key size underflow");
	}
	std::pair<addr_t, uint64_t> out;
	::memcpy(std::get<0>(out).data(), key->data, 32);
	::memcpy(&std::get<1>(out), key->data + 32, 8);
	std::get<1>(out) = vnx::flip_bytes(std::get<1>(out));
	return out;
}

std::shared_ptr<db_val_t> get_entry_key(const addr_t& contract, uint64_t dst, uint64_t key)
{
	auto out = std::make_shared<db_val_t>(48);
	::memcpy(out->data, contract.data(), contract.size());
	dst = vnx::flip_bytes(dst);
	::memcpy(out->data + contract.size(), &dst, 8);
	key = vnx::flip_bytes(key);
	::memcpy(out->data + contract.size() + 8, &key, 8);
	return out;
}

std::tuple<addr_t, uint64_t, uint64_t> read_entry_key(std::shared_ptr<const db_val_t> key)
{
	if(key->size < 48) {
		throw std::runtime_error("key size underflow");
	}
	std::tuple<addr_t, uint64_t, uint64_t> out;
	::memcpy(&std::get<0>(out), key->data, 32);
	::memcpy(&std::get<1>(out), key->data + 32, 8);
	::memcpy(&std::get<2>(out), key->data + 40, 8);
	std::get<1>(out) = vnx::flip_bytes(std::get<1>(out));
	std::get<2>(out) = vnx::flip_bytes(std::get<2>(out));
	return out;
}

std::shared_ptr<db_val_t> write_index_key(const addr_t& contract, const std::pair<const uint8_t*, size_t>& value)
{
	auto out = std::make_shared<db_val_t>(contract.size() + value.second);
	::memcpy(out->data, contract.data(), contract.size());
	::memcpy(out->data + contract.size(), value.first, value.second);
	return out;
}

StorageDB::StorageDB(const std::string& database_path, DataBase& db)
{
	table = std::make_shared<Table>(database_path + "storage");
	table_entries = std::make_shared<Table>(database_path + "storage_entries");
	table_index = std::make_shared<Table>(database_path + "storage_index");
	db.add(table);
	db.add(table_entries);
	db.add(table_index);
}

StorageDB::~StorageDB()
{
}

std::unique_ptr<var_t> StorageDB::read(const addr_t& contract, const uint64_t src) const
{
	return read_ex(contract, src, -1);
}

std::unique_ptr<var_t> StorageDB::read_ex(const addr_t& contract, const uint64_t src, const uint32_t height) const
{
	const auto key = get_key(contract, src);
	if(auto value = table->find(key, height)) {
		std::unique_ptr<var_t> var;
		deserialize(var, value->data, value->size);
		return var;
	}
	return nullptr;
}

std::unique_ptr<var_t> StorageDB::read(const addr_t& contract, const uint64_t src, const uint64_t key) const
{
	const auto entry_key = get_entry_key(contract, src, key);
	if(auto value = table_entries->find(entry_key)) {
		std::unique_ptr<var_t> var;
		deserialize(var, value->data, value->size, false);
		return var;
	}
	return nullptr;
}

void StorageDB::write(const addr_t& contract, const uint64_t dst, const var_t& value)
{
	const auto key = get_key(contract, dst);
	auto data = serialize(value);

	if(value.flags & FLAG_KEY) {
		const auto key = write_index_key(contract, std::make_pair(data.first.get() + 5, data.second - 5));
		table_index->insert(key, std::make_shared<db_val_t>(&dst, sizeof(dst)));
	}
	table->insert(key, std::make_shared<db_val_t>(data.first.release(), data.second, false));
}

void StorageDB::write(const addr_t& contract, const uint64_t dst, const uint64_t key, const var_t& value)
{
	const auto entry_key = get_entry_key(contract, dst, key);
	auto data = serialize(value, false);
	table_entries->insert(entry_key, std::make_shared<db_val_t>(data.first.release(), data.second, false));
}

uint64_t StorageDB::lookup(const addr_t& contract, const var_t& value) const
{
	const auto data = serialize(value, false, false);
	const auto key = write_index_key(contract, std::make_pair(data.first.get(), data.second));

	uint64_t out = 0;
	if(auto value = table_index->find(key)) {
		if(value->size < sizeof(out)) {
			throw std::runtime_error("value underflow");
		}
		::memcpy(&out, value->data, sizeof(out));
	}
	return out;
}

std::vector<std::pair<uint64_t, varptr_t>> StorageDB::find_range(
		const addr_t& contract, const uint64_t begin, const uint64_t end, const uint32_t height) const
{
	std::vector<std::pair<uint64_t, varptr_t>> out;
	std::vector<std::pair<std::shared_ptr<db_val_t>, uint64_t>> keys;

	Table::Iterator iter(table);
	iter.seek(get_key(contract, begin));
	while(iter.is_valid()) {
		const auto key = read_key(iter.key());
		const auto address = std::get<1>(key);
		if(std::get<0>(key) != contract || address >= end) {
			break;
		}
		keys.emplace_back(iter.key(), address);
		iter.next();
	}
	for(const auto& entry : keys) {
		if(auto value = table->find(entry.first, height)) {
			std::unique_ptr<var_t> var;
			deserialize(var, value->data, value->size);
			out.emplace_back(entry.second, std::move(var));
		}
	}
	return out;
}

std::vector<std::pair<uint64_t, varptr_t>> StorageDB::find_entries(
		const addr_t& contract, const uint64_t address, const uint32_t height) const
{
	std::vector<std::pair<uint64_t, varptr_t>> out;
	std::vector<std::pair<std::shared_ptr<db_val_t>, uint64_t>> keys;

	Table::Iterator iter(table_entries);
	iter.seek(get_entry_key(contract, address, 0));
	while(iter.is_valid()) {
		const auto key = read_entry_key(iter.key());
		if(std::get<0>(key) != contract || std::get<1>(key) != address) {
			break;
		}
		keys.emplace_back(iter.key(), std::get<2>(key));
		iter.next();
	}
	for(const auto& entry : keys) {
		if(auto value = table_entries->find(entry.first, height)) {
			std::unique_ptr<var_t> var;
			deserialize(var, value->data, value->size);
			out.emplace_back(entry.second, std::move(var));
		}
	}
	return out;
}

std::vector<varptr_t> StorageDB::read_array(
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
