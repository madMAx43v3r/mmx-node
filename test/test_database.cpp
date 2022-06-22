/*
 * test_database.cpp
 *
 *  Created on: Jun 9, 2022
 *      Author: mad
 */

#include <mmx/DataBase.h>
#include <mmx/table.h>
#include <mmx/multi_table.h>

#include <vnx/vnx.h>
#include <vnx/test/Test.h>


template<typename T>
std::shared_ptr<mmx::db_val_t> db_write(T value)
{
	auto out = std::make_shared<mmx::db_val_t>(sizeof(T));
	vnx::write_value(out->data, vnx::flip_bytes(value));
	return out;
}

template<typename T>
T db_read(std::shared_ptr<mmx::db_val_t> value)
{
	if(!value) {
		throw std::runtime_error("!value");
	}
	if(value->size != sizeof(T)) {
		throw std::runtime_error("size mismatch: " + std::to_string(value->size) + " != " + std::to_string(sizeof(T)));
	}
	T out = T();
	::memcpy(&out, value->data, sizeof(T));
	return vnx::flip_bytes(out);
}


int main(int argc, char** argv)
{
	vnx::init("test_database", argc, argv);
	{
		const uint32_t num_entries = 100;

		auto table = std::make_shared<mmx::Table>("tmp/test_table");
		table->max_block_size = 1024 * 1024;

		if(table->current_version() == 1) {
			for(uint32_t i = 0; i < num_entries; ++i) {
				vnx::test::expect(db_read<uint64_t>(table->find(db_write(uint32_t(i * 2)))), i);
			}
		}
		table->revert(0);

		for(uint32_t i = 0; i < num_entries; ++i) {
			table->insert(db_write(uint32_t(i * 2)), db_write(uint64_t(i)));
		}
		table->commit(1);

		for(uint32_t i = 0; i < num_entries; ++i) {
			vnx::test::expect(db_read<uint64_t>(table->find(db_write(uint32_t(i * 2)))), i);
		}
		if(false) {
			auto iter = std::make_shared<mmx::Table::Iterator>(table);
			iter->seek_begin();
			uint32_t i = 0;
			while(iter->is_valid()) {
				vnx::test::expect(db_read<uint32_t>(iter->key()), i * 2);
				vnx::test::expect(db_read<uint64_t>(iter->value()), i);
				iter->next();
				i++;
			}
		}
		table->flush();

		for(uint32_t i = 2 * num_entries; i > 0; --i) {
			table->insert(db_write(uint32_t((i - 1) * 2)), db_write(uint64_t(i)));
		}
		table->commit(2);

		for(uint32_t i = 0; i < 2 * num_entries; ++i) {
			vnx::test::expect(db_read<uint64_t>(table->find(db_write(uint32_t(i * 2)))), i + 1);
		}
		table->flush();

		for(uint32_t i = 0; i < 2 * num_entries; ++i) {
			vnx::test::expect(db_read<uint64_t>(table->find(db_write(uint32_t(i * 2)))), i + 1);
		}
		if(table->find(db_write(uint32_t(5 * num_entries)))) {
			throw std::logic_error("found something");
		}
		{
			auto iter = std::make_shared<mmx::Table::Iterator>(table);
			iter->seek_begin();
			uint32_t i = 0;
			while(iter->is_valid()) {
				vnx::test::expect(db_read<uint32_t>(iter->key()), i * 2);
				vnx::test::expect(db_read<uint64_t>(iter->value()), i + 1);
				iter->next();
				i++;
			}
		}
		{
			auto iter = std::make_shared<mmx::Table::Iterator>(table);
			iter->seek(db_write(uint32_t(num_entries + 1)));
			vnx::test::expect(iter->is_valid(), true);
			vnx::test::expect(db_read<uint32_t>(iter->key()), num_entries + 2);
			iter->prev();
			vnx::test::expect(iter->is_valid(), true);
			vnx::test::expect(db_read<uint32_t>(iter->key()), num_entries);
			iter->prev();
			vnx::test::expect(iter->is_valid(), true);
			vnx::test::expect(db_read<uint32_t>(iter->key()), num_entries - 2);
		}
		table->revert(1);

		for(uint32_t i = 0; i < num_entries; ++i) {
			vnx::test::expect(db_read<uint64_t>(table->find(db_write(uint32_t(i * 2)))), i);
		}
	}
	{
		mmx::uint_table<uint32_t, std::string> table("tmp/uint_table");
		table.revert(0);
		table.insert(3, "dfhgdfgdfgdfg");
		table.insert(1, "sdfsdfsdf");
		{
			std::string value;
			const auto res = table.find(1, value);
			vnx::test::expect(res, true);
			vnx::test::expect(value, "sdfsdfsdf");
		}
		{
			std::string value;
			const auto res = table.find(3, value);
			vnx::test::expect(res, true);
			vnx::test::expect(value, "dfhgdfgdfgdfg");
		}
		table.commit(1);
		table.flush();
		{
			std::string value;
			const auto res = table.find(1, value);
			vnx::test::expect(res, true);
			vnx::test::expect(value, "sdfsdfsdf");
		}
		{
			std::string value;
			const auto res = table.find(3, value);
			vnx::test::expect(res, true);
			vnx::test::expect(value, "dfhgdfgdfgdfg");
		}
		table.scan([](const uint32_t& key, const std::string& value) {
			std::cout << key << " => " << value << std::endl;
		});
	}
	{
		mmx::uint_multi_table<uint32_t, std::string> table("tmp/uint_multi_table");
		table.revert(0);
		table.insert(1, "234523345");
		table.insert(3, "dfhgdfgdfgdfg");
		table.insert(1, "sdfsdfsdf");
		{
			std::vector<std::string> value;
			const auto res = table.find(1, value);
			vnx::test::expect(res, size_t(2));
			vnx::test::expect(value[0], "234523345");
			vnx::test::expect(value[1], "sdfsdfsdf");
		}
		{
			std::vector<std::string> value;
			const auto res = table.find(3, value);
			vnx::test::expect(res, size_t(1));
			vnx::test::expect(value[0], "dfhgdfgdfgdfg");
		}
		table.commit(1);
		table.flush();
		{
			std::vector<std::string> value;
			const auto res = table.find(1, value);
			vnx::test::expect(res, size_t(2));
			vnx::test::expect(value[0], "234523345");
			vnx::test::expect(value[1], "sdfsdfsdf");
		}
		{
			std::vector<std::string> value;
			const auto res = table.find(3, value);
			vnx::test::expect(res, size_t(1));
			vnx::test::expect(value[0], "dfhgdfgdfgdfg");
		}
		table.scan([](const uint32_t& key, const std::string& value) {
			std::cout << key << " => " << value << std::endl;
		});
	}
	vnx::close();
	return 0;
}

