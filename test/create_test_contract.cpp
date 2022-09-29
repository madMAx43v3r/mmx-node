/*
 * create_test_contract.cpp
 *
 *  Created on: May 11, 2022
 *      Author: mad
 */

#include <mmx/vm/var_t.h>
#include <mmx/vm/instr_t.h>
#include <mmx/vm/varptr_t.hpp>
#include <mmx/vm/Engine.h>

#include <mmx/contract/Binary.hxx>
#include <mmx/contract/Executable.hxx>

using namespace mmx;


int main(int argc, char** argv)
{
	auto bin = mmx::contract::Binary::create();
	auto exec = mmx::contract::Executable::create();

	std::vector<vm::varptr_t> constant;
	constant.push_back(new vm::var_t());
	constant.push_back(new vm::var_t(vm::TYPE_TRUE));
	constant.push_back(new vm::var_t(vm::TYPE_FALSE));
	constant.push_back(new vm::uint_t(0));
	constant.push_back(new vm::uint_t(1));					// 0x4
	constant.push_back(new vm::array_t());
	constant.push_back(new vm::map_t());
	constant.push_back(new vm::uint_t(1337));
	constant.push_back(vm::binary_t::alloc("test"));		// 0x8
	constant.push_back(vm::binary_t::alloc("invalid user"));
	constant.push_back(vm::binary_t::alloc("other"));
	constant.push_back(vm::binary_t::alloc("set_value"));

	bin->fields["array"] = vm::MEM_STATIC + 0;
	bin->fields["map"] = vm::MEM_STATIC + 1;
	bin->fields["value"] = vm::MEM_STATIC + 2;
	bin->fields["owner"] = vm::MEM_STATIC + 3;

	std::vector<vm::instr_t> code;
	{
		mmx::contract::method_t method;
		method.name = "init";
		method.args = {"owner"};
		method.entry_point = code.size();
		code.emplace_back(vm::OP_COPY, 0, vm::MEM_STATIC + 0, 0x5);
		code.emplace_back(vm::OP_COPY, 0, vm::MEM_STATIC + 1, 0x6);
		code.emplace_back(vm::OP_COPY, 0, vm::MEM_STATIC + 2, 0x7);
		code.emplace_back(vm::OP_CONV, 0, vm::MEM_STATIC + 3, vm::MEM_STACK + 1, vm::CONVTYPE_UINT, vm::CONVTYPE_ADDRESS);
		code.emplace_back(vm::OP_PUSH_BACK, 0, vm::MEM_STATIC + 0, 0x7);
		code.emplace_back(vm::OP_PUSH_BACK, 0, vm::MEM_STATIC + 0, 0x8);
		code.emplace_back(vm::OP_SET, 0, vm::MEM_STATIC + 1, 0x8, 0x7);
		code.emplace_back(vm::OP_CLONE, 0, vm::MEM_STACK + 2, 0x6);
		code.emplace_back(vm::OP_SET, 0, vm::MEM_STACK + 2, 0x7, 0x8);
		code.emplace_back(vm::OP_SET, 0, vm::MEM_STATIC + 1, 0x7, vm::MEM_STACK + 2);
		code.emplace_back(vm::OP_RET);
		bin->methods["init"] = method;
	}
	{
		mmx::contract::method_t method;
		method.name = "push_back";
		method.args = {"value"};
		method.is_public = true;
		method.entry_point = code.size();
		code.emplace_back(vm::OP_PUSH_BACK, 0, vm::MEM_STATIC + 0, vm::MEM_STACK + 1);
		code.emplace_back(vm::OP_RET);
		bin->methods["push_back"] = method;
	}
	{
		mmx::contract::method_t method;
		method.name = "insert";
		method.args = {"key", "value"};
		method.is_public = true;
		method.entry_point = code.size();
		code.emplace_back(vm::OP_SET, 0, vm::MEM_STATIC + 1, vm::MEM_STACK + 1, vm::MEM_STACK + 2);
		code.emplace_back(vm::OP_RET);
		bin->methods["insert"] = method;
	}
	{
		mmx::contract::method_t method;
		method.name = "set_value";
		method.args = {"value"};
		method.is_public = true;
		method.entry_point = code.size();
		code.emplace_back(vm::OP_COPY, 0, vm::MEM_STATIC + 2, vm::MEM_STACK + 1);
		code.emplace_back(vm::OP_RET);
		bin->methods["set_value"] = method;
	}
	{
		mmx::contract::method_t method;
		method.name = "read_array";
		method.args = {"index"};
		method.is_const = true;
		method.is_public = true;
		method.entry_point = code.size();
		code.emplace_back(vm::OP_GET, 0, vm::MEM_STACK + 0, vm::MEM_STATIC + 0, vm::MEM_STACK + 1);
		code.emplace_back(vm::OP_RET);
		bin->methods["read_array"] = method;
	}
	{
		mmx::contract::method_t method;
		method.name = "mint";
		method.args = {"address", "amount"};
		method.is_public = true;
		method.entry_point = code.size();
		code.emplace_back(vm::OP_CONV, 0, vm::MEM_STACK + 3, vm::MEM_STACK + 1, vm::CONVTYPE_UINT, vm::CONVTYPE_ADDRESS);
		code.emplace_back(vm::OP_MINT, 0, vm::MEM_STACK + 3, vm::MEM_STACK + 2);
		code.emplace_back(vm::OP_RET);
		bin->methods["mint"] = method;
	}
	{
		mmx::contract::method_t method;
		method.name = "deposit";
		method.is_public = true;
		method.is_payable = true;
		method.entry_point = code.size();
		code.emplace_back(vm::OP_RET);
		bin->methods["deposit"] = method;
	}
	{
		mmx::contract::method_t method;
		method.name = "withdraw";
		method.is_public = true;
		method.args = {"address", "amount", "currency"};
		method.entry_point = code.size();
		code.emplace_back(vm::OP_CMP_EQ, 0, vm::MEM_STACK + 10, vm::MEM_EXTERN + vm::EXTERN_USER, vm::MEM_STATIC + 3);
		code.emplace_back(vm::OP_JUMPI, 0, method.entry_point + 3, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_FAIL, 0, vm::MEM_CONST + 9);
		code.emplace_back(vm::OP_CONV, 0, vm::MEM_STACK + 4, vm::MEM_STACK + 1, vm::CONVTYPE_UINT, vm::CONVTYPE_ADDRESS);
		code.emplace_back(vm::OP_CONV, 0, vm::MEM_STACK + 5, vm::MEM_STACK + 3, vm::CONVTYPE_UINT, vm::CONVTYPE_ADDRESS);
		code.emplace_back(vm::OP_SEND, 0, vm::MEM_STACK + 4, vm::MEM_STACK + 2, vm::MEM_STACK + 5);
		code.emplace_back(vm::OP_RET);
		bin->methods["withdraw"] = method;
	}
	{
		mmx::contract::method_t method;
		method.name = "cross_call";
		method.is_public = true;
		method.args = {"value"};
		method.entry_point = code.size();
		code.emplace_back(vm::OP_COPY, 0, vm::MEM_STACK + 3, vm::MEM_STACK + 1);
		code.emplace_back(vm::OP_RCALL, 0, vm::MEM_CONST + 10, vm::MEM_CONST + 11, 2, 1);
		code.emplace_back(vm::OP_RET);
		bin->methods["cross_call"] = method;
	}

	for(const auto& var : constant) {
		auto data = serialize(*var.get(), false, false);
		bin->constant.insert(bin->constant.end(), data.first, data.first + data.second);
		::free(data.first);
	}
	{
		auto data = serialize(code);
		bin->binary = std::vector<uint8_t>(data.first, data.first + data.second);
		::free(data.first);
	}
	exec->symbol = "XYZ";
	exec->name = "XYZ Coin";
	exec->binary = std::string("...");
	exec->depends["other"] = std::string("mmx1q0rs30csp2pgy4v597gtcdu8paevjnuf9pzkcrhfma8c7kaxlztspdaak9");
	exec->init_method = "init";
	exec->init_args.emplace_back("mmx1nn8u9etvnghq7x8atj2y55he76z9yvxalc9t3nx8ym0xqr4yuzvsdf8jp8");

	vnx::write_to_file(argc > 1 ? argv[1] : "binary.dat", bin);
	vnx::write_to_file(argc > 2 ? argv[2] : "executable.dat", exec);

	return 0;
}





