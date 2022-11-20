/*
 * create_plot_binary.cpp
 *
 *  Created on: Nov 5, 2022
 *      Author: mad
 */


#include <mmx/vm/var_t.h>
#include <mmx/vm/instr_t.h>
#include <mmx/vm/varptr_t.hpp>
#include <mmx/vm/Engine.h>

#include <mmx/contract/Binary.hxx>

using namespace mmx;


int main(int argc, char** argv)
{
	auto bin = mmx::contract::Binary::create();
	bin->name = "mmx/offer";

	std::vector<vm::varptr_t> constant;
	std::map<std::string, size_t> const_map;

	const_map["null"] = constant.size();
	constant.push_back(std::make_unique<vm::var_t>());

	const_map["zero"] = constant.size();
	constant.push_back(std::make_unique<vm::uint_t>(0));

	const_map["100"] = constant.size();
	constant.push_back(std::make_unique<vm::uint_t>(100));

	const_map["WITHDRAW_FACTOR"] = constant.size();
	constant.push_back(std::make_unique<vm::uint_t>(90));

	const_map["fail_owner"] = constant.size();
	constant.push_back(vm::binary_t::alloc("user != owner"));

	{
		size_t off = 1;
		bin->fields["owner"] = vm::MEM_STATIC + (off++);
	}

	std::vector<vm::instr_t> code;
	{
		mmx::contract::method_t method;
		method.name = "init";
		method.args = {"owner"};
		method.entry_point = code.size();
		code.emplace_back(vm::OP_CONV, 0, bin->fields["owner"], vm::MEM_STACK + 1, vm::CONVTYPE_UINT, vm::CONVTYPE_ADDRESS);
		code.emplace_back(vm::OP_RET);
		bin->methods[method.name] = method;
	}
	{
		mmx::contract::method_t method;
		method.name = "withdraw";
		method.args = {"amount"};
		method.is_public = true;
		method.entry_point = code.size();
		code.emplace_back(vm::OP_CMP_EQ, 0, vm::MEM_STACK + 10, vm::MEM_EXTERN + vm::EXTERN_USER, bin->fields["owner"]);
		code.emplace_back(vm::OP_JUMPI, 0, code.size() + 2, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_FAIL, 0, const_map["fail_owner"]);
		code.emplace_back(vm::OP_MUL, vm::OPFLAG_CATCH_OVERFLOW, vm::MEM_STACK + 11, vm::MEM_STACK + 1, const_map["WITHDRAW_FACTOR"]);
		code.emplace_back(vm::OP_DIV, 0, vm::MEM_STACK + 11, vm::MEM_STACK + 11, const_map["100"]);
		code.emplace_back(vm::OP_SUB, vm::OPFLAG_CATCH_OVERFLOW, vm::MEM_STACK + 12, vm::MEM_STACK + 1, vm::MEM_STACK + 11);
		code.emplace_back(vm::OP_SEND, 0, bin->fields["owner"], vm::MEM_STACK + 11, const_map["zero"]);
		code.emplace_back(vm::OP_SEND, 0, const_map["zero"], vm::MEM_STACK + 12, const_map["zero"]);
		code.emplace_back(vm::OP_RET);
		bin->methods[method.name] = method;
	}

	for(const auto& var : constant) {
		const auto data = serialize(*var.get(), false, false);
		bin->constant.insert(bin->constant.end(), data.first.get(), data.first.get() + data.second);
	}
	{
		const auto data = vm::serialize(code);
		bin->binary = std::vector<uint8_t>(data.first.get(), data.first.get() + data.second);

		std::vector<vm::instr_t> test;
		auto length = vm::deserialize(test, data.first.get(), data.second);
		if(length != data.second) {
			throw std::logic_error("length != data.second");
		}
		if(test.size() != code.size()) {
			throw std::logic_error("test.size() != code.size()");
		}
		for(size_t i = 0; i < test.size(); ++i) {
			if(test[i].code != code[i].code) {
				throw std::logic_error("test[i].code != code[i].code");
			}
		}
	}
	vnx::write_to_file(argc > 1 ? argv[1] : "plot_binary.dat", bin);

	return 0;
}




