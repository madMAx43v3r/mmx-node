/*
 * engine_tests.cpp
 *
 *  Created on: May 21, 2022
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
void expect(const vm::var_t* got, vm::varptr_t want) {
	expect(got, want.get());
}
void expect(vm::varptr_t got, const vm::var_t* want) {
	expect(got.get(), want);
}
void expect(vm::varptr_t got, vm::varptr_t want) {
	expect(got.get(), want.get());
}

void test_serialize(vm::varptr_t var, bool with_rc, bool with_vf)
{
	const auto data = serialize(*var.get(), with_rc, with_vf);
	std::unique_ptr<vm::var_t> res;
	auto size = deserialize(res, data.first.get(), data.second, with_rc, with_vf);
	vnx::test::expect(size, data.second);
	vnx::test::expect(compare(res.get(), var.get()), 0);
	if(with_rc) {
		vnx::test::expect(res->ref_count, var->ref_count);
	}
	if(with_vf) {
		vnx::test::expect(res->flags, var->flags);
	}
}

void test_serialize(vm::varptr_t var) {
	test_serialize(var, true, true);
	test_serialize(var, false, true);
	test_serialize(var, true, false);
	test_serialize(var, false, false);
}

void test_compare_self(vm::varptr_t var) {
	vnx::test::expect(var == var, true);
	vnx::test::expect(var != var, false);
	vnx::test::expect(var < var, false);
	vnx::test::expect(var > var, false);
}

void test_clone(vm::varptr_t var) {
	const auto res = clone(var.get());
	vnx::test::expect(compare(res.get(), var.get()), 0);
}


int main(int argc, char** argv) {

	vnx::test::init("mmx.vm.engine");

	vnx::init("vm_engine_tests", argc, argv);

	VNX_TEST_BEGIN("serialize")
	{
		test_serialize(std::make_unique<vm::var_t>());
		test_serialize(std::make_unique<vm::var_t>(true));
		test_serialize(std::make_unique<vm::var_t>(false));
		test_serialize(std::make_unique<vm::ref_t>(uint64_t(-1)));
		test_serialize(std::make_unique<vm::uint_t>());
		test_serialize(std::make_unique<vm::uint_t>(uint64_t(1) << 8));
		test_serialize(std::make_unique<vm::uint_t>(uint64_t(1) << 16));
		test_serialize(std::make_unique<vm::uint_t>(uint64_t(1) << 32));
		test_serialize(std::make_unique<vm::uint_t>(uint128_t(1) << 64));
		test_serialize(std::make_unique<vm::uint_t>(uint256_t(1) << 128));
		test_serialize(std::make_unique<vm::uint_t>(uint256_max));
		test_serialize(vm::binary_t::alloc("test"));
		test_serialize(vm::binary_t::alloc(10, vm::TYPE_BINARY));
		test_serialize(std::make_unique<vm::array_t>(-1));
		test_serialize(std::make_unique<vm::map_t>());
	}
	VNX_TEST_END()

	VNX_TEST_BEGIN("clone")
	{
		test_clone(nullptr);
		test_clone(std::make_unique<vm::var_t>());
		test_clone(std::make_unique<vm::var_t>(true));
		test_clone(std::make_unique<vm::var_t>(false));
		test_clone(std::make_unique<vm::ref_t>(uint64_t(-1)));
		test_clone(std::make_unique<vm::uint_t>(uint256_max));
		test_clone(vm::binary_t::alloc("test"));
		test_clone(vm::binary_t::alloc(10, vm::TYPE_BINARY));
		test_clone(std::make_unique<vm::array_t>(-1));
		test_clone(std::make_unique<vm::map_t>());
	}
	VNX_TEST_END()

	VNX_TEST_BEGIN("compare_func")
	{
		test_compare_self(std::make_unique<vm::var_t>());
		test_compare_self(std::make_unique<vm::var_t>(true));
		test_compare_self(std::make_unique<vm::var_t>(false));
		test_compare_self(std::make_unique<vm::ref_t>(0));
		test_compare_self(std::make_unique<vm::uint_t>());
		test_compare_self(std::make_unique<vm::uint_t>(uint256_max));
		test_compare_self(vm::binary_t::alloc("test"));
		test_compare_self(vm::binary_t::alloc(10, vm::TYPE_BINARY));
		test_compare_self(std::make_unique<vm::array_t>());
		test_compare_self(std::make_unique<vm::map_t>());
		vnx::test::expect(compare(vm::var_t(), vm::var_t(false)), -1);
		vnx::test::expect(compare(vm::var_t(false), vm::var_t(true)), -1);
		vnx::test::expect(compare(vm::ref_t(0), vm::ref_t(1)), -1);
		vnx::test::expect(compare(vm::ref_t(1), vm::ref_t(1)), 0);
		vnx::test::expect(compare(vm::uint_t(), vm::uint_t(1)), -1);
		vnx::test::expect(compare(vm::uint_t(1), vm::uint_t(1)), 0);
		vnx::test::expect(vm::varptr_t(vm::binary_t::alloc("")) < vm::varptr_t(vm::binary_t::alloc("test")), true);
		vnx::test::expect(vm::varptr_t(vm::binary_t::alloc("test")) == vm::varptr_t(vm::binary_t::alloc("test")), true);
	}
	VNX_TEST_END()

	VNX_TEST_BEGIN("arithmetic")
	{
		auto storage = std::make_shared<vm::StorageRAM>();
		vm::Engine engine(mmx::addr_t(), storage, false);
		engine.write(vm::MEM_CONST + 1, vm::uint_t());
		engine.write(vm::MEM_CONST + 2, vm::uint_t(1));
		engine.write(vm::MEM_CONST + 3, vm::uint_t(1337));
		engine.write(vm::MEM_CONST + 4, vm::uint_t(uint256_max));
		engine.write(vm::MEM_CONST + 5, vm::uint_t(uint256_max >> 100));
		engine.write(vm::MEM_CONST + 6, vm::uint_t(uint256_max >> 128));
		engine.write(vm::MEM_CONST + 7, vm::uint_t(100));
		engine.write(vm::MEM_CONST + 8, vm::uint_t(128));

		auto& code = engine.code;
		code.emplace_back(vm::OP_COPY, 0, vm::MEM_STACK + 1, vm::MEM_CONST + 4);
		code.emplace_back(vm::OP_SHR, 0, vm::MEM_STATIC + 1, vm::MEM_STACK + 1, 100);
		code.emplace_back(vm::OP_SHR, vm::OPFLAG_REF_C, vm::MEM_STATIC + 2, vm::MEM_STACK + 1, vm::MEM_CONST + 8);
		code.emplace_back(vm::OP_RET);

		engine.total_gas = 1000000;
		engine.init();
		engine.begin(0);
		engine.run();
		vnx::test::expect(compare(engine.read_fail(vm::MEM_STATIC + 1), vm::uint_t(uint256_max >> 100)), 0);
		vnx::test::expect(compare(engine.read_fail(vm::MEM_STATIC + 2), vm::uint_t(uint256_max >> 128)), 0);
		engine.commit();
	}
	VNX_TEST_END()

	vnx::close();
}





