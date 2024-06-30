/*
 * table.cpp
 *
 *  Created on: Sep 11, 2022
 *      Author: mad
 */

#include <mmx/table.h>

#include <vnx/vnx.h>


namespace mmx {

void sync_type_codes(const std::string& file_path)
{
	uint_table<vnx::Hash64, vnx::TypeCode> table(file_path, false);

	table.scan([](const vnx::Hash64& code_hash, const vnx::TypeCode& type_code) -> bool {
		auto copy = std::make_shared<vnx::TypeCode>(type_code);
		copy->build();
		vnx::register_type_code(copy);
		return true;
	});

	size_t num_insert = 0;
	for(auto type_code : vnx::get_all_type_codes())
	{
		bool found = true;
		try {
			vnx::TypeCode tmp;
			found = table.find(type_code->code_hash, tmp);
		} catch(...) {
			// not found
		}
		if(!found) {
			table.insert(type_code->code_hash, *type_code);
			num_insert++;
		}
	}
	if(num_insert) {
		table.commit();
	}
}


} // mmx
