/*
 * multi_table.h
 *
 *  Created on: Jun 21, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_MULTI_TABLE_H_
#define INCLUDE_MMX_MULTI_TABLE_H_

#include <mmx/table.h>


namespace mmx {

template<typename K, typename V, typename I = uint32_t>
class multi_table : protected table<std::pair<K, I>, V> {
private:
	typedef table<std::pair<K, I>, V> super_t;

protected:
	using super_t::db;
	using super_t::read;
	using super_t::write;

public:
	multi_table() = default;

	multi_table(const std::string& file_path)
		:	super_t(file_path)
	{
	}

	using super_t::open;
	using super_t::close;

	void insert(const K& key, const V& value)
	{
		insert_many(key, {value});
	}

	void insert_many(const K& key, const std::vector<V>& values)
	{
		std::pair<K, I> key_(key, std::numeric_limits<I>::max());
		{
			Table::Iterator iter(db);
			iter.seek_prev(write(key_));
			key_.second = 0;

			if(iter.is_valid()) {
				std::pair<K, I> found;
				read(iter.key(), found);
				if(found.first == key) {
					key_.second = found.second + 1;
				}
			}
		}
		for(const auto& value : values) {
			if(key_.second == std::numeric_limits<I>::max()) {
				throw std::runtime_error("key space overflow (" + super_t::get_path() + ")");
			}
			super_t::insert(key_, value);
			key_.second++;
		}
	}

	size_t count(const K& key) const
	{
		std::vector<V> values;
		return find(key, values);
	}

	size_t find(const K& key, std::vector<V>& values, const bool greater_equal = false) const
	{
		values.clear();
		std::pair<K, I> key_(key, 0);

		Table::Iterator iter(db);
		iter.seek(write(key_));
		while(iter.is_valid()) {
			read(iter.key(), key_);
			if(!greater_equal && !(key_.first == key)) {
				break;
			}
			try {
				V value;
				read(iter.value(), value, super_t::value_type, super_t::value_code);
				values.push_back(std::move(value));
			} catch(...) {
				// ignore
			}
			iter.next();
		}
		return values.size();
	}

	size_t find_last(const K& key, std::vector<V>& values, const size_t limit) const
	{
		values.clear();
		std::pair<K, I> key_(key, std::numeric_limits<I>::max());

		Table::Iterator iter(db);
		iter.seek_prev(write(key_));
		while(iter.is_valid() && values.size() < limit)
		{
			read(iter.key(), key_);
			if(!(key_.first == key)) {
				break;
			}
			try {
				V value;
				read(iter.value(), value, super_t::value_type, super_t::value_code);
				values.push_back(std::move(value));
			} catch(...) {
				// ignore
			}
			iter.prev();
		}
		return values.size();
	}

	size_t find_range(const K& begin, const K& end, std::vector<V>& values) const
	{
		values.clear();
		std::pair<K, I> key_(begin, 0);

		Table::Iterator iter(db);
		iter.seek(write(key_));
		while(iter.is_valid()) {
			read(iter.key(), key_);
			if(!(key_.first < end)) {
				break;
			}
			try {
				V value;
				read(iter.value(), value, super_t::value_type, super_t::value_code);
				values.push_back(std::move(value));
			} catch(...) {
				// ignore
			}
			iter.next();
		}
		return values.size();
	}

	size_t find_range(const K& begin, const K& end, std::vector<std::pair<K, V>>& result) const
	{
		result.clear();
		std::pair<K, I> key_(begin, 0);

		Table::Iterator iter(db);
		iter.seek(write(key_));
		while(iter.is_valid()) {
			read(iter.key(), key_);
			if(!(key_.first < end)) {
				break;
			}
			try {
				std::pair<K, V> tmp;
				tmp.first = key_.first;
				read(iter.value(), tmp.second, super_t::value_type, super_t::value_code);
				result.push_back(std::move(tmp));
			} catch(...) {
				// ignore
			}
			iter.next();
		}
		return result.size();
	}

	void scan(const std::function<void(const K&, const V&)>& callback) const
	{
		super_t::scan([callback](const std::pair<K, I>& key, const V& value) {
			callback(key.first, value);
		});
	}

	using super_t::commit;
	using super_t::revert;
	using super_t::flush;
	using super_t::iterator;
	using super_t::get_impl;
	using super_t::get_path;

};


template<typename K, typename V, typename I = uint32_t>
class uint_multi_table : public multi_table<K, V, I> {
public:
	uint_multi_table() : multi_table<K, V, I>() {}
	uint_multi_table(const std::string& file_path) : multi_table<K, V, I>(file_path) {}
protected:
	// TODO: use a to_big_endian() function
	void read(std::shared_ptr<const db_val_t> entry, std::pair<K, I>& key) const override {
		if(entry->size != sizeof(K) + sizeof(I)) {
			throw std::logic_error("key size mismatch: " + std::to_string(entry->size) + " (" + multi_table<K, V, I>::get_path() + ")");
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

template<typename K, typename V, typename I = uint32_t>
class hash_multi_table : public multi_table<K, V, I> {
public:
	hash_multi_table() : multi_table<K, V, I>() {}
	hash_multi_table(const std::string& file_path) : multi_table<K, V, I>(file_path) {}
protected:
	void read(std::shared_ptr<const db_val_t> entry, std::pair<K, I>& key) const override {
		if(entry->size != key.first.size() + sizeof(I)) {
			throw std::logic_error("key size mismatch: " + std::to_string(entry->size) + " (" + multi_table<K, V, I>::get_path() + ")");
		}
		::memcpy(key.first.data(), entry->data, key.first.size());
		key.second = vnx::flip_bytes(*((const I*)(entry->data + key.first.size())));
	}
	std::shared_ptr<db_val_t> write(const std::pair<K, I>& key) const override {
		auto out = std::make_shared<mmx::db_val_t>(key.first.size() + sizeof(I));
		::memcpy(out->data, key.first.data(), key.first.size());
		vnx::write_value(out->data + key.first.size(), vnx::flip_bytes(key.second));
		return out;
	}
};

template<typename K, typename H, typename V, typename I = uint32_t>
class hash_uint_multi_table : public multi_table<std::pair<K, H>, V, I> {
public:
	hash_uint_multi_table() : multi_table<std::pair<K, H>, V, I>() {}
	hash_uint_multi_table(const std::string& file_path) : multi_table<std::pair<K, H>, V, I>(file_path) {}
protected:
	void read(std::shared_ptr<const db_val_t> entry, std::pair<std::pair<K, H>, I>& key) const override {
		if(entry->size != key.first.first.size() + sizeof(H) + sizeof(I)) {
			throw std::logic_error("key size mismatch: " + std::to_string(entry->size) + " (" + multi_table<std::pair<K, H>, V, I>::get_path() + ")");
		}
		auto* src = entry->data;
		::memcpy(key.first.first.data(), src, key.first.first.size()); src += key.first.first.size();
		key.first.second = vnx::flip_bytes(*((const H*)src)); src += sizeof(H);
		key.second = vnx::flip_bytes(*((const I*)src));
	}
	std::shared_ptr<db_val_t> write(const std::pair<std::pair<K, H>, I>& key) const override {
		auto out = std::make_shared<mmx::db_val_t>(key.first.first.size() + sizeof(H) + sizeof(I));
		auto* dst = out->data;
		::memcpy(dst, key.first.first.data(), key.first.first.size()); dst += key.first.first.size();
		vnx::write_value(dst, vnx::flip_bytes(key.first.second)); dst += sizeof(H);
		vnx::write_value(dst, vnx::flip_bytes(key.second));
		return out;
	}
};


} // mmx

#endif /* INCLUDE_MMX_MULTI_TABLE_H_ */
