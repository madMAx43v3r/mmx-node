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

const std::function<bool(const db_val_t&, const db_val_t&)> Table::default_comparator =
	[](const db_val_t& lhs, const db_val_t& rhs) -> bool {
		if(lhs.size == rhs.size) {
			return ::memcmp(lhs.data, rhs.data, lhs.size) < 0;
		}
		return lhs.size < rhs.size;
	};

Table::Table(const std::string& file_path, const std::function<bool(const db_val_t&, const db_val_t&)>& comparator)
	:	file_path(file_path), comparator(comparator), mem_block(mem_compare_t(this))
{
	debug_log.open(file_path + "debug.log", std::ios_base::app);

	index = vnx::read_from_file<TableIndex>(file_path + "index.dat");
	if(!index) {
		index = TableIndex::create();
		debug_log << "Initialized table" << std::endl;
	} else {
		debug_log << "Loaded table at version " << index->version << std::endl;
	}
	for(const auto& name : index->blocks) {
		blocks.push_back(read_block(name));
	}
//	for(const auto& name : index->deleted) {
//		vnx::File(file_path + name).remove();
//	}
//	index->deleted.clear();

	// purge left-over data
	revert(index->version);

	write_log->open(file_path + "write_log.dat", "ab");
	write_log->open("rb+");
	{
		auto& in = write_log->in;
		auto offset = in.get_input_pos();
		const auto min_version = blocks.empty() ? 0 : blocks.back()->max_version + 1;
		while(true) {
			try {
				uint32_t version;
				std::shared_ptr<db_val_t> key;
				std::shared_ptr<db_val_t> value;
				read_entry(write_log->in, version, key, value);
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
		write_log->seek_to(offset);
		debug_log << "Loaded " << mem_block.size() << " entries from write_log.dat" << std::endl;
	}
}

std::shared_ptr<Table::block_t> Table::read_block(const std::string& name) const
{
	auto block = std::make_shared<block_t>();
	block->name = name;

	vnx::File file(file_path + name);
	auto& in = file.in;

	uint16_t format = 0;
	vnx::read(in, format);
	if(format != 0) {
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

void Table::read_entry(vnx::TypeInput& in, uint32_t& version, std::shared_ptr<db_val_t>& key, std::shared_ptr<db_val_t>& value)
{
	vnx::read(in, version);
	{
		uint32_t size = 0;
		vnx::read(in, size);
		key = std::make_shared<db_val_t>(size);
		in.read(key->data, key->size);
	}
	{
		uint32_t size = 0;
		vnx::read(in, size);
		value = std::make_shared<db_val_t>(size);
		in.read(value->data, value->size);
	}
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
	write_entry(write_log->out, index->version, key, value);
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
	return value;
}

void Table::commit(const uint32_t new_version)
{
	if(new_version <= index->version) {
		throw std::logic_error("commit(): new version <= current version");
	}
	write_log->flush();

	if(mem_block_size >= max_block_size) {
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
	index->version = new_version;
	write_index();

	for(auto iter = blocks.begin(); iter != blocks.end();) {
		const auto& block = *iter;
		if(block->min_version >= new_version) {
			// delete block
			vnx::File(file_path + block->name).remove();
			iter = blocks.erase(iter);
		} else if(block->max_version >= new_version) {
			// update max version
			vnx::File file(file_path + block->name);
			file.seek_to(2 + 4 + 8);
			block->max_version = new_version - 1;
			vnx::write(file.out, block->max_version);
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
	file.seek_to(2 + 20 + mem_block.size() * 8);
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
	write_log->open("wb");
}

void Table::write_index()
{
	vnx::write_to_file(file_path + "index.dat", index);
}





} // mmx
