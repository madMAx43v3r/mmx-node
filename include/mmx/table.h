/*
 * table.h
 *
 *  Created on: Jun 20, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_TABLE_H_
#define INCLUDE_MMX_TABLE_H_

#include <mmx/DataBase.h>
#include <mmx/addr_t.hpp>

#include <vnx/Type.h>
#include <vnx/Input.hpp>
#include <vnx/Output.hpp>
#include <vnx/Memory.hpp>
#include <vnx/Buffer.hpp>

#include <tuple>


namespace mmx {

void sync_type_codes(const std::string& file_path);

template<typename K, typename V>
class table {
public:
	table(bool disable_type_codes = true)
		:	stream(disable_type_codes)
	{
		vnx::type<V>().create_dynamic_code(value_code);
		value_type = vnx::type<V>().get_type_code();
	}

	table(const std::string& file_path, bool disable_type_codes = true)
		:	table(disable_type_codes)
	{
		open(file_path);
	}

	virtual ~table() {
		close();
	}

	std::shared_ptr<Table> open(const std::string& file_path)
	{
		close();
		return db = std::make_shared<Table>(file_path);
	}

	void close() {
		db = nullptr;
	}

	void insert(const K& key, const V& value)
	{
		db->insert(write(key), write(value, value_type, value_code));
	}

	bool count(const K& key, const uint32_t max_version = -1) const
	{
		V dummy;
		return find(key, dummy, max_version);
	}

	bool find(const K& key, V& value, const uint32_t max_version = -1) const
	{
		const auto entry = db->find(write(key), max_version);
		if(entry) {
			read(entry, value, value_type, value_code);
			return true;
		}
		return false;
	}

	bool find_first(V& value) const
	{
		K dummy;
		return find_first(dummy, value);
	}

	bool find_first(K& key, V& value) const
	{
		Table::Iterator iter(db);
		iter.seek_begin();
		if(iter.is_valid()) {
			read(iter.key(), key);
			read(iter.value(), value, value_type, value_code);
			return true;
		}
		key = K();
		value = V();
		return false;
	}

	bool find_last(V& value) const
	{
		K dummy;
		return find_last(dummy, value);
	}

	bool find_last(K& key, V& value) const
	{
		Table::Iterator iter(db);
		iter.seek_last();
		if(iter.is_valid()) {
			read(iter.key(), key);
			read(iter.value(), value, value_type, value_code);
			return true;
		}
		key = K();
		value = V();
		return false;
	}

	size_t find_greater_equal(const K& key, std::vector<V>& values, const size_t limit = -1) const
	{
		values.clear();

		Table::Iterator iter(db);
		iter.seek(write(key));
		while(iter.is_valid() && values.size() < limit) {
			try {
				V value;
				read(iter.value(), value, value_type, value_code);
				values.push_back(std::move(value));
			} catch(...) {
				// ignore
			}
			iter.next();
		}
		return values.size();
	}

	size_t find_greater_equal(const K& key, std::vector<std::pair<K, V>>& result, const size_t limit = -1) const
	{
		result.clear();

		Table::Iterator iter(db);
		iter.seek(write(key));
		while(iter.is_valid() && result.size() < limit) {
			try {
				std::pair<K, V> tmp;
				read(iter.key(), tmp.first);
				read(iter.value(), tmp, value_type, value_code);
				result.push_back(std::move(tmp));
			} catch(...) {
				// ignore
			}
			iter.next();
		}
		return result.size();
	}

	size_t find_range(const K& begin, const K& end, std::vector<V>& values, const size_t limit = -1) const
	{
		values.clear();

		Table::Iterator iter(db);
		iter.seek(write(begin));
		while(iter.is_valid() && values.size() < limit) {
			K key;
			read(iter.key(), key);
			if(!(key < end)) {
				break;
			}
			try {
				V value;
				read(iter.value(), value, value_type, value_code);
				values.push_back(std::move(value));
			} catch(...) {
				// ignore
			}
			iter.next();
		}
		return values.size();
	}

	size_t find_range(const K& begin, const K& end, std::vector<std::pair<K, V>>& result, const size_t limit = -1) const
	{
		result.clear();

		Table::Iterator iter(db);
		iter.seek(write(begin));
		while(iter.is_valid() && result.size() < limit) {
			K key;
			read(iter.key(), key);
			if(!(key < end)) {
				break;
			}
			try {
				std::pair<K, V> tmp;
				tmp.first = key;
				read(iter.value(), tmp.second, value_type, value_code);
				result.push_back(std::move(tmp));
			} catch(...) {
				// ignore
			}
			iter.next();
		}
		return result.size();
	}

	size_t find_last_range(const K& begin, const K& end, std::vector<V>& result, const size_t limit) const
	{
		result.clear();

		Table::Iterator iter(db);
		iter.seek_prev(write(end));
		while(iter.is_valid() && result.size() < limit) {
			K key;
			read(iter.key(), key);
			if(!(begin < key || key == begin)) {
				break;
			}
			try {
				V value;
				read(iter.value(), value, value_type, value_code);
				result.push_back(std::move(value));
			} catch(...) {
				// ignore
			}
			iter.prev();
		}
		return result.size();
	}

	size_t find_last_range(const K& begin, const K& end, std::vector<std::pair<K, V>>& result, const size_t limit) const
	{
		result.clear();

		Table::Iterator iter(db);
		iter.seek_prev(write(end));
		while(iter.is_valid() && result.size() < limit) {
			K key;
			read(iter.key(), key);
			if(!(begin < key || key == begin)) {
				break;
			}
			try {
				std::pair<K, V> tmp;
				tmp.first = key;
				read(iter.value(), tmp.second, value_type, value_code);
				result.push_back(std::move(tmp));
			} catch(...) {
				// ignore
			}
			iter.prev();
		}
		return result.size();
	}

	void scan(const std::function<void(const K&, const V&)>& callback) const
	{
		Table::Iterator iter(db);
		iter.seek_begin();
		while(iter.is_valid()) {
			K key;
			V value;
			bool valid = false;
			try {
				read(iter.key(), key);
				read(iter.value(), value, value_type, value_code);
				valid = true;
			} catch(...) {
				// ignore
			}
			if(valid) {
				callback(key, value);
			}
			iter.next();
		}
	}

	void commit() {
		db->commit(db->current_version() + 1);
	}

	void commit(const uint32_t new_version) {
		db->commit(new_version);
	}

	void revert(const uint32_t new_version) {
		db->revert(new_version);
	}

	void flush() {
		db->flush();
	}

	Table::Iterator iterator() const {
		return Table::Iterator(db);
	}

	std::shared_ptr<Table> get_impl() const {
		return db;
	}

	std::string get_path() const {
		return db->root_path;
	}

protected:
	struct stream_t {
		vnx::Memory memory;
		vnx::Buffer buffer;
		vnx::MemoryOutputStream stream;
		vnx::TypeOutput out;
		stream_t(bool disable_type_codes = true) : stream(&memory), out(&stream) {
			out.disable_type_codes = disable_type_codes;
		}
	};

	virtual void read(std::shared_ptr<const db_val_t> entry, K& key) const = 0;

	virtual std::shared_ptr<db_val_t> write(const K& key) const = 0;

	void read(std::shared_ptr<const db_val_t> slice, V& value, const vnx::TypeCode* type_code, const std::vector<uint16_t>& code) const
	{
		vnx::PointerInputStream stream(slice->data, slice->size);
		vnx::TypeInput in(&stream);
		vnx::read(in, value, type_code, type_code ? nullptr : code.data());
	}

	std::shared_ptr<db_val_t> write(const V& value, const vnx::TypeCode* type_code, const std::vector<uint16_t>& code)
	{
		stream.out.reset();
		stream.memory.clear();

		vnx::write(stream.out, value, type_code, type_code ? nullptr : code.data());

		if(stream.memory.get_size()) {
			stream.out.flush();
			stream.buffer = stream.memory;
			return std::make_shared<db_val_t>(stream.buffer.data(), stream.buffer.size());
		}
		return std::make_shared<db_val_t>(stream.out.get_buffer(), stream.out.get_buffer_pos());
	}

protected:
	std::shared_ptr<Table> db;

	stream_t stream;
	std::vector<uint16_t> value_code;
	const vnx::TypeCode* value_type = nullptr;

};


template<typename K, typename V>
class uint_table : public table<K, V> {
public:
	uint_table() : table<K, V>() {}
	uint_table(const std::string& file_path, bool disable_type_codes = true) : table<K, V>(file_path, disable_type_codes) {}
protected:
	// TODO: use a to_big_endian() function
	void read(std::shared_ptr<const db_val_t> entry, K& key) const override {
		if(entry->size != sizeof(K)) {
			throw std::logic_error("key size mismatch: " + std::to_string(entry->size) + " (" + table<K, V>::get_path() + ")");
		}
		key = vnx::flip_bytes(*((const K*)entry->data));
	}
	std::shared_ptr<db_val_t> write(const K& key) const override {
		auto out = std::make_shared<mmx::db_val_t>(sizeof(K));
		vnx::write_value(out->data, vnx::flip_bytes(key));
		return out;
	}
};

template<typename K, typename I, typename V>
class uint_uint_table : public table<std::pair<K, I>, V> {
public:
	uint_uint_table() : table<std::pair<K, I>, V>() {}
	uint_uint_table(const std::string& file_path) : table<std::pair<K, I>, V>(file_path) {}
protected:
	// TODO: use a to_big_endian() function
	void read(std::shared_ptr<const db_val_t> entry, std::pair<K, I>& key) const override {
		if(entry->size != sizeof(K) + sizeof(I)) {
			throw std::logic_error("key size mismatch: " + std::to_string(entry->size) + " (" + table<std::pair<K, I>, V>::get_path() + ")");
		}
		key.first = vnx::flip_bytes(*((const K*)entry->data));
		key.second = vnx::flip_bytes(*((const I*)(entry->data + sizeof(K))));
	}
	std::shared_ptr<db_val_t> write(const std::pair<K, I>& key) const override {
		auto out = std::make_shared<mmx::db_val_t>(sizeof(K) + sizeof(I));
		vnx::write_value(out->data, vnx::flip_bytes(key.first));
		vnx::write_value(out->data + sizeof(K), vnx::flip_bytes(key.second));
		return out;
	}
};

template<typename K, typename V>
class hash_table : public table<K, V> {
public:
	hash_table() : table<K, V>() {}
	hash_table(const std::string& file_path) : table<K, V>(file_path) {}
protected:
	void read(std::shared_ptr<const db_val_t> entry, K& key) const override {
		if(entry->size != key.size()) {
			throw std::logic_error("key size mismatch: " + std::to_string(entry->size) + " (" + table<K, V>::get_path() + ")");
		}
		::memcpy(key.data(), entry->data, key.size());
	}
	std::shared_ptr<db_val_t> write(const K& key) const override {
		auto out = std::make_shared<mmx::db_val_t>(key.size());
		::memcpy(out->data, key.data(), key.size());
		return out;
	}
};

template<typename K, typename H, typename I, typename V>
class hash_uint_uint_table : public table<std::tuple<K, H, I>, V> {
public:
	hash_uint_uint_table() : table<std::tuple<K, H, I>, V>() {}
	hash_uint_uint_table(const std::string& file_path) : table<std::tuple<K, H, I>, V>(file_path) {}
protected:
	void read(std::shared_ptr<const db_val_t> entry, std::tuple<K, H, I>& key) const override {
		auto& hash = std::get<0>(key);
		if(entry->size != hash.size() + sizeof(H) + sizeof(I)) {
			throw std::logic_error("key size mismatch: " + std::to_string(entry->size) + " (" + table<std::tuple<K, H, I>, V>::get_path() + ")");
		}
		auto* src = entry->data;
		::memcpy(hash.data(), src, hash.size()); src += hash.size();
		std::get<1>(key) = vnx::flip_bytes(*((const H*)src)); src += sizeof(H);
		std::get<2>(key) = vnx::flip_bytes(*((const I*)src));
	}
	std::shared_ptr<db_val_t> write(const std::tuple<K, H, I>& key) const override {
		const auto& hash = std::get<0>(key);
		auto out = std::make_shared<mmx::db_val_t>(hash.size() + sizeof(H) + sizeof(I));
		auto* dst = out->data;
		::memcpy(dst, hash.data(), hash.size()); dst += hash.size();
		vnx::write_value(dst, vnx::flip_bytes(std::get<1>(key))); dst += sizeof(H);
		vnx::write_value(dst, vnx::flip_bytes(std::get<2>(key)));
		return out;
	}
};

template<typename V>
class balance_table_t : public table<std::pair<addr_t, addr_t>, V> {
public:
	balance_table_t() : table<std::pair<addr_t, addr_t>, V>() {}
	balance_table_t(const std::string& file_path) : table<std::pair<addr_t, addr_t>, V>(file_path) {}
protected:
	void read(std::shared_ptr<const db_val_t> entry, std::pair<addr_t, addr_t>& key) const override {
		if(entry->size != 64) {
			throw std::logic_error("key size mismatch: " + std::to_string(entry->size) + " (" + table<std::pair<addr_t, addr_t>, V>::get_path() + ")");
		}
		::memcpy(key.first.data(), entry->data, 32);
		::memcpy(key.second.data(), entry->data + 32, 32);
	}
	std::shared_ptr<db_val_t> write(const std::pair<addr_t, addr_t>& key) const override {
		auto out = std::make_shared<mmx::db_val_t>(64);
		::memcpy(out->data, key.first.data(), 32);
		::memcpy(out->data + 32, key.second.data(), 32);
		return out;
	}
};


} // mmx

#endif /* INCLUDE_MMX_TABLE_H_ */
