/*
 * test_engine.cpp
 *
 *  Created on: May 1, 2022
 *      Author: mad
 */

#include <mmx/vm/Engine.h>
#include <mmx/vm/StorageDB.h>
#include <mmx/vm/StorageRAM.h>
#include <mmx/vm/StorageCache.h>

#include <iostream>

using namespace mmx;


int main(int arcv, char** argv)
{
	auto db = std::make_shared<mmx::DataBase>(1);
//	auto storage = std::make_shared<vm::StorageRAM>();
	auto backend = std::make_shared<vm::StorageDB>("tmp/", db);
	db->revert(0);
	auto storage = std::make_shared<vm::StorageCache>(backend);
	{
		vm::Engine engine(addr_t(), storage, false);
		engine.write(vm::MEM_CONST + 0, vm::var_t());
		engine.write(vm::MEM_CONST + 1, vm::uint_t(1337));
		engine.write(vm::MEM_CONST + 2, vm::map_t());
		engine.assign(vm::MEM_CONST + 3, vm::binary_t::alloc("value"));
		engine.write(vm::MEM_CONST + 4, vm::uint_t(11337));
		engine.write(vm::MEM_CONST + 5, vm::array_t());

		auto& code = engine.code;
		code.emplace_back(vm::OP_CLONE, 0, vm::MEM_STACK + 1, vm::MEM_CONST + 1);
		code.emplace_back(vm::OP_COPY, vm::OPFLAG_REF_B, vm::MEM_STACK + 0, vm::MEM_STACK + 1);
		code.emplace_back(vm::OP_COPY, 0, vm::MEM_STATIC + 0, vm::MEM_CONST + 2);
		code.emplace_back(vm::OP_CLONE, 0, vm::MEM_STACK + 3, vm::MEM_CONST + 3);
		code.emplace_back(vm::OP_CONCAT, 0, vm::MEM_STACK + 4, vm::MEM_CONST + 3, vm::MEM_CONST + 3);
		code.emplace_back(vm::OP_SET, 0, vm::MEM_STATIC + 0, vm::MEM_CONST + 3, vm::MEM_CONST + 1);
		code.emplace_back(vm::OP_SET, 0, vm::MEM_STATIC + 0, vm::MEM_STACK + 4, vm::MEM_CONST + 3);
		code.emplace_back(vm::OP_SET, vm::OPFLAG_REF_B, vm::MEM_STATIC + 0, vm::MEM_STACK + 3, vm::MEM_CONST + 4);
		code.emplace_back(vm::OP_GET, 0, vm::MEM_STACK + 2, vm::MEM_STATIC + 0, vm::MEM_CONST + 3);
		code.emplace_back(vm::OP_COPY, 0, vm::MEM_STATIC + 1, vm::MEM_CONST + 5);
		code.emplace_back(vm::OP_PUSH_BACK, vm::OPFLAG_REF_A, vm::MEM_STATIC + 1, vm::MEM_CONST + 1);
		code.emplace_back(vm::OP_PUSH_BACK, vm::OPFLAG_REF_A, vm::MEM_STATIC + 1, vm::MEM_CONST + 4);
		code.emplace_back(vm::OP_PUSH_BACK, vm::OPFLAG_REF_A, vm::MEM_STATIC + 1, vm::MEM_CONST + 3);
		code.emplace_back(vm::OP_SIZE, 0, vm::MEM_STATIC + 8, vm::MEM_STATIC + 1);
		code.emplace_back(vm::OP_TYPE, 0, vm::MEM_STATIC + 9, vm::MEM_STATIC + 0);
		code.emplace_back(vm::OP_RET);

		engine.gas_limit = 1000000;
		engine.init();
		engine.begin(0);
		engine.run();
		engine.dump_memory();
		engine.commit();
		std::cout << "Cost: " << engine.gas_used << std::endl;
	}
	storage->commit();
	db->commit(1);
	{
		vm::Engine engine(addr_t(), storage, false);
		engine.write(vm::MEM_CONST + 0, vm::var_t());
		engine.write(vm::MEM_CONST + 1, vm::uint_t());
		engine.write(vm::MEM_CONST + 2, vm::uint_t(1));
		engine.assign(vm::MEM_CONST + 3, vm::binary_t::alloc("value"));

		auto& code = engine.code;
		code.emplace_back(vm::OP_GET, 0, vm::MEM_STACK + 0, vm::MEM_STATIC + 0, vm::MEM_CONST + 3);
		code.emplace_back(vm::OP_ERASE, 0, vm::MEM_STATIC + 0, vm::MEM_CONST + 3);
		code.emplace_back(vm::OP_GET, 0, vm::MEM_STATIC + 16, vm::MEM_STATIC + 1, vm::MEM_CONST + 1);
		code.emplace_back(vm::OP_GET, 0, vm::MEM_STATIC + 17, vm::MEM_STATIC + 1, vm::MEM_CONST + 2);
		code.emplace_back(vm::OP_POP_BACK, 0, vm::MEM_STATIC + 18, vm::MEM_STATIC + 1);
		code.emplace_back(vm::OP_RET);

		engine.gas_limit = 100000;
		engine.init();
		engine.begin(0);
		engine.run();
		engine.dump_memory();
		engine.commit();
		std::cout << "Cost: " << engine.gas_used << std::endl;
	}
	storage->commit();
	db->commit(2);
	{
		vm::Engine engine(addr_t(), storage, true);
		engine.write(vm::MEM_CONST + 0, vm::var_t());
		engine.write(vm::MEM_CONST + 1, vm::uint_t());
		engine.write(vm::MEM_CONST + 2, vm::uint_t(1));
		engine.assign(vm::MEM_CONST + 3, vm::binary_t::alloc("value"));
		engine.write(vm::MEM_CONST + 4, vm::uint_t(2));

		auto& code = engine.code;
		code.emplace_back(vm::OP_GET, 0, vm::MEM_STACK + 0, vm::MEM_STATIC + 0, vm::MEM_CONST + 3);
		code.emplace_back(vm::OP_GET, 0, vm::MEM_STATIC + 32, vm::MEM_STATIC + 1, vm::MEM_CONST + 1);
		code.emplace_back(vm::OP_GET, 0, vm::MEM_STATIC + 33, vm::MEM_STATIC + 1, vm::MEM_CONST + 2);
		code.emplace_back(vm::OP_GET, 0, vm::MEM_STATIC + 34, vm::MEM_STATIC + 1, vm::MEM_CONST + 4);
		code.emplace_back(vm::OP_CLONE, 0, vm::MEM_STACK + 1, vm::MEM_STATIC + 32);
		code.emplace_back(vm::OP_CLONE, 0, vm::MEM_STATIC + 40, vm::MEM_STATIC + 32);
		code.emplace_back(vm::OP_RET);

		engine.gas_limit = 100000;
		engine.init();
		engine.begin(0);
		engine.run();
		engine.dump_memory();
		std::cout << "Cost: " << engine.gas_used << std::endl;
	}
	{
		vm::Engine engine(addr_t(), storage, true);
		engine.write(vm::MEM_CONST + 0, vm::var_t());
		engine.write(vm::MEM_CONST + 1, vm::uint_t());
		engine.write(vm::MEM_CONST + 2, vm::uint_t(1));
		engine.write(vm::MEM_CONST + 3, vm::uint_t(10000));

		auto& code = engine.code;
		code.emplace_back(vm::OP_COPY, 0, vm::MEM_STACK + 0, vm::MEM_CONST + 1);
		code.emplace_back(vm::OP_CMP_GTE, 0, vm::MEM_STACK + 1, vm::MEM_STACK + 0, vm::MEM_CONST + 3);
		code.emplace_back(vm::OP_JUMPI, 0, 5, vm::MEM_STACK + 1);
		code.emplace_back(vm::OP_ADD, 0, vm::MEM_STACK + 0, vm::MEM_STACK + 0, vm::MEM_CONST + 2);
		code.emplace_back(vm::OP_JUMP, 0, 1);
		code.emplace_back(vm::OP_RET);

		engine.gas_limit = 10000000;
		engine.init();
		engine.begin(0);
		engine.run();
		engine.dump_memory();
		std::cout << "Cost: " << engine.gas_used << std::endl;
	}
	return 0;
}


