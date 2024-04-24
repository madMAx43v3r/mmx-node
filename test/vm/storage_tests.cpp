/*
 * storage_tests.cpp
 *
 *  Created on: Apr 23, 2024
 *      Author: mad
 */

#include <mmx/vm/Engine.h>
#include <mmx/vm/StorageDB.h>
#include <mmx/vm/StorageRAM.h>
#include <mmx/vm/StorageCache.h>
#include <mmx/vm_interface.h>

#include <vnx/vnx.h>
#include <vnx/test/Test.h>

#include <iostream>

using namespace mmx;


void expect(const vm::var_t* got, const vm::var_t* want) {
	if(compare(got, want)) {
		throw std::logic_error("expected " + to_string(want) + " but got " + to_string(got));
	}
}

void expect(const vm::var_t* got, const vm::var_t& want) {
	expect(got, &want);
}

void expect(const vm::var_t* got, vm::varptr_t want) {
	expect(got, want.get());
}


int main(int argc, char** argv)
{
	vnx::test::init("mmx.vm.storage");

	mmx::DataBase db;

	auto storage = std::make_shared<vm::StorageDB>("tmp/", db);
	auto engine = std::make_shared<vm::Engine>(addr_t(), storage, false);
	engine->gas_limit = 1000000;
	engine->init();

	db.revert(0);

	VNX_TEST_BEGIN("lookup")
	{
		{
			const auto key = engine->lookup(vm::to_binary("test"), false);
			engine->write(vm::MEM_STATIC + 0, vm::map_t());
			engine->write_key(vm::MEM_STATIC + 0, key, vm::uint_t(1337));
		}
		engine->commit();
		db.commit(1);
		{
			const auto key = engine->lookup(vm::to_binary("test"), true);
			const auto value = engine->read_key(vm::MEM_STATIC + 0, key);
			expect(value, vm::uint_t(1337));
		}
	}
	VNX_TEST_END()


	return vnx::test::done();
}

