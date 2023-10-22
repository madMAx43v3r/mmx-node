/*
 * dump_table.cpp
 *
 *  Created on: Oct 22, 2023
 *      Author: mad
 */

#include <mmx/DataBase.h>


int main(int argc, char** argv)
{
	if(argc < 1) {
		return -1;
	}
	mmx::Table table(argv[1]);

	mmx::Table::Iterator iter(&table);

	iter.seek_begin();

	while(iter.is_valid())
	{
		const auto key = iter.key();
		const auto value = iter.value();
		std::cout << "[" << key->to_hex_string() << "] => " << value->to_hex_string() << std::endl;

		iter.next();
	}
	std::cout << "version = " << table.current_version() << std::endl;
}


