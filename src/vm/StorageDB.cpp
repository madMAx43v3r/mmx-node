/*
 * StorageRocksDB.cpp
 *
 *  Created on: May 5, 2022
 *      Author: mad
 */

#include <mmx/vm/StorageDB.h>

#include <vnx/Output.hpp>
#include <vnx/Util.hpp>


namespace mmx {
namespace vm {

constexpr int HASH_KEY_SIZE = 20;

std::shared_ptr<db_val_t> get_key(const addr_t& contract, uint64_t dst)
{
	auto out = std::make_shared<db_val_t>(HASH_KEY_SIZE + 8);
	::memcpy(out->data, contract.data(), HASH_KEY_SIZE);
	dst = vnx::to_big_endian(dst);
	::memcpy(out->data + HASH_KEY_SIZE, &dst, 8);
	return out;
}

std::tuple<addr_t, uint64_t> read_key(std::shared_ptr<const db_val_t> key)
{
	if(key->size < HASH_KEY_SIZE + 8) {
		throw std::runtime_error("key size underflow");
	}
	std::tuple<addr_t, uint64_t> out;
	::memcpy(std::get<0>(out).data(), key->data, HASH_KEY_SIZE);
	::memcpy(&std::get<1>(out), key->data + HASH_KEY_SIZE, 8);
	std::get<1>(out) = vnx::from_big_endian(std::get<1>(out));
	return out;
}

std::shared_ptr<db_val_t> write_entry_key(const addr_t& contract, uint64_t dst, uint64_t key)
{
	auto out = std::make_shared<db_val_t>(HASH_KEY_SIZE + 16);
	::memcpy(out->data, contract.data(), HASH_KEY_SIZE);
	dst = vnx::to_big_endian(dst);
	::memcpy(out->data + HASH_KEY_SIZE, &dst, 8);
	key = vnx::to_big_endian(key);
	::memcpy(out->data + HASH_KEY_SIZE + 8, &key, 8);
	return out;
}

std::tuple<addr_t, uint64_t, uint64_t> read_entry_key(std::shared_ptr<const db_val_t> key)
{
	if(key->size < HASH_KEY_SIZE + 16) {
		throw std::runtime_error("entry key size underflow");
	}
	std::tuple<addr_t, uint64_t, uint64_t> out;
	::memcpy(std::get<0>(out).data(), key->data, HASH_KEY_SIZE);
	::memcpy(&std::get<1>(out), key->data + HASH_KEY_SIZE, 8);
	::memcpy(&std::get<2>(out), key->data + HASH_KEY_SIZE + 8, 8);
	std::get<1>(out) = vnx::from_big_endian(std::get<1>(out));
	std::get<2>(out) = vnx::from_big_endian(std::get<2>(out));
	return out;
}

std::shared_ptr<db_val_t> write_index_key(const addr_t& contract, const var_t& value)
{
	auto data = serialize(value, false, false);
	auto out = std::make_shared<db_val_t>(contract.size() + data.second);
	::memcpy(out->data, contract.data(), contract.size());
	::memcpy(out->data + contract.size(), data.first.get(), data.second);
	return out;
}

StorageDB::StorageDB(const std::string& database_path, std::shared_ptr<DataBase> db)
{
	table = std::make_shared<Table>(database_path + "storage");
	table_entries = std::make_shared<Table>(database_path + "storage_entries");
	table_index = std::make_shared<Table>(database_path + "storage_index");
	db->add(table);
	db->add(table_entries);
	db->add(table_index);
}

StorageDB::~StorageDB()
{
}

std::unique_ptr<var_t> StorageDB::read(const addr_t& contract, const uint64_t src) const
{
	return read_ex(contract, src, -1);
}

std::unique_ptr<var_t> StorageDB::read(const addr_t& contract, const uint64_t src, const uint64_t key) const
{
	return read_ex(contract, src, key, -1);
}

std::unique_ptr<var_t> StorageDB::read_ex(const addr_t& contract, const uint64_t src, const uint32_t height) const
{
	const auto key = get_key(contract, src);
	if(auto value = table->find(key, height)) {
		std::unique_ptr<var_t> var;
		deserialize(var, value->data, value->size, false);
		return var;
	}
	return nullptr;
}

std::unique_ptr<var_t> StorageDB::read_ex(const addr_t& contract, const uint64_t src, const uint64_t key, const uint32_t height) const
{
	const auto entry_key = write_entry_key(contract, src, key);
	if(auto value = table_entries->find(entry_key, height)) {
		std::unique_ptr<var_t> var;
		deserialize(var, value->data, value->size, false);
		return var;
	}
	return nullptr;
}

void StorageDB::write(const addr_t& contract, const uint64_t dst, const var_t& value)
{
	const auto key = get_key(contract, dst);
	auto data = serialize(value, false);

	if(value.flags & FLAG_KEY) {
		const auto key = write_index_key(contract, value);
		table_index->insert(key, std::make_shared<db_val_t>(&dst, sizeof(dst)));
	}
	table->insert(key, std::make_shared<db_val_t>(data.first.release(), data.second, false));
}

void StorageDB::write(const addr_t& contract, const uint64_t dst, const uint64_t key, const var_t& value)
{
	const auto entry_key = write_entry_key(contract, dst, key);
	auto data = serialize(value, false);
	table_entries->insert(entry_key, std::make_shared<db_val_t>(data.first.release(), data.second, false));
}

uint64_t StorageDB::lookup(const addr_t& contract, const var_t& value) const
{
	const auto key = write_index_key(contract, value);

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
		if(::memcmp(std::get<0>(key).data(), contract.data(), HASH_KEY_SIZE) || address >= end) {
			break;
		}
		keys.emplace_back(iter.key(), address);
		iter.next();
	}
	for(const auto& entry : keys) {
		if(auto value = table->find(entry.first, height)) {
			std::unique_ptr<var_t> var;
			deserialize(var, value->data, value->size, false);
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
	iter.seek(write_entry_key(contract, address, 0));
	while(iter.is_valid()) {
		const auto key = read_entry_key(iter.key());
		if(::memcmp(std::get<0>(key).data(), contract.data(), HASH_KEY_SIZE) || std::get<1>(key) != address) {
			break;
		}
		keys.emplace_back(iter.key(), std::get<2>(key));
		iter.next();
	}
	for(const auto& entry : keys) {
		if(auto value = table_entries->find(entry.first, height)) {
			std::unique_ptr<var_t> var;
			deserialize(var, value->data, value->size, false);
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

void StorageDB::set_balance(const addr_t& contract, const addr_t& currency, const uint128& amount)
{
	if(write_balance) {
		write_balance(contract, currency, amount);
	}
}

std::unique_ptr<uint128> StorageDB::get_balance(const addr_t& contract, const addr_t& currency)
{
	if(read_balance) {
		return read_balance(contract, currency);
	}
	return nullptr;
}


} // vm
} // mmx
