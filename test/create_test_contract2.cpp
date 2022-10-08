/*
 * create_test_contract2.cpp
 *
 *  Created on: May 21, 2022
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
	constant.push_back(std::make_unique<vm::var_t>());
	constant.push_back(std::make_unique<vm::var_t>(true));
	constant.push_back(std::make_unique<vm::var_t>(false));
	constant.push_back(std::make_unique<vm::uint_t>());
	constant.push_back(std::make_unique<vm::uint_t>(1));					// [4]
	constant.push_back(std::make_unique<vm::array_t>());
	constant.push_back(std::make_unique<vm::map_t>());
	constant.push_back(std::make_unique<vm::uint_t>(100000));				// price change per height
	constant.push_back(std::make_unique<vm::uint_t>(1000000000));			// initial price values [8]
	constant.push_back(vm::binary_t::alloc("invalid token"));

	bin->fields["price"] = vm::MEM_STATIC + 0;
	bin->fields["inv_price"] = vm::MEM_STATIC + 1;
	bin->fields["token"] = vm::MEM_STATIC + 2;
	bin->fields["last_height"] = vm::MEM_STATIC + 3;

	std::vector<vm::instr_t> code;
	{
		mmx::contract::method_t method;
		method.name = "init";
		method.args = {"token"};
		method.entry_point = code.size();
		code.emplace_back(vm::OP_COPY, 0, vm::MEM_STATIC + 0, 8);
		code.emplace_back(vm::OP_COPY, 0, vm::MEM_STATIC + 1, 8);
		code.emplace_back(vm::OP_CONV, 0, vm::MEM_STATIC + 2, vm::MEM_STACK + 1, vm::CONVTYPE_UINT, vm::CONVTYPE_ADDRESS);
		code.emplace_back(vm::OP_COPY, 0, vm::MEM_STATIC + 3, vm::MEM_EXTERN + vm::EXTERN_HEIGHT);
		code.emplace_back(vm::OP_RET);
		bin->methods["init"] = method;
	}
	{
		mmx::contract::method_t method;
		method.name = "update";
		method.is_public = true;
		method.entry_point = code.size();
		code.emplace_back(vm::OP_SUB, vm::OPFLAG_CATCH_OVERFLOW, vm::MEM_STACK + 1, vm::MEM_EXTERN + vm::EXTERN_HEIGHT, vm::MEM_STATIC + 3);
		code.emplace_back(vm::OP_MUL, vm::OPFLAG_CATCH_OVERFLOW, vm::MEM_STACK + 1, vm::MEM_STACK + 1, 7);
		code.emplace_back(vm::OP_ADD, vm::OPFLAG_CATCH_OVERFLOW, vm::MEM_STATIC + 1, vm::MEM_STATIC + 1, vm::MEM_STACK + 1);
		code.emplace_back(vm::OP_COPY, 0, vm::MEM_STATIC + 3, vm::MEM_EXTERN + vm::EXTERN_HEIGHT);
		code.emplace_back(vm::OP_RET);
		bin->methods["update"] = method;
	}
	{
		mmx::contract::method_t method;
		method.name = "mint";
		method.is_public = true;
		method.is_payable = true;
		method.args = {"address"};
		method.entry_point = code.size();
		code.emplace_back(vm::OP_CONV, 0, vm::MEM_STACK + 1, vm::MEM_STACK + 1, vm::CONVTYPE_UINT, vm::CONVTYPE_ADDRESS);
		code.emplace_back(vm::OP_GET, 0, vm::MEM_STACK + 2, vm::MEM_EXTERN + vm::EXTERN_DEPOSIT, 3);
		code.emplace_back(vm::OP_CMP_EQ, 0, vm::MEM_STACK + 2, vm::MEM_STACK + 2, vm::MEM_STATIC + 2);
		code.emplace_back(vm::OP_JUMPI, 0, method.entry_point + 5, vm::MEM_STACK + 2);
		code.emplace_back(vm::OP_FAIL, 0, 9);
		code.emplace_back(vm::OP_CALL, 0, bin->methods["update"].entry_point, 2);
		code.emplace_back(vm::OP_GET, 0, vm::MEM_STACK + 2, vm::MEM_EXTERN + vm::EXTERN_DEPOSIT, 4);
		code.emplace_back(vm::OP_ADD, vm::OPFLAG_CATCH_OVERFLOW, vm::MEM_STATIC + 0, vm::MEM_STATIC + 0, vm::MEM_STACK + 2);
		code.emplace_back(vm::OP_MUL, vm::OPFLAG_CATCH_OVERFLOW, vm::MEM_STACK + 2, vm::MEM_STACK + 2, vm::MEM_STATIC + 1);
		code.emplace_back(vm::OP_DIV, vm::OPFLAG_CATCH_OVERFLOW, vm::MEM_STACK + 2, vm::MEM_STACK + 2, vm::MEM_STATIC + 0);
		code.emplace_back(vm::OP_MINT, 0, vm::MEM_STACK + 1, vm::MEM_STACK + 2);
		code.emplace_back(vm::OP_RET);
		bin->methods["mint"] = method;
	}
	{
		mmx::contract::method_t method;
		method.name = "burn";
		method.is_public = true;
		method.is_payable = true;
		method.args = {"address"};
		method.entry_point = code.size();
		code.emplace_back(vm::OP_CONV, 0, vm::MEM_STACK + 1, vm::MEM_STACK + 1, vm::CONVTYPE_UINT, vm::CONVTYPE_ADDRESS);
		code.emplace_back(vm::OP_GET, 0, vm::MEM_STACK + 2, vm::MEM_EXTERN + vm::EXTERN_DEPOSIT, 3);
		code.emplace_back(vm::OP_CMP_EQ, 0, vm::MEM_STACK + 2, vm::MEM_STACK + 2, vm::MEM_EXTERN + vm::EXTERN_ADDRESS);
		code.emplace_back(vm::OP_JUMPI, 0, method.entry_point + 5, vm::MEM_STACK + 2);
		code.emplace_back(vm::OP_FAIL, 0, 9);
		code.emplace_back(vm::OP_CALL, 0, bin->methods["update"].entry_point, 2);
		code.emplace_back(vm::OP_GET, 0, vm::MEM_STACK + 2, vm::MEM_EXTERN + vm::EXTERN_DEPOSIT, 4);
		code.emplace_back(vm::OP_ADD, vm::OPFLAG_CATCH_OVERFLOW, vm::MEM_STATIC + 1, vm::MEM_STATIC + 1, vm::MEM_STACK + 2);
		code.emplace_back(vm::OP_MUL, vm::OPFLAG_CATCH_OVERFLOW, vm::MEM_STACK + 2, vm::MEM_STACK + 2, vm::MEM_STATIC + 0);
		code.emplace_back(vm::OP_DIV, vm::OPFLAG_CATCH_OVERFLOW, vm::MEM_STACK + 2, vm::MEM_STACK + 2, vm::MEM_STATIC + 1);
		code.emplace_back(vm::OP_SEND, 0, vm::MEM_STACK + 1, vm::MEM_STACK + 2, vm::MEM_STATIC + 2);
		code.emplace_back(vm::OP_RET);
		bin->methods["burn"] = method;
	}

	for(const auto& var : constant) {
		const auto data = serialize(*var.get(), false, false);
		bin->constant.insert(bin->constant.end(), data.first.get(), data.first.get() + data.second);
	}
	{
		const auto data = serialize(code);
		bin->binary = std::vector<uint8_t>(data.first.get(), data.first.get() + data.second);
	}
	exec->symbol = "MMT";
	exec->name = "MMT Algo Token";
	exec->binary = std::string("...");
	exec->init_method = "init";
	exec->init_args.emplace_back("mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev");

	vnx::write_to_file(argc > 1 ? argv[1] : "binary.dat", bin);
	vnx::write_to_file(argc > 2 ? argv[2] : "executable.dat", exec);

	return 0;
}





