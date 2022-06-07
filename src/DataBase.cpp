/*
 * DataBase.cpp
 *
 *  Created on: Jun 7, 2022
 *      Author: mad
 */

#include <mmx/DataBase.h>

#include <vnx/vnx.h>


namespace mmx {

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
}

void Table::insert(std::shared_ptr<db_val_t> key, std::shared_ptr<db_val_t> value)
{
	if(!key || !value) {
		throw std::logic_error("!key || !value");
	}
	auto& out = write_log->out;
	vnx::write(out, version);
	vnx::write(out, key->size);
	out.write(key->data, key->size);
	vnx::write(out, value->size);
	out.write(value->data, value->size);

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
	if(new_version <= version) {
		throw std::logic_error("commit(): new version <= current version");
	}
	write_log->flush();

	if(mem_block_size >= max_block_size) {
		flush();
	}
	version = new_version;
}

void Table::revert(const uint32_t new_version)
{
	if(new_version > version) {
		throw std::logic_error("revert(): new version > current version");
	}
	for(auto iter = mem_block.begin(); iter != mem_block.end();) {
		if(iter->first.second > new_version) {
			iter = mem_block.erase(iter);
		} else {
			iter++;
		}
	}
}

void Table::flush()
{
	// TODO
}


} // mmx
