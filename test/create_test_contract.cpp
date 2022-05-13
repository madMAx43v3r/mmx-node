/*
 * create_test_contract.cpp
 *
 *  Created on: May 11, 2022
 *      Author: mad
 */

#include <mmx/vm/var_t.h>
#include <mmx/vm/instr_t.h>
#include <mmx/vm/varptr_t.hpp>

#include <mmx/contract/Executable.hxx>

using namespace mmx::vm;


int main(int argc, char** argv)
{
	auto exec = mmx::contract::Executable::create();

	std::vector<varptr_t> constant;
	constant.push_back(new var_t());
	constant.push_back(new var_t(TYPE_TRUE));
	constant.push_back(new var_t(TYPE_FALSE));
	constant.push_back(new uint_t(0));
	constant.push_back(new uint_t(1));					// 0x4
	constant.push_back(new array_t());
	constant.push_back(new map_t());
	constant.push_back(new uint_t(1337));
	constant.push_back(binary_t::alloc("test"));		// 0x8

	exec->fields["array"] = MEM_STATIC + 0;
	exec->fields["map"] = MEM_STATIC + 1;
	exec->fields["value"] = MEM_STATIC + 2;
	exec->fields["owner"] = MEM_STATIC + 3;

	std::vector<instr_t> code;
	{
		mmx::contract::method_t method;
		method.name = "init";
		method.args = {"owner"};
		method.entry_point = code.size();
		code.emplace_back(OP_COPY, 0, MEM_STATIC + 0, 0x5);
		code.emplace_back(OP_COPY, 0, MEM_STATIC + 1, 0x6);
		code.emplace_back(OP_COPY, 0, MEM_STATIC + 2, 0x7);
		code.emplace_back(OP_CONV, 0, MEM_STATIC + 3, MEM_STACK + 1, CONVTYPE_UINT, CONVTYPE_ADDRESS);
		code.emplace_back(OP_PUSH_BACK, 0, MEM_STATIC + 0, 0x7);
		code.emplace_back(OP_PUSH_BACK, 0, MEM_STATIC + 0, 0x8);
		code.emplace_back(OP_SET, 0, MEM_STATIC + 1, 0x8, 0x7);
		code.emplace_back(OP_CLONE, 0, MEM_STACK + 2, 0x6);
		code.emplace_back(OP_SET, 0, MEM_STACK + 2, 0x7, 0x8);
		code.emplace_back(OP_SET, 0, MEM_STATIC + 1, 0x7, MEM_STACK + 2);
		code.emplace_back(OP_RET);
		exec->methods["init"] = method;
	}
	while(code.size() < 50) {
		code.emplace_back(OP_NOP);
	}
	{
		mmx::contract::method_t method;
		method.name = "push_back";
		method.args = {"value"};
		method.is_public = true;
		method.entry_point = code.size();
		code.emplace_back(OP_PUSH_BACK, 0, MEM_STATIC + 0, MEM_STACK + 1);
		code.emplace_back(OP_RET);
		exec->methods["push_back"] = method;
	}
	while(code.size() < 60) {
		code.emplace_back(OP_NOP);
	}
	{
		mmx::contract::method_t method;
		method.name = "insert";
		method.args = {"key", "value"};
		method.is_public = true;
		method.entry_point = code.size();
		code.emplace_back(OP_SET, 0, MEM_STATIC + 1, MEM_STACK + 1, MEM_STACK + 2);
		code.emplace_back(OP_RET);
		exec->methods["insert"] = method;
	}
	while(code.size() < 70) {
		code.emplace_back(OP_NOP);
	}
	{
		mmx::contract::method_t method;
		method.name = "set_value";
		method.args = {"value"};
		method.is_public = true;
		method.entry_point = code.size();
		code.emplace_back(OP_COPY, 0, MEM_STATIC + 2, MEM_STACK + 1);
		code.emplace_back(OP_RET);
		exec->methods["set_value"] = method;
	}
	while(code.size() < 80) {
		code.emplace_back(OP_NOP);
	}
	{
		mmx::contract::method_t method;
		method.name = "read_array";
		method.args = {"index"};
		method.is_const = true;
		method.is_public = true;
		method.entry_point = code.size();
		code.emplace_back(OP_GET, 0, MEM_STACK + 0, MEM_STATIC + 0, MEM_STACK + 1);
		code.emplace_back(OP_RET);
		exec->methods["read_array"] = method;
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
	exec->symbol = "XYZ";
	exec->name = "XYZ Coin";
	exec->init_method = "init";
	exec->init_args.emplace_back("mmx1nn8u9etvnghq7x8atj2y55he76z9yvxalc9t3nx8ym0xqr4yuzvsdf8jp8");

	vnx::write_to_file(argc > 1 ? argv[1] : "contract.dat", exec);

	return 0;
}





