/*
 * test_engine.cpp
 *
 *  Created on: May 1, 2022
 *      Author: mad
 */

#include <mmx/vm/Engine.h>
#include <mmx/vm/StorageRAM.h>
#include <mmx/vm/StorageProxy.h>

#include <iostream>

using namespace mmx;


int main(int arcv, char** argv)
{
	auto backend = std::make_shared<vm::StorageRAM>();
	{
		vm::Engine engine(addr_t(), backend, false);
		engine.assign(vm::MEM_CONST + 0, new vm::var_t());
		engine.assign(vm::MEM_CONST + 1, new vm::uint_t(1337));

		auto& code = engine.code;
		code.emplace_back(vm::OP_NOP);
		code.emplace_back(vm::OP_COPY, 0, vm::MEM_STACK + 0, vm::MEM_CONST + 1);
		code.emplace_back(vm::OP_RET);

		engine.credits = 10000;
		engine.begin(0);
		engine.run();
		engine.dump_memory();
		std::cout << vm::to_string(engine.read(vm::MEM_STACK + 0)) << std::endl;
	}
	return 0;
}


