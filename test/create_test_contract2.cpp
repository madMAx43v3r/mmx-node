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

#include <mmx/contract/Executable.hxx>

using namespace mmx::vm;


int main(int argc, char** argv)
{
	auto exec = mmx::contract::Executable::create();

	std::vector<varptr_t> constant;
	constant.push_back(new var_t());
	constant.push_back(new var_t(true));
	constant.push_back(new var_t(false));
	constant.push_back(new uint_t());
	constant.push_back(new uint_t(1));					// [4]
	constant.push_back(new array_t());
	constant.push_back(new map_t());
	constant.push_back(new uint_t(100000));				// price change per height
	constant.push_back(new uint_t(1000000000));			// initial price values [8]
	constant.push_back(binary_t::alloc("invalid token"));

	exec->fields["price"] = MEM_STATIC + 0;
	exec->fields["inv_price"] = MEM_STATIC + 1;
	exec->fields["token"] = MEM_STATIC + 2;
	exec->fields["last_height"] = MEM_STATIC + 3;

	std::vector<instr_t> code;
	{
		mmx::contract::method_t method;
		method.name = "init";
		method.args = {"token"};
		method.entry_point = code.size();
		code.emplace_back(OP_COPY, 0, MEM_STATIC + 0, 8);
		code.emplace_back(OP_COPY, 0, MEM_STATIC + 1, 8);
		code.emplace_back(OP_CONV, 0, MEM_STATIC + 2, MEM_STACK + 1, CONVTYPE_UINT, CONVTYPE_ADDRESS);
		code.emplace_back(OP_COPY, 0, MEM_STATIC + 3, MEM_EXTERN + EXTERN_HEIGHT);
		code.emplace_back(OP_RET);
		exec->methods["init"] = method;
	}
	{
		mmx::contract::method_t method;
		method.name = "update";
		method.is_public = true;
		method.entry_point = code.size();
		code.emplace_back(OP_SUB, OPFLAG_CATCH_OVERFLOW, MEM_STACK + 1, MEM_EXTERN + EXTERN_HEIGHT, MEM_STATIC + 3);
		code.emplace_back(OP_MUL, OPFLAG_CATCH_OVERFLOW, MEM_STACK + 1, MEM_STACK + 1, 7);
		code.emplace_back(OP_ADD, OPFLAG_CATCH_OVERFLOW, MEM_STATIC + 1, MEM_STATIC + 1, MEM_STACK + 1);
		code.emplace_back(OP_COPY, 0, MEM_STATIC + 3, MEM_EXTERN + EXTERN_HEIGHT);
		code.emplace_back(OP_RET);
		exec->methods["update"] = method;
	}
	{
		mmx::contract::method_t method;
		method.name = "mint";
		method.is_public = true;
		method.is_payable = true;
		method.args = {"address"};
		method.entry_point = code.size();
		code.emplace_back(OP_CONV, 0, MEM_STACK + 1, MEM_STACK + 1, CONVTYPE_UINT, CONVTYPE_ADDRESS);
		code.emplace_back(OP_GET, 0, MEM_STACK + 2, MEM_EXTERN + EXTERN_DEPOSIT, 3);
		code.emplace_back(OP_CMP_EQ, 0, MEM_STACK + 2, MEM_STACK + 2, MEM_STATIC + 2);
		code.emplace_back(OP_JUMPI, 0, method.entry_point + 5, MEM_STACK + 2);
		code.emplace_back(OP_FAIL, 0, 9);
		code.emplace_back(OP_CALL, 0, exec->methods["update"].entry_point, 2);
		code.emplace_back(OP_GET, 0, MEM_STACK + 2, MEM_EXTERN + EXTERN_DEPOSIT, 4);
		code.emplace_back(OP_ADD, OPFLAG_CATCH_OVERFLOW, MEM_STATIC + 0, MEM_STATIC + 0, MEM_STACK + 2);
		code.emplace_back(OP_MUL, OPFLAG_CATCH_OVERFLOW, MEM_STACK + 2, MEM_STACK + 2, MEM_STATIC + 1);
		code.emplace_back(OP_DIV, OPFLAG_CATCH_OVERFLOW, MEM_STACK + 2, MEM_STACK + 2, MEM_STATIC + 0);
		code.emplace_back(OP_MINT, 0, MEM_STACK + 1, MEM_STACK + 2);
		code.emplace_back(OP_RET);
		exec->methods["mint"] = method;
	}
	{
		mmx::contract::method_t method;
		method.name = "burn";
		method.is_public = true;
		method.is_payable = true;
		method.args = {"address"};
		method.entry_point = code.size();
		code.emplace_back(OP_CONV, 0, MEM_STACK + 1, MEM_STACK + 1, CONVTYPE_UINT, CONVTYPE_ADDRESS);
		code.emplace_back(OP_GET, 0, MEM_STACK + 2, MEM_EXTERN + EXTERN_DEPOSIT, 3);
		code.emplace_back(OP_CMP_EQ, 0, MEM_STACK + 2, MEM_STACK + 2, MEM_EXTERN + EXTERN_ADDRESS);
		code.emplace_back(OP_JUMPI, 0, method.entry_point + 5, MEM_STACK + 2);
		code.emplace_back(OP_FAIL, 0, 9);
		code.emplace_back(OP_CALL, 0, exec->methods["update"].entry_point, 2);
		code.emplace_back(OP_GET, 0, MEM_STACK + 2, MEM_EXTERN + EXTERN_DEPOSIT, 4);
		code.emplace_back(OP_ADD, OPFLAG_CATCH_OVERFLOW, MEM_STATIC + 1, MEM_STATIC + 1, MEM_STACK + 2);
		code.emplace_back(OP_MUL, OPFLAG_CATCH_OVERFLOW, MEM_STACK + 2, MEM_STACK + 2, MEM_STATIC + 0);
		code.emplace_back(OP_DIV, OPFLAG_CATCH_OVERFLOW, MEM_STACK + 2, MEM_STACK + 2, MEM_STATIC + 1);
		code.emplace_back(OP_SEND, 0, MEM_STACK + 1, MEM_STACK + 2, MEM_STATIC + 2);
		code.emplace_back(OP_RET);
		exec->methods["burn"] = method;
	}

	for(const auto& var : constant) {
		auto data = serialize(*var.get(), false, false);
		exec->constant.insert(exec->constant.end(), data.first, data.first + data.second);
		::free(data.first);
	}
	{
		auto data = serialize(code);
		exec->binary = std::vector<uint8_t>(data.first, data.first + data.second);
		::free(data.first);
	}
	exec->symbol = "MMT";
	exec->name = "MMT Algo Token";
	exec->init_method = "init";
	exec->init_args.emplace_back("mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev");

	vnx::write_to_file(argc > 1 ? argv[1] : "contract.dat", exec);

	return 0;
}





