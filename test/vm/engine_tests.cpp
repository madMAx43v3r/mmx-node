/*
 * engine_tests.cpp
 *
 *  Created on: May 21, 2022
 *      Author: mad
 */

#include <mmx/vm/Engine.h>
#include <mmx/vm/StorageRAM.h>
#include <mmx/vm/StorageCache.h>
#include <mmx/vm/StorageRocksDB.h>

#include <vnx/vnx.h>
#include <vnx/test/Test.h>

#include <iostream>

using namespace mmx::vm;


void expect(const var_t* got, const var_t* want) {
	if(compare(got, want)) {
		throw std::logic_error("expected " + to_string(want) + " but got " + to_string(got));
	}
}
void expect(const var_t* got, varptr_t want) {
	expect(got, want.get());
}
void expect(varptr_t got, const var_t* want) {
	expect(got.get(), want);
}
void expect(varptr_t got, varptr_t want) {
	expect(got.get(), want.get());
}

void test_serialize(varptr_t var, bool with_rc, bool with_vf)
{
	auto data = serialize(*var.get(), with_rc, with_vf);
	var_t* res = nullptr;
	auto size = deserialize(res, data.first, data.second, with_rc, with_vf);
	vnx::test::expect(size, data.second);
	vnx::test::expect(compare(res, var.get()), 0);
	if(with_rc) {
		vnx::test::expect(res->ref_count, var->ref_count);
	}
	if(with_vf) {
		vnx::test::expect(res->flags, var->flags);
	}
	::free(data.first);
	delete res;
}

void test_serialize(varptr_t var) {
	test_serialize(var, true, true);
	test_serialize(var, false, true);
	test_serialize(var, true, false);
	test_serialize(var, false, false);
}

void test_compare_self(varptr_t var) {
	vnx::test::expect(var == var, true);
	vnx::test::expect(var != var, false);
	vnx::test::expect(var < var, false);
	vnx::test::expect(var > var, false);
}

void test_clone(varptr_t var) {
	auto res = clone(var.get());
	vnx::test::expect(compare(res, var.get()), 0);
	delete res;
}


int main(int argc, char** argv) {

	vnx::test::init("mmx.vm.engine");

	vnx::init("vm_engine_tests", argc, argv);

	VNX_TEST_BEGIN("serialize")
	{
		test_serialize(new var_t());
		test_serialize(new var_t(true));
		test_serialize(new var_t(false));
		test_serialize(new ref_t(uint64_t(-1)));
		test_serialize(new uint_t());
		test_serialize(new uint_t(uint64_t(1) << 8));
		test_serialize(new uint_t(uint64_t(1) << 16));
		test_serialize(new uint_t(uint64_t(1) << 32));
		test_serialize(new uint_t(uint128_t(1) << 64));
		test_serialize(new uint_t(uint256_t(1) << 128));
		test_serialize(new uint_t(uint256_max));
		test_serialize(binary_t::alloc("test"));
		test_serialize(binary_t::alloc(10, TYPE_BINARY));
		test_serialize(new array_t(-1));
		test_serialize(new map_t());
	}
	VNX_TEST_END()

	VNX_TEST_BEGIN("clone")
	{
		test_clone(nullptr);
		test_clone(new var_t());
		test_clone(new var_t(true));
		test_clone(new var_t(false));
		test_clone(new ref_t(uint64_t(-1)));
		test_clone(new uint_t(uint256_max));
		test_clone(binary_t::alloc("test"));
		test_clone(binary_t::alloc(10, TYPE_BINARY));
		test_clone(new array_t(-1));
		test_clone(new map_t());
	}
	VNX_TEST_END()

	VNX_TEST_BEGIN("compare_func")
	{
		test_compare_self(new var_t());
		test_compare_self(new var_t(true));
		test_compare_self(new var_t(false));
		test_compare_self(new ref_t(0));
		test_compare_self(new uint_t());
		test_compare_self(new uint_t(uint256_max));
		test_compare_self(binary_t::alloc("test"));
		test_compare_self(binary_t::alloc(10, TYPE_BINARY));
		test_compare_self(new array_t());
		test_compare_self(new map_t());
		vnx::test::expect(compare(var_t(), var_t(false)), -1);
		vnx::test::expect(compare(var_t(false), var_t(true)), -1);
		vnx::test::expect(compare(ref_t(0), ref_t(1)), -1);
		vnx::test::expect(compare(ref_t(1), ref_t(1)), 0);
		vnx::test::expect(compare(uint_t(), uint_t(1)), -1);
		vnx::test::expect(compare(uint_t(1), uint_t(1)), 0);
		vnx::test::expect(varptr_t(binary_t::alloc("")) < varptr_t(binary_t::alloc("test")), true);
		vnx::test::expect(varptr_t(binary_t::alloc("test")) == varptr_t(binary_t::alloc("test")), true);
	}
	VNX_TEST_END()

	VNX_TEST_BEGIN("arithmetic")
	{
		auto storage = std::make_shared<StorageRAM>();
		Engine engine(mmx::addr_t(), storage, false);
		engine.write(MEM_CONST + 1, uint_t());
		engine.write(MEM_CONST + 2, uint_t(1));
		engine.write(MEM_CONST + 3, uint_t(1337));
		engine.write(MEM_CONST + 4, uint_t(uint256_max));

		auto& code = engine.code;
		// TODO
		code.emplace_back(OP_RET);

		engine.total_gas = 100000;
		engine.init();
		engine.begin(0);
		engine.run();
		engine.commit();
	}
	VNX_TEST_END()

	vnx::close();
}





