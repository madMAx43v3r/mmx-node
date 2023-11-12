/*
 * database_fill.cpp
 *
 *  Created on: May 24, 2023
 *      Author: mad
 */

#include <mmx/DataBase.h>


int main(int argc, char** argv)
{
	const std::string path = argc > 1 ? std::string(argv[1]) : "tmp/test_table";
	const size_t num_rows = argc > 2 ? ::atoi(argv[2]) : 1;
	const size_t key_size = argc > 3 ? ::atoi(argv[3]) : 32;
	const size_t value_size = argc > 4 ? ::atoi(argv[4]) : 32;

	::srand(::time(nullptr));

	auto table = std::make_shared<mmx::Table>(path);

	auto version = table->current_version();

	std::vector<uint8_t> key(key_size);
	std::vector<uint8_t> value(value_size);

	for(size_t i = 0; i < num_rows; ++i) {
		for(size_t k = 0; k < key_size; ++k) {
			key[k] = ::rand();
		}
		table->insert(
				std::make_shared<mmx::db_val_t>(key.data(), key.size()),
				std::make_shared<mmx::db_val_t>(value.data(), value.size()));

		if(i % 10000 == 0) {
			table->commit(++version);
		}
	}

	table->commit(++version);

	return 0;
}

