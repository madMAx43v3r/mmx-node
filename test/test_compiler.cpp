/*
 * test_compiler.cpp
 *
 *  Created on: Dec 19, 2022
 *      Author: mad
 */

#include <mmx/vm/Compiler.h>

#include <vnx/vnx.h>


using namespace mmx;


int main(int argc, char** argv)
{
	std::map<std::string, std::string> options;
	options["f"] = "file";

	vnx::init("test_compiler", argc, argv, options);

	std::string file_name;
	if(vnx::read_config("file", file_name))
	{
		std::ifstream stream(file_name);
		std::stringstream buffer;
		buffer << stream.rdbuf();

		vm::compile(buffer.str());
	}

	vnx::close();

	return 0;
}


