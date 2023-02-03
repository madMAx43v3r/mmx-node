/*
 * dump_binary.cpp
 *
 *  Created on: Jan 6, 2023
 *      Author: mad
 */

#include <mmx/contract/Binary.hxx>
#include <mmx/vm_interface.h>

#include <vnx/vnx.h>


int main(int argc, char** argv)
{
	std::map<std::string, std::string> options;
	options["f"] = "file";
	options["m"] = "method";

	vnx::init("dump_binary", argc, argv, options);

	vnx::optional<std::string> method;
	vnx::read_config("method", method);

	std::string file_name;
	if(vnx::read_config("file", file_name))
	{
		if(auto bin = vnx::read_from_file<mmx::contract::Binary>(file_name))
		{
			mmx::vm::dump_code(std::cout, bin, method);
		}
		else {
			std::cerr << "Not a binary!" << std::endl;
		}
	}

	vnx::close();

	return 0;
}


