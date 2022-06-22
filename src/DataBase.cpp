/*
 * DataBase.cpp
 *
 *  Created on: Jun 7, 2022
 *      Author: mad
 */

#include <mmx/DataBase.h>

#include <vnx/vnx.h>


namespace mmx {

template<typename T>
std::string to_number(const T& value, const size_t n_zero)
{
	const auto tmp = std::to_string(value);
	return std::string(n_zero - std::min(n_zero, tmp.length()), '0') + tmp;
}

const std::function<int(const db_val_t&, const db_val_t&)> Table::default_comparator =
	[](const db_val_t& lhs, const db_val_t& rhs) -> int {
		if(lhs.size == rhs.size) {
			return ::memcmp(lhs.data, rhs.data, lhs.size);
		}
		return lhs.size < rhs.size ? -1 : 1;
	};

Table::Table(const std::string& file_path, const std::function<int(const db_val_t&, const db_val_t&)>& comparator)
	:	file_path(file_path), comparator(comparator), mem_index(key_compare_t(this)), mem_block(mem_compare_t(this))
{
	vnx::Directory(file_path).create();
	debug_log.open(file_path + "debug.log", std::ios_base::app);

	index = vnx::read_from_file<TableIndex>(file_path + "index.dat");
	if(!index) {
		index = TableIndex::create();
		debug_log << "Initialized table" << std::endl;
	} else {
		debug_log << "Loaded table at version " << index->version << std::endl;
	}
	// TODO: delete *.tmp files

	for(const auto& name : index->blocks) {
		auto block = read_block(name);
		blocks.push_back(block);
		debug_log << "Loaded block " << block->name << " at level " << block->level << " with " << block->index.size() << " / " << block->total_count
				<< " entries, min_version = " << block->min_version << ", max_version = " << block->max_version << std::endl;
	}
	for(const auto& name : index->delete_files) {
		vnx::File(file_path + name).remove();
	}
	index->delete_files.clear();

	// purge left-over data
	revert(index->version);

	write_log.open(file_path + "write_log.dat", "ab");
	write_log.open("rb+");
	{
		auto& in = write_log.in;
		auto offset = in.get_input_pos();
		const auto min_version = blocks.empty() ? 0 : blocks.back()->max_version + 1;
		while(true) {
			try {
				uint32_t version;
				std::shared_ptr<db_val_t> key;
				std::shared_ptr<db_val_t> value;
				read_entry(write_log.in, version, key, value);
				if(version >= min_version && version <= index->version) {
					insert_entry(version, key, value);
				}
				offset = in.get_input_pos();
			}
			catch(const std::underflow_error& ex) {
				break;
			}
			catch(const std::exception& ex) {
				debug_log << "Reading from write_log.dat failed with: " << ex.what() << std::endl;
			}
		}
		write_log.seek_to(offset);
		debug_log << "Loaded " << mem_block.size() << " entries from write_log.dat" << std::endl;
	}
}

std::shared_ptr<Table::block_t> Table::read_block(const std::string& name) const
{
	auto block = std::make_shared<block_t>();
	block->name = name;

	vnx::File file(file_path + name);
	file.open("rb");

	auto& in = file.in;
	uint16_t format = 0;
	vnx::read(in, format);
	if(format > 0) {
		throw std::runtime_error("invalid block format: " + std::to_string(format));
	}
	vnx::read(in, block->level);
	vnx::read(in, block->min_version);
	vnx::read(in, block->max_version);
	vnx::read(in, block->total_count);
	vnx::read(in, block->index_offset);

	file.seek_to(block->index_offset);
	uint64_t index_size = 0;
	vnx::read(in, index_size);
	block->index.resize(index_size);
	in.read(block->index.data(), block->index.size() * 8);
	return block;
}

void Table::read_entry(vnx::TypeInput& in, uint32_t& version, std::shared_ptr<db_val_t>& key, std::shared_ptr<db_val_t>& value) const
{
	read_key(in, version, key);
	read_value(in, value);
}

void Table::read_key_at(vnx::File& file, int64_t offset, uint32_t& version, std::shared_ptr<db_val_t>& key) const
{
	file.seek_to(offset);
	read_key(file.in, version, key);
}

void Table::read_key(vnx::TypeInput& in, uint32_t& version, std::shared_ptr<db_val_t>& key) const
{
	vnx::read(in, version);

	uint32_t size = 0;
	vnx::read(in, size);
	key = std::make_shared<db_val_t>(size);
	in.read(key->data, key->size);
}

void Table::read_value(vnx::TypeInput& in, std::shared_ptr<db_val_t>& value) const
{
	uint32_t size = 0;
	vnx::read(in, size);
	value = std::make_shared<db_val_t>(size);
	in.read(value->data, value->size);
}

void Table::read_value_at(vnx::File& file, int64_t offset, std::shared_ptr<db_val_t> key, std::shared_ptr<db_val_t>& value) const
{
	file.seek_to(offset + 8 + key->size);
	read_value(file.in, value);
}

void Table::write_entry(vnx::TypeOutput& out, uint32_t version, std::shared_ptr<db_val_t> key, std::shared_ptr<db_val_t> value)
{
	vnx::write(out, version);
	vnx::write(out, key->size);
	out.write(key->data, key->size);
	vnx::write(out, value->size);
	out.write(value->data, value->size);
}

void Table::insert(std::shared_ptr<db_val_t> key, std::shared_ptr<db_val_t> value)
{
	if(!key || !value) {
		throw std::logic_error("!key || !value");
	}
	{
		std::lock_guard lock(mutex);
		if(write_lock) {
			throw std::logic_error("table is write locked");
		}
	}
	write_entry(write_log.out, index->version, key, value);
	insert_entry(index->version, key, value);
}

void Table::insert_entry(uint32_t version, std::shared_ptr<db_val_t> key, std::shared_ptr<db_val_t> value)
{
	auto& entry = mem_block[std::make_pair(key, version)];
	if(entry) {
		mem_block_size -= key->size + entry->size;
	}
	entry = value;
	mem_index[key] = std::make_pair(value, version);
	mem_block_size += key->size + value->size;
}

std::shared_ptr<db_val_t> Table::find(std::shared_ptr<db_val_t> key) const
{
	if(!key) {
		return nullptr;
	}
	auto iter = mem_index.find(key);
	if(iter != mem_index.end()) {
		return iter->second.first;
	}
	for(auto block = blocks.rbegin(); block != blocks.rend(); ++block) {
		if(auto value = find(*block, key)) {
			return value;
		}
	}
	return nullptr;
}

std::shared_ptr<db_val_t> Table::find(std::shared_ptr<block_t> block, std::shared_ptr<db_val_t> key) const
{
	vnx::File file(file_path + block->name);
	file.open("rb");

	auto res = key;
	uint32_t version; bool is_match;
	lower_bound(file, block, version, res, is_match);

	std::shared_ptr<db_val_t> value;
	if(is_match) {
		read_value(file.in, value);
	}
	return value;
}

size_t Table::lower_bound(vnx::File& file, std::shared_ptr<block_t> block, uint32_t& version, std::shared_ptr<db_val_t>& key, bool& is_match) const
{
	if(!key) {
		throw std::logic_error("!key");
	}
	const auto end = block->index.size();
	// find match or successor
	size_t L = 0;
	size_t R = end;
	while(L < R) {
		const auto pos = (L + R) / 2;
		uint32_t version;
		std::shared_ptr<db_val_t> key_i;
		read_key_at(file, block->index[pos], version, key_i);
		if(comparator(*key, *key_i) < 0) {
			R = pos;
		} else {
			L = pos + 1;
		}
	}
	if(R > 0) {
		std::shared_ptr<db_val_t> key_i;
		read_key_at(file, block->index[R - 1], version, key_i);
		if(*key == *key_i) {
			is_match = true;
			return R - 1;
		}
	}
	if(R < end) {
		read_key_at(file, block->index[R], version, key);
	} else {
		version = -1;
		key = nullptr;
	}
	is_match = false;
	return R;
}

void Table::commit(const uint32_t new_version)
{
	if(new_version <= index->version) {
		throw std::logic_error("commit(): new version <= current version");
	}
	write_log.flush();

	if(mem_block_size + mem_block.size() * entry_overhead >= max_block_size) {
		flush();
	}
	index->version = new_version;
	write_index();
}

void Table::revert(const uint32_t new_version)
{
	if(new_version > index->version) {
		throw std::logic_error("revert(): new version > current version");
	}
	if(new_version != index->version) {
		index->version = new_version;
		write_index();
	}
	std::set<std::string> deleted_blocks;
	for(auto& block : blocks) {
		if(block->min_version >= new_version) {
			deleted_blocks.insert(block->name);
		}
		else if(block->max_version >= new_version) {
			auto new_block = std::make_shared<block_t>();
			new_block->name = block->name;
			new_block->level = block->level;
			new_block->min_version = block->min_version;

			vnx::File src(file_path + block->name);
			vnx::File dst(file_path + block->name + ".tmp");
			src.open("rb");
			dst.open("wb");
			auto& in = src.in;
			auto& out = dst.out;
			src.seek_to(block_header_size);
			dst.seek_to(block_header_size);

			int64_t offset = -1;
			std::shared_ptr<db_val_t> prev;
			for(size_t i = 0; i < block->total_count; ++i) {
				uint32_t version;
				std::shared_ptr<db_val_t> key;
				std::shared_ptr<db_val_t> value;
				read_entry(in, version, key, value);
				if(version < new_version) {
					if(prev && *key != *prev) {
						new_block->index.push_back(offset);
					}
					new_block->max_version = std::max(version, new_block->max_version);
					new_block->total_count++;
					offset = out.get_output_pos();
					write_entry(out, version, key, value);
					prev = key;
				}
			}
			new_block->index_offset = out.get_output_pos();
			new_block->index.push_back(offset);
			src.close();

			dst.seek_begin();
			vnx::write(out, uint16_t(0));
			vnx::write(out, new_block->level);
			vnx::write(out, new_block->min_version);
			vnx::write(out, new_block->max_version);
			vnx::write(out, new_block->total_count);
			vnx::write(out, new_block->index_offset);

			dst.seek_to(new_block->index_offset);
			vnx::write(out, uint64_t(new_block->index.size()));
			out.write(new_block->index.data(), new_block->index.size() * 8);
			dst.close();
			std::rename(dst.get_path().c_str(), src.get_path().c_str());

			debug_log << "Rewrote block " << block->name << " with max_version = " << new_block->max_version
					<< ", " << new_block->index.size() << " / " << new_block->total_count << " entries"
					<< ", from " << block->index.size() << " / " << block->total_count << " entries" << std::endl;
			block = new_block;
		}
	}
	if(deleted_blocks.size()) {
		index->blocks.clear();
		for(auto iter = blocks.begin(); iter != blocks.end();) {
			const auto block = *iter;
			if(deleted_blocks.count(block->name)) {
				debug_log << "Deleted block " << block->name << " in revert to version " << new_version << std::endl;
				index->delete_files.push_back(block->name);
				iter = blocks.erase(iter);
			} else {
				index->blocks.push_back(block->name);
				iter++;
			}
		}
		write_index();
	}
	if(index->delete_files.size()) {
		for(const auto& name : index->delete_files) {
			vnx::File(file_path + name).remove();
		}
		index->delete_files.clear();
		write_index();
	}
	for(auto iter = mem_block.begin(); iter != mem_block.end();) {
		const auto& key = iter->first;
		if(key.second >= new_version) {
			mem_block_size -= key.first->size + iter->second->size;
			iter = mem_block.erase(iter);
		} else {
			iter++;
		}
	}
	for(auto iter = mem_index.begin(); iter != mem_index.end();) {
		const auto& entry = iter->second;
		if(entry.second >= new_version) {
			iter = mem_index.erase(iter);
		} else {
			iter++;
		}
	}
}

void Table::flush()
{
	{
		std::lock_guard lock(mutex);
		if(write_lock) {
			throw std::logic_error("table is write locked");
		}
	}
	if(mem_block.empty()) {
		return;
	}
	auto block = std::make_shared<block_t>();
	block->min_version = -1;
	block->name = to_number(index->next_block_id++, 6) + ".dat";
	block->index.reserve(mem_index.size());

	vnx::File file(file_path + block->name);
	file.open("wb+");

	auto& out = file.out;
	file.seek_to(block_header_size);

	int64_t offset = -1;
	std::shared_ptr<db_val_t> prev;
	for(const auto& entry : mem_block) {
		const auto& version = entry.first.second;
		const auto& key = entry.first.first;
		if(prev && *key != *prev) {
			block->index.push_back(offset);
		}
		block->total_count++;
		block->min_version = std::min(version, block->min_version);
		block->max_version = std::max(version, block->max_version);
		offset = out.get_output_pos();
		write_entry(out, version, key, entry.second);
		prev = key;
	}
	block->index.push_back(offset);
	block->index_offset = out.get_output_pos();

	file.seek_begin();
	vnx::write(out, uint16_t(0));
	vnx::write(out, block->level);
	vnx::write(out, block->min_version);
	vnx::write(out, block->max_version);
	vnx::write(out, block->total_count);
	vnx::write(out, block->index_offset);

	file.seek_to(block->index_offset);
	vnx::write(out, uint64_t(block->index.size()));
	out.write(block->index.data(), block->index.size() * 8);
	file.close();

	mem_index.clear();
	mem_block.clear();
	mem_block_size = 0;
	blocks.push_back(block);

	index->blocks.push_back(block->name);
	write_index();

	// clear log after writing index
	write_log.open("wb");

	debug_log << "Flushed block " << block->name << " with " << block->index.size() << " / " << block->total_count
			<< " entries, min_version = " << block->min_version << ", max_version = " << block->max_version << std::endl;
}

void Table::write_index()
{
	vnx::write_to_file(file_path + "index.dat", index);
}


Table::Iterator::Iterator(std::shared_ptr<const Table> table)
	:	table(table), block_map(key_compare_t(table.get()))
{
	std::lock_guard lock(table->mutex);
	table->write_lock++;
}

Table::Iterator::~Iterator()
{
	std::lock_guard lock(table->mutex);
	table->write_lock--;
}

bool Table::Iterator::is_valid() const
{
	return !block_map.empty() && direction;
}

std::map<std::shared_ptr<db_val_t>, Table::Iterator::pointer_t, Table::key_compare_t>::const_iterator
Table::Iterator::current() const
{
	if(!is_valid()) {
		throw std::logic_error("iterator not valid");
	}
	if(direction > 0) {
		return block_map.begin();
	} else {
		return --block_map.end();
	}
}

uint32_t Table::Iterator::version() const
{
	return current()->second.version;
}

std::shared_ptr<db_val_t> Table::Iterator::key() const
{
	return current()->first;
}

std::shared_ptr<db_val_t> Table::Iterator::value() const
{
	return current()->second.value;
}

void Table::Iterator::prev()
{
	if(!is_valid()) {
		throw std::logic_error("iterator not valid");
	}
	if(direction < 0) {
		const auto iter = --block_map.end();
		const auto& entry = iter->second;
		if(const auto& block = entry.block) {
			if(entry.pos > 0) {
				const auto pos = entry.pos - 1;
				uint32_t version;
				std::shared_ptr<db_val_t> key;
				table->read_key_at(*entry.file, block->index[pos], version, key);
				auto& next = block_map[key];
				if(version >= next.version) {
					next.block = block;
					next.file = entry.file;
					next.version = version;
					next.pos = pos;
					table->read_value(entry.file->in, next.value);
				}
			}
		} else {
			if(entry.iter != table->mem_index.begin()) {
				auto iter = entry.iter; iter--;
				auto& next = block_map[iter->first];
				next.iter = iter;
				next.version = iter->second.second;
				next.value = iter->second.first;
			}
		}
		block_map.erase(iter);
	} else {
		seek_prev(key());
	}
}

void Table::Iterator::next()
{
	if(!is_valid()) {
		throw std::logic_error("iterator not valid");
	}
	if(direction > 0) {
		const auto iter = block_map.begin();
		const auto& entry = iter->second;
		if(const auto& block = entry.block) {
			const auto pos = entry.pos + 1;
			if(pos < block->index.size()) {
				uint32_t version;
				std::shared_ptr<db_val_t> key;
				table->read_key_at(*entry.file, block->index[pos], version, key);
				auto& next = block_map[key];
				if(version >= next.version) {
					next.block = block;
					next.file = entry.file;
					next.version = version;
					next.pos = pos;
					table->read_value(entry.file->in, next.value);
				}
			}
		} else {
			auto iter = entry.iter; iter++;
			if(iter != table->mem_index.end()) {
				auto& next = block_map[iter->first];
				next.iter = iter;
				next.version = iter->second.second;
				next.value = iter->second.first;
			}
		}
		block_map.erase(iter);
	} else {
		seek_next(key());
	}
}

void Table::Iterator::seek_begin()
{
	seek(nullptr, 0);
}

void Table::Iterator::seek_last()
{
	seek(nullptr, -1);
}

void Table::Iterator::seek(std::shared_ptr<db_val_t> key)
{
	seek(key, 0);
}

void Table::Iterator::seek(std::shared_ptr<db_val_t> key, const int mode)
{
	block_map.clear();

	for(const auto& block : table->blocks)
	{
		const auto end = block->index.size();
		auto file = std::make_shared<vnx::File>(table->file_path + block->name);
		file->open("rb");

		bool is_match = false;
		uint32_t version = -1;
		size_t pos = 0;
		auto res = key;
		if(res) {
			pos = table->lower_bound(*file, block, version, res, is_match);
		}
		else if(mode == 0) {
			if(pos < end) {
				table->read_key_at(*file, block->index[pos], version, res);
			} else {
				continue;
			}
		}
		else if(mode < 0) {
			pos = end;
		}
		if(mode < 0) {
			if(pos == 0) {
				continue;
			}
			table->read_key_at(*file, block->index[--pos], version, res);
		}
		else if(mode > 0) {
			if(pos + 1 >= end) {
				continue;
			}
			table->read_key_at(*file, block->index[++pos], version, res);
		}
		if(!res) {
			continue;
		}
		auto& entry = block_map[res];
		if(version >= entry.version) {
			entry.block = block;
			entry.file = file;
			entry.version = version;
			entry.pos = pos;
			table->read_value(file->in, entry.value);
		}
	}
	const auto& mem_index = table->mem_index;
	if(!mem_index.empty()) {
		auto iter = mem_index.begin();
		if(key) {
			iter = mem_index.lower_bound(key);
		} else if(mode < 0) {
			iter = mem_index.end();
		}
		if(mode < 0) {
			if(iter != mem_index.begin()) {
				iter--;
			} else {
				iter = mem_index.end();
			}
		} else if(mode > 0) {
			if(iter != mem_index.end()) {
				iter++;
			}
		}
		if(iter != mem_index.end()) {
			auto& entry = block_map[iter->first];
			entry.iter = iter;
			entry.version = iter->second.second;
			entry.value = iter->second.first;
		}
	}
	direction = mode >= 0 ? 1 : -1;
}

void Table::Iterator::seek_next(std::shared_ptr<db_val_t> key)
{
	seek(key, 1);
}

void Table::Iterator::seek_prev(std::shared_ptr<db_val_t> key)
{
	seek(key, -1);
}

void Table::Iterator::seek_for_prev(std::shared_ptr<db_val_t> key_)
{
	seek(key_);
	if(is_valid() && *key() != *key_) {
		prev();
	}
}


void DataBase::add(std::shared_ptr<Table> table)
{
	tables.push_back(table);
}

void DataBase::commit(const uint32_t new_version)
{
	for(const auto& table : tables) {
		table->commit(new_version);
	}
}

void DataBase::revert(const uint32_t new_version)
{
	for(const auto& table : tables) {
		table->revert(new_version);
	}
}

uint32_t DataBase::recover()
{
	uint32_t min_version = -1;
	for(const auto& table : tables) {
		min_version = std::min(table->current_version(), min_version);
	}
	revert(min_version);
	return min_version;
}


} // mmx
