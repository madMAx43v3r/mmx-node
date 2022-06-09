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
	:	file_path(file_path), comparator(comparator), mem_block(mem_compare_t(this))
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
		debug_log << "Loaded block " << block->name << " at level " << block->level << " with " << block->index.size()
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
	mem_block_size += key->size + value->size;
}

std::shared_ptr<db_val_t> Table::find(std::shared_ptr<db_val_t> key) const
{
	if(!key) {
		return nullptr;
	}
	std::shared_ptr<db_val_t> value;
	for(auto iter = mem_block.lower_bound(std::make_pair(key, 0)); iter != mem_block.end() && *iter->first.first == *key; ++iter) {
		value = iter->second;
	}
	if(value) {
		return value;
	}
	for(auto iter = blocks.rbegin(); iter != blocks.rend(); ++iter) {
		value = find(*iter, key);
		if(value) {
			break;
		}
	}
	return value;
}

std::shared_ptr<db_val_t> Table::find(std::shared_ptr<block_t> block, std::shared_ptr<db_val_t> key) const
{
	vnx::File file(file_path + block->name);
	file.open("rb");

	const auto pos = find(file, block, key);
	std::shared_ptr<db_val_t> value;
	if(pos < block->index.size()) {
		uint32_t version;
		std::shared_ptr<db_val_t> res;
		read_key_at(file, block->index[pos], version, res);
		if(!key || *res == *key) {
			read_value(file.in, value);
		}
	}
	return value;
}

size_t Table::find(vnx::File& file, std::shared_ptr<block_t> block, std::shared_ptr<db_val_t> key) const
{
	if(block->index.empty()) {
		return 0;
	}
	const auto end = block->index.size();

	if(!key) {
		// advance version of first entry
		size_t pos = end;
		for(size_t i = 0; i < end; ++i) {
			uint32_t version;
			std::shared_ptr<db_val_t> key_i;
			read_key_at(file, block->index[i], version, key_i);

			if(key && *key_i != *key) {
				break;
			}
			pos = i;
			key = key_i;
		}
		return pos;
	}
	// find right most entry or successor
	size_t L = 0;
	size_t R = end - 1;
	while(L != R) {
		const auto pos = (L + R + 1) / 2;
		uint32_t version;
		std::shared_ptr<db_val_t> key_i;
		read_key_at(file, block->index[pos], version, key_i);
		if(comparator(*key, *key_i) < 0) {
			R = pos - 1;
		} else {
			L = pos;
		}
	}
	// advance version if successor
	auto pos = L;
	bool advance = false;
	while(pos < end) {
		uint32_t version;
		std::shared_ptr<db_val_t> key_i;
		read_key_at(file, block->index[pos], version, key_i);
		const auto res = comparator(*key, *key_i);
		if(res == 0) {
			if(advance) {
				if(pos + 1 < end) {
					pos++;
				} else {
					break;
				}
			} else {
				break;
			}
		} else if(res < 0) {
			if(advance) {
				pos--;
				break;
			} else {
				if(pos + 1 < end) {
					pos++;
				} else {
					break;
				}
				key = key_i;
				advance = true;
			}
		} else {
			pos++;
		}
	}
	return pos;
}

