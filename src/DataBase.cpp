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

void read_key(vnx::TypeInput& in, uint32_t& version, std::shared_ptr<db_val_t>& key)
{
	vnx::read(in, version);

	uint32_t size = 0;
	vnx::read(in, size);
	key = std::make_shared<db_val_t>(size);
	in.read(key->data, key->size);
}

void read_key_at(const vnx::File& file, const int64_t offset, uint32_t& version, std::shared_ptr<db_val_t>& key)
{
	vnx::FileSectionInputStream stream(file.get_handle(), offset, -1, 256);
	vnx::TypeInput in(&stream);
	read_key(in, version, key);
}

void read_value(vnx::TypeInput& in, std::shared_ptr<db_val_t>& value)
{
	uint32_t size = 0;
	vnx::read(in, size);
	value = std::make_shared<db_val_t>(size);
	in.read(value->data, value->size);
}

void read_value_at(const vnx::File& file, const int64_t offset, std::shared_ptr<db_val_t> key, std::shared_ptr<db_val_t>& value)
{
	vnx::FileSectionInputStream stream(file.get_handle(), offset + 8 + key->size, -1, 1024);
	vnx::TypeInput in(&stream);
	read_value(in, value);
}

void read_entry(vnx::TypeInput& in, uint32_t& version, std::shared_ptr<db_val_t>& key, std::shared_ptr<db_val_t>& value)
{
	read_key(in, version, key);
	read_value(in, value);
}

void read_entry_at(const vnx::File& file, const int64_t offset, uint32_t& version, std::shared_ptr<db_val_t>& key, std::shared_ptr<db_val_t>& value)
{
	vnx::FileSectionInputStream stream(file.get_handle(), offset, -1, 1024);
	vnx::TypeInput in(&stream);
	read_key(in, version, key);
	read_value(in, value);
}

void write_entry(vnx::TypeOutput& out, uint32_t version, std::shared_ptr<db_val_t> key, std::shared_ptr<db_val_t> value)
{
	vnx::write(out, version);
	vnx::write(out, key->size);
	out.write(key->data, key->size);
	vnx::write(out, value->size);
	out.write(value->data, value->size);
}

const std::function<int(const db_val_t&, const db_val_t&)> Table::default_comparator =
	[](const db_val_t& lhs, const db_val_t& rhs) -> int {
		if(lhs.size == rhs.size) {
			return ::memcmp(lhs.data, rhs.data, lhs.size);
		}
		return lhs.size < rhs.size ? -1 : 1;
	};

const Table::options_t Table::default_options;

