/*
 * DataBase.h
 *
 *  Created on: Jun 6, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_DB_DATABASE_H_
#define INCLUDE_MMX_DB_DATABASE_H_

#include <mmx/TableIndex.hxx>

#include <vnx/File.h>
#include <fstream>
#include <mutex>

#ifdef _MSC_VER
#include <mmx_db_export.h>
#else
#define MMX_DB_EXPORT
#endif

namespace mmx {

struct db_val_t {
	uint8_t* data = nullptr;
	uint32_t size = 0;

	db_val_t() = default;
	db_val_t(const uint32_t size) : size(size) {
		if(size) {
			data = (uint8_t*)::malloc(size);
		}
	}
	db_val_t(const void* data, const uint32_t size) : db_val_t(size) {
		::memcpy(this->data, data, size);
	}
	db_val_t(void* data, const uint32_t size, bool copy = true) : size(size) {
		if(copy) {
			this->data = (uint8_t*)::malloc(size);
			::memcpy(this->data, data, size);
		} else {
			this->data = (uint8_t*)data;
		}
	}
	db_val_t(const db_val_t&) = delete;

	~db_val_t() {
		::free(data);
		data = nullptr;
		size = 0;
	}

	bool operator==(const db_val_t& other) const {
		if(other.size == size) {
			return ::memcmp(other.data, data, size) == 0;
		}
		return false;
	}
	bool operator!=(const db_val_t& other) const {
		if(other.size == size) {
			return ::memcmp(other.data, data, size) != 0;
		}
		return true;
	}
};

class Table {
protected:
	struct block_t {
		uint32_t level = 0;
		uint32_t min_version = 0;
		uint32_t max_version = 0;
		uint64_t total_count = 0;
		uint64_t index_offset = 0;
		std::string name;
		std::vector<int64_t> index;
	};

	struct key_compare_t {
		const Table* table = nullptr;
		key_compare_t(const Table* table) : table(table) {}

		bool operator()(const std::shared_ptr<db_val_t>& lhs, const std::shared_ptr<db_val_t>& rhs) const {
			return table->comparator(*lhs, *rhs) < 0;
		}
	};

	struct mem_compare_t {
		const Table* table = nullptr;
		mem_compare_t(const Table* table) : table(table) {}

		bool operator()(const std::pair<std::shared_ptr<db_val_t>, uint32_t>& lhs, const std::pair<std::shared_ptr<db_val_t>, uint32_t>& rhs) const {
			const auto res = table->comparator(*lhs.first, *rhs.first);
			if(res == 0) {
				return lhs.second > rhs.second;
			}
			return res < 0;
		}
	};

public:
	int level_factor = 4;
	size_t max_block_size = 16 * 1024 * 1024;

	const std::string file_path;
	const std::function<int(const db_val_t&, const db_val_t&)> comparator;

	static constexpr uint32_t entry_overhead = 20;
	static constexpr uint32_t block_header_size = 30;

	MMX_DB_EXPORT
	static const std::function<int(const db_val_t&, const db_val_t&)> default_comparator;

	Table(const std::string& file_path, const std::function<int(const db_val_t&, const db_val_t&)>& comparator = default_comparator);

	void insert(std::shared_ptr<db_val_t> key, std::shared_ptr<db_val_t> value);

	std::shared_ptr<db_val_t> find(std::shared_ptr<db_val_t> key) const;

	void commit(const uint32_t new_version);

	void revert(const uint32_t new_version);

	void flush();

	uint32_t current_version() const {
		return index->version;
	}

	class Iterator {
	public:
		Iterator() = default;
		Iterator(std::shared_ptr<const Table> table);
		~Iterator();

		bool is_valid() const;
		uint32_t version() const;
		std::shared_ptr<db_val_t> key() const;
		std::shared_ptr<db_val_t> value() const;

		void prev();
		void next();
		void seek_begin();
		void seek_last();
		void seek(std::shared_ptr<db_val_t> key);
		void seek_next(std::shared_ptr<db_val_t> key);
		void seek_prev(std::shared_ptr<db_val_t> key);
		void seek_for_prev(std::shared_ptr<db_val_t> key);
	private:
		struct pointer_t {
			size_t pos = -1;
			std::shared_ptr<block_t> block;
			std::shared_ptr<db_val_t> value;
			std::shared_ptr<vnx::File> file;
			std::map<std::shared_ptr<db_val_t>, std::pair<std::shared_ptr<db_val_t>, uint32_t>, key_compare_t>::const_iterator iter;
		};

		struct compare_t {
			const Iterator* iter = nullptr;
			compare_t(const Iterator* iter) : iter(iter) {}
			bool operator()(const std::pair<std::shared_ptr<db_val_t>, uint32_t>& lhs, const std::pair<std::shared_ptr<db_val_t>, uint32_t>& rhs) const;
		};

		void seek(std::shared_ptr<db_val_t> key, const int mode);
		std::map<std::pair<std::shared_ptr<db_val_t>, uint32_t>, pointer_t, key_compare_t>::const_iterator current() const;

		int direction = 0;
		std::shared_ptr<const Table> table;
		std::map<std::pair<std::shared_ptr<db_val_t>, uint32_t>, pointer_t, compare_t> block_map;
	};

private:
	void read_entry(vnx::TypeInput& in, uint32_t& version, std::shared_ptr<db_val_t>& key, std::shared_ptr<db_val_t>& value) const;

	void read_key_at(vnx::File& file, int64_t offset, uint32_t& version, std::shared_ptr<db_val_t>& key) const;

	void read_key(vnx::TypeInput& in, uint32_t& version, std::shared_ptr<db_val_t>& key) const;

	void read_value(vnx::TypeInput& in, std::shared_ptr<db_val_t>& value) const;

	void read_value_at(vnx::File& file, int64_t offset, std::shared_ptr<db_val_t> key, std::shared_ptr<db_val_t>& value) const;

	void write_entry(vnx::TypeOutput& out, uint32_t version, std::shared_ptr<db_val_t> key, std::shared_ptr<db_val_t> value);

	void insert_entry(uint32_t version, std::shared_ptr<db_val_t> key, std::shared_ptr<db_val_t> value);

	std::shared_ptr<block_t> read_block(const std::string& name) const;

	std::shared_ptr<db_val_t> find(std::shared_ptr<block_t> block, std::shared_ptr<db_val_t> key) const;

	size_t lower_bound(vnx::File& file, std::shared_ptr<block_t> block, uint32_t& version, std::shared_ptr<db_val_t>& key, bool& is_match) const;

	void write_index();

	std::ofstream debug_log;
	std::shared_ptr<TableIndex> index;
	vnx::File write_log;
	std::list<std::shared_ptr<block_t>> blocks;

	size_t mem_block_size = 0;
	std::map<std::shared_ptr<db_val_t>, std::pair<std::shared_ptr<db_val_t>, uint32_t>, key_compare_t> mem_index;
	std::map<std::pair<std::shared_ptr<db_val_t>, uint32_t>, std::shared_ptr<db_val_t>, mem_compare_t> mem_block;

	mutable std::mutex mutex;
	mutable int64_t write_lock = 0;

};

class DataBase {
public:
	void add(std::shared_ptr<Table> table);

	void commit(const uint32_t new_version);

	void revert(const uint32_t new_version);

	uint32_t min_version() const;

	uint32_t recover();

private:
	std::vector<std::shared_ptr<Table>> tables;

};


} // mmx

#endif /* INCLUDE_MMX_DB_DATABASE_H_ */