void Table::commit(const uint32_t new_version)
{
	if(new_version <= index->version) {
		throw std::logic_error("commit(): new version <= current version");
	}
	write_log.flush();

	if(mem_block.size() * entry_overhead + mem_block_size >= max_block_size) {
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
	for(auto iter = blocks.begin(); iter != blocks.end();) {
		const auto& block = *iter;
		if(block->min_version >= new_version) {
			// delete block
			index->delete_files.push_back(block->name);
			debug_log << "Deleted block " << block->name << " in revert to version " << new_version << std::endl;
			iter = blocks.erase(iter);
		} else if(block->max_version >= new_version) {
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
			src.seek_to(block_header_size + block->index.size() * 8);
			dst.seek_to(block_header_size + block->index.size() * 8);
			for(size_t i = 0; i < block->index.size(); ++i) {
				uint32_t version;
				std::shared_ptr<db_val_t> key;
				std::shared_ptr<db_val_t> value;
				read_entry(in, version, key, value);
				if(version < new_version) {
					new_block->max_version = std::max(version, new_block->max_version);
					new_block->index.push_back(out.get_output_pos());
					write_entry(out, version, key, value);
				}
			}
			src.close();
			dst.seek_begin();
			vnx::write(out, uint16_t(0));
			vnx::write(out, new_block->level);
			vnx::write(out, new_block->min_version);
			vnx::write(out, new_block->max_version);
			vnx::write(out, uint64_t(new_block->index.size()));
			out.write(new_block->index.data(), new_block->index.size() * 8);
			dst.close();
			std::rename(dst.get_path().c_str(), src.get_path().c_str());

			debug_log << "Rewrote block " << block->name << " with max_version = " << new_block->max_version
					<< ", " << new_block->index.size() << " / " << block->index.size() << " entries" << std::endl;
			*iter = new_block;
			iter++;
		} else {
			iter++;
		}
	}
	index->blocks.clear();
	for(const auto& block : blocks) {
		index->blocks.push_back(block->name);
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
	write_index();

	if(index->delete_files.size()) {
		for(const auto& name : index->delete_files) {
			vnx::File(file_path + name).remove();
		}
		index->delete_files.clear();
		write_index();
	}
}

void Table::flush()
{
	if(mem_block.empty()) {
		return;
	}
	auto block = std::make_shared<block_t>();
	block->min_version = -1;
	block->name = to_number(index->next_block_id++, 6) + ".dat";
	block->index.reserve(mem_block.size());

	vnx::File file(file_path + block->name);
	file.open("wb+");

	auto& out = file.out;
	file.seek_to(block_header_size + mem_block.size() * 8);
	for(const auto& entry : mem_block) {
		const auto& version = entry.first.second;
		block->min_version = std::min(version, block->min_version);
		block->max_version = std::max(version, block->max_version);
		block->index.push_back(out.get_output_pos());
		write_entry(out, version, entry.first.first, entry.second);
	}
	file.seek_begin();
	vnx::write(out, uint16_t(0));
	vnx::write(out, block->level);
	vnx::write(out, block->min_version);
	vnx::write(out, block->max_version);
	vnx::write(out, uint64_t(block->index.size()));
	out.write(block->index.data(), block->index.size() * 8);
	file.close();

	mem_block.clear();
	mem_block_size = 0;
	blocks.push_back(block);

	index->blocks.push_back(block->name);
	write_index();

	// clear log after writing index
	write_log.open("wb");

	debug_log << "Flushed block " << block->name << " with " << block->index.size()
			<< " entries, min_version = " << block->min_version << ", max_version = " << block->max_version << std::endl;
}

void Table::write_index()
{
	vnx::write_to_file(file_path + "index.dat", index);
}


Table::Iterator::Iterator(std::shared_ptr<const Table> table)
	:	table(table), block_map(mem_compare_t(table.get()))
{
	for(const auto& block : table->blocks) {
		if(block->index.empty()) {
			continue;
		}
		auto file = std::make_shared<vnx::File>(table->file_path + block->name);
		file->open("rb");

		const auto pos = table->find(*file, block, nullptr);
		if(pos < block->index.size()) {
			uint32_t version;
			std::shared_ptr<db_val_t> key;
			table->read_key_at(*file, block->index[pos], version, key);
			auto& entry = block_map[std::make_pair(key, version)];
			entry.block = block;
			entry.file = file;
			entry.pos = pos;
		}
	}
	if(!table->mem_block.empty()) {
		const auto iter = table->mem_block.begin();
		block_map[iter->first].iter = iter;
	}
}

bool Table::Iterator::is_valid() const
{
	return !block_map.empty();
}

uint32_t Table::Iterator::version() const
{
	if(!is_valid()) {
		throw std::logic_error("iterator not valid");
	}
	return block_map.begin()->first.second;
}

std::shared_ptr<db_val_t> Table::Iterator::key() const
{
	if(is_valid()) {
		return block_map.begin()->first.first;
	}
	return nullptr;
}

std::shared_ptr<db_val_t> Table::Iterator::value() const
{
	if(!is_valid()) {
		return nullptr;
	}
	std::shared_ptr<db_val_t> value;
	const auto iter = block_map.begin();
	const auto& entry = iter->second;
	if(entry.block) {
		const auto& key = iter->first.first;
		entry.file->seek_to(entry.block->index[entry.pos] + 8 + key->size);
		table->read_value(entry.file->in, value);
	} else {
		value = entry.iter->second;
	}
	return value;
}

void Table::Iterator::prev()
{
}

void Table::Iterator::next()
{
}

void Table::Iterator::seek(std::shared_ptr<db_val_t> key)
{
}



} // mmx