Table::Table(const std::string& root_path, const options_t& options)
	:	options(options), root_path(root_path), mem_index(key_compare_t(this)), mem_block(mem_compare_t(this))
{
	const auto time_begin = vnx::get_wall_time_millis();
	vnx::Directory root(root_path);
	root.create();

	debug_log.open(root_path + "/debug.log", std::ios_base::app);

	std::map<uint64_t, std::string> block_map;
	for(const auto& file : root.files()) {
		if(file->get_extension() == ".tmp") {
			const auto name = file->get_name(false);
			const vnx::File new_file(root_path + '/' + name);
			if(!new_file.exists() && new_file.get_extension() == ".dat") {
				debug_log << "Renamed " << name << ".tmp to " << name << std::endl;
				file->rename(new_file.get_path());
			}
		}
		if(file->get_extension() == ".dat") {
			vnx::Variant name;
			vnx::from_string_value(file->get_name(false), name);
			if(name.is_ulong()) {
				const auto index = name.to<uint64_t>();
				block_map[index] = file->get_name();
				next_block_id = index + 1;
			}
		}
	}
	{
		std::vector<std::shared_ptr<block_t>> block_list;
		for(const auto& entry : block_map) {
			const auto block = read_block(entry.second);
			block_list.push_back(block);
			debug_log << "Loaded " << block->name << " at level " << block->level << " with " << block->index.size() << " / " << block->total_count
					<< " entries, min_version = " << block->min_version << ", max_version = " << block->max_version << std::endl;
		}
		std::sort(block_list.begin(), block_list.end(),
			[](const std::shared_ptr<block_t>& lhs, const std::shared_ptr<block_t>& rhs) -> bool {
				if(lhs->min_version == rhs->min_version) {
					return lhs->level > rhs->level;
				}
				return lhs->min_version < rhs->min_version;
			});

		std::shared_ptr<block_t> prev;
		for(const auto& block : block_list) {
			if(!prev || block->min_version > prev->max_version) {
				curr_version = block->max_version + 1;
				blocks.push_back(block);
			} else {
				block->file.remove();
				debug_log << "Deleted " << block->name << std::endl;
			}
			prev = block;
		}
	}
	write_log.open(root_path + "/write_log.dat", "ab");
	write_log.open("rb+");
	{
		auto& in = write_log.in;
		auto offset = in.get_input_pos();
		const auto min_version = curr_version;
		std::multimap<uint32_t, std::pair<std::shared_ptr<db_val_t>, std::shared_ptr<db_val_t>>> entries;
		while(true) {
			try {
				uint32_t version;
				std::shared_ptr<db_val_t> key;
				std::shared_ptr<db_val_t> value;
				read_entry(write_log.in, version, key, value);
				if(version == uint32_t(-1)) {
					const auto cmd = key->to_string();
					if(cmd == "commit") {
						curr_version = value->to<uint32_t>();
					}
					else if(cmd == "revert") {
						const auto height = value->to<uint32_t>();
						curr_version = height;
						entries.erase(entries.lower_bound(height), entries.end());
					}
				} else if(version >= min_version) {
					entries.emplace(version, std::make_pair(key, value));
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

		for(const auto& entry : entries) {
			insert_entry(entry.first, entry.second.first, entry.second.second);
		}
		debug_log << "Loaded " << mem_index.size() << " / " << mem_block.size() << " entries from write_log.dat" << std::endl;
	}
	debug_log << "Loaded table at version " << curr_version << ", took " << (vnx::get_wall_time_millis() - time_begin) / 1e3 << " sec" << std::endl;

	revert(curr_version);

	check_rewrite();
}

std::shared_ptr<Table::block_t> Table::read_block(const std::string& name) const
{
	auto block = std::make_shared<block_t>();
	block->name = name;
	block->file.open(root_path + '/' + name, "rb");

	auto& in = block->file.in;
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

	block->file.seek_to(block->index_offset);
	uint64_t index_size = 0;
	vnx::read(in, index_size);
	block->index.resize(index_size);
	in.read(block->index.data(), block->index.size() * 8);
	return block;
}

void Table::insert(std::shared_ptr<db_val_t> key, std::shared_ptr<db_val_t> value)
{
	if(!key || !value) {
		throw std::logic_error("!key || !value");
	}
	std::lock_guard lock(mutex);
	if(write_lock) {
		throw std::logic_error("table is write locked");
	}
	write_entry(write_log.out, curr_version, key, value);
	insert_entry(curr_version, key, value);
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
	auto res = key;
	uint32_t version; bool is_match;
	const auto pos = lower_bound(block, version, res, is_match);

	std::shared_ptr<db_val_t> value;
	if(is_match) {
		read_value_at(block->file, block->index[pos], key, value);
	}
	return value;
}

size_t Table::lower_bound(std::shared_ptr<block_t> block, uint32_t& version, std::shared_ptr<db_val_t>& key, bool& is_match) const
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
		read_key_at(block->file, block->index[pos], version, key_i);
		if(options.comparator(*key, *key_i) < 0) {
			R = pos;
		} else {
			L = pos + 1;
		}
	}
	if(R > 0) {
		std::shared_ptr<db_val_t> key_i;
		read_key_at(block->file, block->index[R - 1], version, key_i);
		if(*key == *key_i) {
			is_match = true;
			return R - 1;
		}
	}
	if(R < end) {
		read_key_at(block->file, block->index[R], version, key);
	} else {
		version = -1;
		key = nullptr;
	}
	is_match = false;
	return R;
}

void Table::commit(const uint32_t new_version)
{
	if(new_version == uint32_t(-1)) {
		throw std::logic_error("invalid version");
	}
	if(new_version <= curr_version) {
		throw std::logic_error("commit(): new version <= current version");
	}
	{
		const std::string cmd = "commit";
		write_entry(write_log.out, -1,
				std::make_shared<db_val_t>(cmd.c_str(), cmd.size()),
				std::make_shared<db_val_t>(&new_version, sizeof(new_version)));
	}
	write_log.flush();

	if(mem_block_size + mem_block.size() * entry_overhead >= options.max_block_size) {
		flush();
	}
	curr_version = new_version;
}

void Table::revert(const uint32_t new_version)
{
	std::lock_guard lock(mutex);
	if(write_lock) {
		throw std::logic_error("table is write locked");
	}
	if(new_version > curr_version) {
		throw std::logic_error("revert(): new version > current version");
	}
	{
		const std::string cmd = "revert";
		write_entry(write_log.out, -1,
				std::make_shared<db_val_t>(cmd.c_str(), cmd.size()),
				std::make_shared<db_val_t>(&new_version, sizeof(new_version)));
	}
	write_log.flush();

	std::set<std::string> deleted_blocks;
	for(auto iter = blocks.begin(); iter != blocks.end();) {
		auto& block = *iter;
		if(block->min_version >= new_version) {
			block->file.remove();
			debug_log << "Deleted " << block->name << std::endl;
			iter = blocks.erase(iter);
			continue;
		}
		if(block->max_version >= new_version) {
			const auto time_begin = vnx::get_wall_time_millis();

			auto new_block = create_block(block->level, block->name + ".tmp");
			new_block->min_version = block->min_version;

			auto& src = block->file;
			auto& dst = new_block->file;
			auto& in = src.in;
			auto& out = dst.out;
			src.seek_to(block_header_size);
			dst.seek_to(block_header_size);

			std::shared_ptr<db_val_t> prev;
			for(size_t i = 0; i < block->total_count; ++i) {
				uint32_t version;
				std::shared_ptr<db_val_t> key;
				std::shared_ptr<db_val_t> value;
				read_entry(in, version, key, value);
				if(version < new_version) {
					if(!prev || *key != *prev) {
						new_block->index.push_back(out.get_output_pos());
					}
					new_block->max_version = std::max(version, new_block->max_version);
					new_block->total_count++;
					write_entry(out, version, key, value);
					prev = key;
				}
			}
			new_block->index_offset = out.get_output_pos();

			finish_block(new_block);
			block->file.close();
			rename(new_block, block->name);

			debug_log << "Rewrote " << block->name << " with max_version = " << new_block->max_version
					<< ", " << new_block->index.size() << " / " << new_block->total_count << " entries"
					<< ", from " << block->index.size() << " / " << block->total_count << " entries"
					<< ", took " << (vnx::get_wall_time_millis() - time_begin) / 1e3 << " sec" << std::endl;
			block = new_block;
		}
		iter++;
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
		const auto& key = iter->first;
		if(iter->second.second >= new_version) {
			if(!mem_block.empty()) {
				const auto found = mem_block.lower_bound(std::make_pair(key, -1));
				if(found != mem_block.end()) {
					if(*found->first.first == *key) {
						iter->second = std::make_pair(found->second, found->first.second);
						iter++;
						continue;
					}
				}
			}
			iter = mem_index.erase(iter);
		} else {
			iter++;
		}
	}
	curr_version = new_version;
}

void Table::flush()
{
	std::lock_guard lock(mutex);
	if(write_lock) {
		throw std::logic_error("table is write locked");
	}
	if(mem_block.empty()) {
		return;
	}
	const auto time_begin = vnx::get_wall_time_millis();

	auto block = create_block(0, "flush.tmp");
	block->index.reserve(mem_index.size());

	auto& out = block->file.out;
	block->file.seek_to(block_header_size);

	std::shared_ptr<db_val_t> prev;
	for(const auto& entry : mem_block) {
		const auto& version = entry.first.second;
		const auto& key = entry.first.first;
		if(!prev || *key != *prev) {
			block->index.push_back(out.get_output_pos());
		}
		block->total_count++;
		block->min_version = std::min(version, block->min_version);
		block->max_version = std::max(version, block->max_version);
		write_entry(out, version, key, entry.second);
		prev = key;
	}
	block->index_offset = out.get_output_pos();

	finish_block(block);
	rename(block, next_block_id++);

	mem_index.clear();
	mem_block.clear();
	mem_block_size = 0;
	blocks.push_back(block);

	// clear log after writing index
	write_log.open("wb");

	debug_log << "Flushed " << block->name << " with " << block->index.size() << " / " << block->total_count
			<< " entries, min_version = " << block->min_version << ", max_version = " << block->max_version
			<< ", took " << (vnx::get_wall_time_millis() - time_begin) / 1e3 << " sec" << std::endl;

	check_rewrite();
}

std::shared_ptr<Table::block_t>
Table::rewrite(std::list<std::shared_ptr<block_t>> blocks, const uint32_t level) const
{
	if(blocks.empty()) {
		throw std::logic_error("no blocks given");
	}
	size_t total_index_entries = 0;
	for(const auto& block : blocks) {
		if(block->level + 1 != level) {
			throw std::logic_error("level mismatch");
		}
		total_index_entries += block->index.size();
	}

	struct pointer_t {
		uint64_t offset = 0;
		std::shared_ptr<block_t> block;
		std::shared_ptr<db_val_t> value;
	};
	std::map<std::pair<std::shared_ptr<db_val_t>, uint32_t>, pointer_t, mem_compare_t> block_map(mem_compare_t(this));

	for(const auto& block : blocks) {
		if(!block->total_count) {
			continue;
		}
		block->file.seek_to(block_header_size);

		pointer_t entry;
		entry.block = block;
		uint32_t version;
		std::shared_ptr<db_val_t> key;
		read_entry(block->file.in, version, key, entry.value);
		block_map[std::make_pair(key, version)] = entry;
	}

	auto block = create_block(level, "rewrite.tmp");
	block->index.reserve(total_index_entries);

	auto& out = block->file.out;
	block->file.seek_to(block_header_size);

	std::shared_ptr<db_val_t> prev;
	while(!block_map.empty()) {
		const auto iter = block_map.begin();
		const auto& key = iter->first.first;
		const auto& version = iter->first.second;
		if(!prev || *key != *prev) {
			block->index.push_back(out.get_output_pos());
		}
		block->total_count++;
		block->min_version = std::min(version, block->min_version);
		block->max_version = std::max(version, block->max_version);
		write_entry(out, version, key, iter->second.value);
		prev = key;

		auto entry = iter->second;
		if(++entry.offset < entry.block->total_count) {
			uint32_t version;
			std::shared_ptr<db_val_t> key;
			read_entry(entry.block->file.in, version, key, entry.value);
			block_map[std::make_pair(key, version)] = entry;
		}
		block_map.erase(iter);
	}
	block->index_offset = out.get_output_pos();

	finish_block(block);
	return block;
}

void Table::check_rewrite()
{
	if(options.level_factor <= 1) {
		return;
	}
	uint32_t level = 0;
	auto iter_begin = blocks.begin();
	auto iter_end = iter_begin;
	std::list<std::shared_ptr<block_t>> selected;
	for(auto iter = blocks.begin(); iter != blocks.end(); ++iter) {
		const auto& block = *iter;
		if(selected.empty() || block->level == level) {
			selected.push_back(block);
			if(selected.size() > options.level_factor) {
				iter_end = iter;
				break;
			}
		} else {
			selected.clear();
			selected.push_back(block);
			iter_begin = iter;
		}
		level = block->level;
	}
	if(selected.size() <= options.level_factor) {
		return;
	}
	selected.pop_back();

	const auto time_begin = vnx::get_wall_time_millis();

	const auto block = rewrite(selected, level + 1);
	rename(block, next_block_id++);

	debug_log << "Wrote " << block->name << " at level " << block->level
			<< " with " << block->index.size() << " / " << block->total_count
			<< " entries, min_version = " << block->min_version << ", max_version = " << block->max_version
			<< ", took " << (vnx::get_wall_time_millis() - time_begin) / 1e3 << " sec" << std::endl;

	for(const auto& block : selected) {
		block->file.remove();
		debug_log << "Deleted " << block->name << std::endl;
	}
	blocks.insert(iter_begin, block);
	blocks.erase(iter_begin, iter_end);

	check_rewrite();
}

void Table::write_block_header(vnx::TypeOutput& out, std::shared_ptr<block_t> block) const
{
	vnx::write(out, uint16_t(0));
	vnx::write(out, block->level);
	vnx::write(out, block->min_version);
	vnx::write(out, block->max_version);
	vnx::write(out, block->total_count);
	vnx::write(out, block->index_offset);
}

void Table::write_block_index(vnx::TypeOutput& out, std::shared_ptr<block_t> block) const
{
	vnx::write(out, uint64_t(block->index.size()));
	out.write(block->index.data(), block->index.size() * 8);
}

std::shared_ptr<Table::block_t> Table::create_block(const uint32_t level, const std::string& name) const
{
	auto block = std::make_shared<block_t>();
	block->level = level;
	block->name = name;
	block->file.open(root_path + '/' + block->name, "wb");
	block->min_version = -1;
	return block;
}

void Table::finish_block(std::shared_ptr<block_t> block) const
{
	auto& file = block->file;

	file.seek_begin();
	write_block_header(file.out, block);

	file.seek_to(block->index_offset);
	write_block_index(file.out, block);

	file.close();
}

void Table::rename(std::shared_ptr<block_t> block, const uint64_t new_index) const
{
	rename(block, to_number(new_index, 6) + ".dat");
}

void Table::rename(std::shared_ptr<block_t> block, const std::string& new_name) const
{
	block->name = new_name;
	block->file.rename(root_path + '/' + block->name);
	block->file.open("rb");
}

Table::Iterator::Iterator(const Table* table)
	:	table(table), block_map(compare_t(this))
{
	std::lock_guard lock(table->mutex);
	table->write_lock++;
}

Table::Iterator::Iterator(std::shared_ptr<const Table> table)
	:	Iterator(table.get())
{
	p_table = table;
}

Table::Iterator::~Iterator()
{
	std::lock_guard lock(table->mutex);
	table->write_lock--;
}

bool Table::Iterator::compare_t::operator()(
		const std::pair<std::shared_ptr<db_val_t>, uint32_t>& lhs, const std::pair<std::shared_ptr<db_val_t>, uint32_t>& rhs) const
{
	if(!iter->direction) {
		throw std::logic_error("!direction");
	}
	const auto res = iter->table->options.comparator(*lhs.first, *rhs.first);
	if(res == 0) {
		return lhs.second > rhs.second;
	}
	return iter->direction > 0 ? res < 0 : res > 0;
}

bool Table::Iterator::is_valid() const
{
	return !block_map.empty() && direction;
}

std::map<std::pair<std::shared_ptr<db_val_t>, uint32_t>, Table::Iterator::pointer_t, Table::key_compare_t>::const_iterator
Table::Iterator::current() const
{
	if(!is_valid()) {
		throw std::logic_error("iterator not valid");
	}
	return block_map.begin();
}

uint32_t Table::Iterator::version() const
{
	return current()->first.second;
}

std::shared_ptr<db_val_t> Table::Iterator::key() const
{
	return current()->first.first;
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
		auto iter = block_map.begin();
		const auto prev = iter->first.first;
		// iterate until next key
		while(iter != block_map.end() && *iter->first.first == *prev) {
			const auto& entry = iter->second;
			if(const auto& block = entry.block) {
				if(entry.pos > 0) {
					const auto pos = entry.pos - 1;
					pointer_t next;
					next.block = block;
					next.pos = pos;
					uint32_t version;
					std::shared_ptr<db_val_t> key;
					read_entry_at(entry.block->file, block->index[pos], version, key, next.value);
					block_map[std::make_pair(key, version)] = next;
				}
			} else {
				if(entry.iter != table->mem_index.begin()) {
					auto iter = entry.iter; iter--;
					pointer_t next;
					next.iter = iter;
					next.value = iter->second.first;
					block_map[std::make_pair(iter->first, iter->second.second)] = next;
				}
			}
			iter = block_map.erase(iter);
		}
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
		auto iter = block_map.begin();
		const auto prev = iter->first.first;
		// iterate until next key
		while(iter != block_map.end() && *iter->first.first == *prev) {
			const auto& entry = iter->second;
			if(const auto& block = entry.block) {
				const auto pos = entry.pos + 1;
				if(pos < block->index.size()) {
					pointer_t next;
					next.block = block;
					next.pos = pos;
					uint32_t version;
					std::shared_ptr<db_val_t> key;
					read_entry_at(entry.block->file, block->index[pos], version, key, next.value);
					block_map[std::make_pair(key, version)] = next;
				}
			} else {
				auto iter = entry.iter; iter++;
				if(iter != table->mem_index.end()) {
					pointer_t next;
					next.iter = iter;
					next.value = iter->second.first;
					block_map[std::make_pair(iter->first, iter->second.second)] = next;
				}
			}
			iter = block_map.erase(iter);
		}
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
	seek(table->blocks, key, mode);

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
			pointer_t entry;
			entry.iter = iter;
			entry.value = iter->second.first;
			block_map[std::make_pair(iter->first, iter->second.second)] = entry;
		}
	}
}

