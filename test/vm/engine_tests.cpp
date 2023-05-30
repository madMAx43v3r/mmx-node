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

void expect(const vm::var_t* got, const vm::var_t& want) {
	expect(got, &want);
}

void expect(const vm::var_t* got, vm::varptr_t want) {
	expect(got, want.get());
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

void test_compare(vm::varptr_t var) {
	vnx::test::expect(var == var, true);
	vnx::test::expect(var != var, false);
	vnx::test::expect(var < var, false);
	vnx::test::expect(var > var, false);

	vm::varptr_t null;
	vnx::test::expect(var == null, var.get() == nullptr);
	vnx::test::expect(var < null, false);
	vnx::test::expect(var > null, var.get() != nullptr);
}

void test_clone(vm::varptr_t var) {
	const auto res = clone(var.get());
	vnx::test::expect(compare(res.get(), var.get()), 0);
}


int main(int argc, char** argv)
{
	vnx::test::init("mmx.vm.engine");

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
		test_compare(std::make_unique<vm::var_t>());
		test_compare(std::make_unique<vm::var_t>(true));
		test_compare(std::make_unique<vm::var_t>(false));
		test_compare(std::make_unique<vm::ref_t>(0));
		test_compare(std::make_unique<vm::uint_t>());
		test_compare(std::make_unique<vm::uint_t>(uint256_max));
		test_compare(vm::binary_t::alloc("test"));
		test_compare(vm::binary_t::alloc(10, vm::TYPE_BINARY));
		test_compare(std::make_unique<vm::array_t>());
		test_compare(std::make_unique<vm::map_t>());
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

	VNX_TEST_BEGIN("stuff")
	{
		vnx::test::expect(vm::to_binary(hash_t(""))->to_hash(), hash_t(""));
		{
			auto var = vm::binary_t::alloc("test");
			var = vm::binary_t::alloc("test1");
			vnx::test::expect(var->to_string(), std::string("test1"));
		}
	}
	VNX_TEST_END()

	auto storage = std::make_shared<vm::StorageRAM>();

	VNX_TEST_BEGIN("setup")
	{
		auto engine = std::make_shared<vm::Engine>(addr_t(), storage, false);
		engine->gas_limit = 1000000;
		vm::assign(engine, vm::MEM_STATIC + 1, vnx::Variant());
		vm::assign(engine, vm::MEM_STATIC + 2, vnx::Variant(true));
		vm::assign(engine, vm::MEM_STATIC + 3, vnx::Variant(false));
		vm::assign(engine, vm::MEM_STATIC + 4, vnx::Variant(1337));
		vm::assign(engine, vm::MEM_STATIC + 5, vnx::Variant("test"));
		vm::assign(engine, vm::MEM_STATIC + 6, vnx::Variant(std::vector<uint8_t>{1, 2, 3, 4}));
		vm::assign(engine, vm::MEM_STATIC + 7, vnx::Variant(hash_t("test")));
		vm::assign(engine, vm::MEM_STATIC + 8, vnx::Variant(addr_t("mmx17uuqmktq33mmh278d3nlqy0mrgw9j2vtg4l5vrte3m06saed9yys2q5hrf")));
		vm::assign(engine, vm::MEM_STATIC + 9, vnx::Variant(std::vector<int64_t>{11, 12, 13, 14}));
		{
			vnx::Object tmp;
			tmp["field"] = 123;
			tmp["field1"] = "test";
			tmp["field2"] = std::vector<uint32_t>{11, 12, 13, 14};
			vm::assign(engine, vm::MEM_STATIC + 10, vnx::Variant(tmp));
		}
		engine->write(vm::MEM_STATIC + 11, vm::uint_t(-1));
		engine->commit();
	}
	VNX_TEST_END()

	VNX_TEST_BEGIN("assign")
	{
		auto engine = std::make_shared<vm::Engine>(addr_t(), storage, true);
		engine->gas_limit = 1000000;
		expect(engine->read(vm::MEM_STATIC + 1), vm::var_t());
		expect(engine->read(vm::MEM_STATIC + 2), vm::var_t(true));
		expect(engine->read(vm::MEM_STATIC + 3), vm::var_t(false));
		expect(engine->read(vm::MEM_STATIC + 4), vm::uint_t(1337));
		expect(engine->read(vm::MEM_STATIC + 5), vm::to_binary("test"));
		expect(engine->read(vm::MEM_STATIC + 6), vm::to_binary(bytes_t<4>(std::vector<uint8_t>{1, 2, 3, 4})));
		expect(engine->read(vm::MEM_STATIC + 7), vm::to_binary(hash_t("test")));
		expect(engine->read(vm::MEM_STATIC + 8), vm::to_binary(addr_t("mmx17uuqmktq33mmh278d3nlqy0mrgw9j2vtg4l5vrte3m06saed9yys2q5hrf")));
		{
			const auto& ref = engine->read_fail<vm::ref_t>(vm::MEM_STATIC + 9, vm::TYPE_REF);
			const auto& var = engine->read_fail<vm::array_t>(ref.address, vm::TYPE_ARRAY);
			vnx::test::expect(var.size, 4u);
			vnx::test::expect(var.address, ref.address);
			vnx::test::expect(var.ref_count, 1u);
			for(size_t i = 0; i < 4; ++i) {
				expect(engine->read_entry(ref.address, i), vm::uint_t(i + 11));
			}
		}
		{
			const auto& ref = engine->read_fail<vm::ref_t>(vm::MEM_STATIC + 10, vm::TYPE_REF);
			const auto& var = engine->read_fail<vm::map_t>(ref.address, vm::TYPE_MAP);
			vnx::test::expect(var.address, ref.address);
			expect(engine->read_key(ref.address, vm::to_binary("field")), vm::uint_t(123));
			expect(engine->read_key(ref.address, vm::to_binary("field1")), vm::to_binary("test"));
			{
				const auto& ref2 = engine->read_key_fail<vm::ref_t>(ref.address,
						engine->lookup(vm::to_binary("field2"), true), vm::TYPE_REF);
				const auto& var = engine->read_fail<vm::array_t>(ref2.address, vm::TYPE_ARRAY);
				vnx::test::expect(var.size, 4u);
				for(size_t i = 0; i < 4; ++i) {
					expect(engine->read_entry(ref2.address, i), vm::uint_t(i + 11));
				}
			}
		}
		expect(engine->read(vm::MEM_STATIC + 11), vm::uint_t(-1));
	}
	VNX_TEST_END()

	VNX_TEST_BEGIN("convert")
	{
		auto engine = std::make_shared<vm::Engine>(addr_t(), storage, true);
		engine->gas_limit = 1000000;
		vnx::test::expect(vm::read(engine, vm::MEM_STATIC + 1).is_null(), true);
		vnx::test::expect(vm::read(engine, vm::MEM_STATIC + 2).to<bool>(), true);
		vnx::test::expect(vm::read(engine, vm::MEM_STATIC + 3).to<bool>(), false);
		vnx::test::expect(vm::read(engine, vm::MEM_STATIC + 4).to<int64_t>(), 1337);
		vnx::test::expect(vm::read(engine, vm::MEM_STATIC + 5).to<std::string>(), "test");
		vnx::test::expect(vm::read(engine, vm::MEM_STATIC + 6).to<std::vector<uint8_t>>(), std::vector<uint8_t>{1, 2, 3, 4});
		vnx::test::expect(vm::read(engine, vm::MEM_STATIC + 7).to<hash_t>(), hash_t("test"));
		vnx::test::expect(vm::read(engine, vm::MEM_STATIC + 8).to<addr_t>(), addr_t("mmx17uuqmktq33mmh278d3nlqy0mrgw9j2vtg4l5vrte3m06saed9yys2q5hrf"));
		vnx::test::expect(vm::read(engine, vm::MEM_STATIC + 9).to<std::vector<int64_t>>(), std::vector<int64_t>{11, 12, 13, 14});
		{
			const auto& ref = engine->read_fail<vm::ref_t>(vm::MEM_STATIC + 10, vm::TYPE_REF);
			engine->read_key(ref.address, vm::to_binary("field"));
			engine->read_key(ref.address, vm::to_binary("field1"));
			engine->read_key(ref.address, vm::to_binary("field2"));
		}
		{
			const auto var = vm::read(engine, vm::MEM_STATIC + 10);
			vnx::test::expect(var.is_object(), true);
			const auto value = var.to_object();
			std::cout << value.to_string() << std::endl;
			vnx::test::expect(value["field"].to<uint64_t>(), 123u);
			vnx::test::expect(value["field1"].to<std::string>(), "test");
			vnx::test::expect(value["field2"].to<std::vector<uint32_t>>(), std::vector<uint32_t>{11, 12, 13, 14});
		}
		vnx::test::expect(vm::read(engine, vm::MEM_STATIC + 11).to<std::string>(), "0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
	}
	VNX_TEST_END()

	vnx::test::exit();
}






