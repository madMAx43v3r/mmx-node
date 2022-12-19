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
	vnx::init("test_compiler", argc, argv);

	vm::compile("1337");
//	vm::compile("\"1337\"");


	vnx::close();

	return 0;
}