void Table::Iterator::seek(const std::list<std::shared_ptr<block_t>>& blocks, std::shared_ptr<db_val_t> key, const int mode)
{
	block_map.clear();
	direction = mode >= 0 ? 1 : -1;

	for(const auto& block : blocks)
	{
		const auto end = block->index.size();

		bool is_match = false;
		uint32_t version = -1;
		size_t pos = 0;
		auto res = key;
		if(res) {
			pos = table->lower_bound(block, version, res, is_match);
		}
		else if(mode == 0) {
			if(pos < end) {
				read_key_at(block->file, block->index[pos], version, res);
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
			read_key_at(block->file, block->index[--pos], version, res);
		}
		else if(mode > 0) {
			if(pos + 1 >= end) {
				continue;
			}
			read_key_at(block->file, block->index[++pos], version, res);
		}
		if(!res) {
			continue;
		}
		pointer_t entry;
		entry.block = block;
		entry.pos = pos;
		read_value_at(block->file, block->index[pos], res, entry.value);
		block_map[std::make_pair(res, version)] = entry;
	}
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
#pragma omp parallel for
	for(int i = 0; i < int(tables.size()); ++i) {
		tables[i]->revert(new_version);
	}
}

uint32_t DataBase::min_version() const
{
	if(tables.empty()) {
		return 0;
	}
	uint32_t min_version = -1;
	for(const auto& table : tables) {
		min_version = std::min(table->current_version(), min_version);
	}
	return min_version;
}

uint32_t DataBase::recover()
{
	const auto version = min_version();
	revert(version);
	return version;
}


} // mmx
