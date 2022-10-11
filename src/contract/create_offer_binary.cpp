/*
 * create_offer_binary.cpp
 *
 *  Created on: Sep 19, 2022
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

	const_map["one"] = constant.size();
	constant.push_back(std::make_unique<vm::uint_t>(1));

	const_map["TYPE_UINT"] = constant.size();
	constant.push_back(std::make_unique<vm::uint_t>(uint32_t(vm::TYPE_UINT)));

	const_map["INIT"] = constant.size();
	constant.push_back(vm::binary_t::alloc("INIT"));

	const_map["OPEN"] = constant.size();
	constant.push_back(vm::binary_t::alloc("OPEN"));

	const_map["CLOSED"] = constant.size();
	constant.push_back(vm::binary_t::alloc("CLOSED"));

	const_map["REVOKED"] = constant.size();
	constant.push_back(vm::binary_t::alloc("REVOKED"));

	const_map["fail_not_init"] = constant.size();
	constant.push_back(vm::binary_t::alloc("state != INIT"));

	const_map["fail_not_open"] = constant.size();
	constant.push_back(vm::binary_t::alloc("state != OPEN"));

	const_map["fail_owner"] = constant.size();
	constant.push_back(vm::binary_t::alloc("user != owner"));

	const_map["fail_partner"] = constant.size();
	constant.push_back(vm::binary_t::alloc("user != partner"));

	const_map["fail_ask_amount_type"] = constant.size();
	constant.push_back(vm::binary_t::alloc("ask_amount != TYPE_UINT"));

	const_map["fail_ask_amount_value"] = constant.size();
	constant.push_back(vm::binary_t::alloc("ask_amount <= 0"));

	const_map["fail_amount"] = constant.size();
	constant.push_back(vm::binary_t::alloc("amount mismatch"));

	const_map["fail_currency"] = constant.size();
	constant.push_back(vm::binary_t::alloc("currency mismatch"));

	{
		size_t off = 1;
		bin->fields["owner"] = vm::MEM_STATIC + (off++);
		bin->fields["partner"] = vm::MEM_STATIC + (off++);
		bin->fields["bid_currency"] = vm::MEM_STATIC + (off++);
		bin->fields["bid_amount"] = vm::MEM_STATIC + (off++);
		bin->fields["ask_currency"] = vm::MEM_STATIC + (off++);
		bin->fields["ask_amount"] = vm::MEM_STATIC + (off++);
		bin->fields["state"] = vm::MEM_STATIC + (off++);
		bin->fields["height_open"] = vm::MEM_STATIC + (off++);	// TODO: obsolete
		bin->fields["height_close"] = vm::MEM_STATIC + (off++);	// TODO: obsolete
		bin->fields["trade_txid"] = vm::MEM_STATIC + (off++);	// TODO: txid_close (for cancel too)
	}

	std::vector<vm::instr_t> code;
	{
		mmx::contract::method_t method;
		method.name = "init";
		method.args = {"owner", "ask_currency", "ask_amount", "partner"};
		method.entry_point = code.size();
		code.emplace_back(vm::OP_TYPE, 0, vm::MEM_STACK + 11, vm::MEM_STACK + 3);
		code.emplace_back(vm::OP_CMP_EQ, 0, vm::MEM_STACK + 10, vm::MEM_STACK + 11, const_map["TYPE_UINT"]);
		code.emplace_back(vm::OP_JUMPI, 0, code.size() + 2, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_FAIL, 0, const_map["fail_ask_amount_type"]);
		code.emplace_back(vm::OP_CMP_GT, 0, vm::MEM_STACK + 10, vm::MEM_STACK + 3, const_map["zero"]);
		code.emplace_back(vm::OP_JUMPI, 0, code.size() + 2, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_FAIL, 0, const_map["fail_ask_amount_value"]);
		code.emplace_back(vm::OP_CONV, 0, bin->fields["owner"], vm::MEM_STACK + 1, vm::CONVTYPE_UINT, vm::CONVTYPE_ADDRESS);
		code.emplace_back(vm::OP_CONV, 0, bin->fields["ask_currency"], vm::MEM_STACK + 2, vm::CONVTYPE_UINT, vm::CONVTYPE_ADDRESS);
		code.emplace_back(vm::OP_COPY, 0, bin->fields["ask_amount"], vm::MEM_STACK + 3);
		code.emplace_back(vm::OP_CMP_EQ, 0, vm::MEM_STACK + 10, vm::MEM_STACK + 4, const_map["null"]);
		code.emplace_back(vm::OP_JUMPI, 0, code.size() + 3, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_CONV, 0, bin->fields["partner"], vm::MEM_STACK + 4, vm::CONVTYPE_UINT, vm::CONVTYPE_ADDRESS);
		code.emplace_back(vm::OP_JUMP, 0, code.size() + 2);
		code.emplace_back(vm::OP_COPY, 0, bin->fields["partner"], const_map["null"]);
		code.emplace_back(vm::OP_COPY, 0, bin->fields["state"], const_map["INIT"]);
		code.emplace_back(vm::OP_RET);
		bin->methods[method.name] = method;
	}
	{
		mmx::contract::method_t method;
		method.name = "open";
		method.is_public = true;
		method.is_payable = true;
		method.entry_point = code.size();
		code.emplace_back(vm::OP_CMP_EQ, 0, vm::MEM_STACK + 10, vm::MEM_EXTERN + vm::EXTERN_USER, bin->fields["owner"]);
		code.emplace_back(vm::OP_JUMPI, 0, code.size() + 2, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_FAIL, 0, const_map["fail_owner"]);
		code.emplace_back(vm::OP_CMP_EQ, 0, vm::MEM_STACK + 10, bin->fields["state"], const_map["INIT"]);
		code.emplace_back(vm::OP_JUMPI, 0, code.size() + 2, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_FAIL, 0, const_map["fail_not_init"]);
		code.emplace_back(vm::OP_GET, 0, vm::MEM_STACK + 11, vm::MEM_EXTERN + vm::EXTERN_DEPOSIT, const_map["zero"]);
		code.emplace_back(vm::OP_COPY, 0, bin->fields["bid_currency"], vm::MEM_STACK + 11);
		code.emplace_back(vm::OP_GET, 0, vm::MEM_STACK + 11, vm::MEM_EXTERN + vm::EXTERN_DEPOSIT, const_map["one"]);
		code.emplace_back(vm::OP_COPY, 0, bin->fields["bid_amount"], vm::MEM_STACK + 11);
		code.emplace_back(vm::OP_COPY, 0, bin->fields["state"], const_map["OPEN"]);
		code.emplace_back(vm::OP_COPY, 0, bin->fields["height_open"], vm::MEM_EXTERN + vm::EXTERN_HEIGHT);
		code.emplace_back(vm::OP_RET);
		bin->methods[method.name] = method;
	}
	{
		mmx::contract::method_t method;
		method.name = "cancel";
		method.is_public = true;
		method.entry_point = code.size();
		code.emplace_back(vm::OP_CMP_EQ, 0, vm::MEM_STACK + 10, vm::MEM_EXTERN + vm::EXTERN_USER, bin->fields["owner"]);
		code.emplace_back(vm::OP_JUMPI, 0, code.size() + 2, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_FAIL, 0, const_map["fail_owner"]);
		code.emplace_back(vm::OP_CMP_EQ, 0, vm::MEM_STACK + 10, bin->fields["state"], const_map["OPEN"]);
		code.emplace_back(vm::OP_JUMPI, 0, code.size() + 2, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_FAIL, 0, const_map["fail_not_open"]);
		code.emplace_back(vm::OP_SEND, 0, bin->fields["owner"], bin->fields["bid_amount"], bin->fields["bid_currency"]);
		code.emplace_back(vm::OP_COPY, 0, bin->fields["state"], const_map["REVOKED"]);
		code.emplace_back(vm::OP_COPY, 0, bin->fields["height_close"], vm::MEM_EXTERN + vm::EXTERN_HEIGHT);
		code.emplace_back(vm::OP_RET);
		bin->methods[method.name] = method;
	}
	{
		mmx::contract::method_t method;
		method.name = "trade";
		method.args = {"dst_addr"};
		method.is_public = true;
		method.is_payable = true;
		method.entry_point = code.size();
		code.emplace_back(vm::OP_CMP_EQ, 0, vm::MEM_STACK + 10, bin->fields["state"], const_map["OPEN"]);
		code.emplace_back(vm::OP_JUMPI, 0, code.size() + 2, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_FAIL, 0, const_map["fail_not_open"]);
		code.emplace_back(vm::OP_CMP_EQ, 0, vm::MEM_STACK + 10, bin->fields["partner"], const_map["null"]);
		code.emplace_back(vm::OP_JUMPI, 0, code.size() + 4, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_CMP_EQ, 0, vm::MEM_STACK + 10, bin->fields["partner"], vm::MEM_EXTERN + vm::EXTERN_USER);
		code.emplace_back(vm::OP_JUMPI, 0, code.size() + 2, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_FAIL, 0, const_map["fail_partner"]);
		code.emplace_back(vm::OP_GET, 0, vm::MEM_STACK + 11, vm::MEM_EXTERN + vm::EXTERN_DEPOSIT, const_map["zero"]);
		code.emplace_back(vm::OP_CMP_EQ, 0, vm::MEM_STACK + 10, vm::MEM_STACK + 11, bin->fields["ask_currency"]);
		code.emplace_back(vm::OP_JUMPI, 0, code.size() + 2, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_FAIL, 0, const_map["fail_currency"]);
		code.emplace_back(vm::OP_GET, 0, vm::MEM_STACK + 11, vm::MEM_EXTERN + vm::EXTERN_DEPOSIT, const_map["one"]);
		code.emplace_back(vm::OP_CMP_EQ, 0, vm::MEM_STACK + 10, vm::MEM_STACK + 11, bin->fields["ask_amount"]);
		code.emplace_back(vm::OP_JUMPI, 0, code.size() + 2, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_FAIL, 0, const_map["fail_amount"]);
		code.emplace_back(vm::OP_CONV, 0, vm::MEM_STACK + 11, vm::MEM_STACK + 1, vm::CONVTYPE_UINT, vm::CONVTYPE_ADDRESS);
		code.emplace_back(vm::OP_SEND, 0, vm::MEM_STACK + 11, bin->fields["bid_amount"], bin->fields["bid_currency"]);
		code.emplace_back(vm::OP_SEND, 0, bin->fields["owner"], bin->fields["ask_amount"], bin->fields["ask_currency"]);
		code.emplace_back(vm::OP_COPY, 0, bin->fields["state"], const_map["CLOSED"]);
		code.emplace_back(vm::OP_COPY, 0, bin->fields["height_close"], vm::MEM_EXTERN + vm::EXTERN_HEIGHT);
		code.emplace_back(vm::OP_COPY, 0, bin->fields["trade_txid"], vm::MEM_EXTERN + vm::EXTERN_TXID);
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
	vnx::write_to_file(argc > 1 ? argv[1] : "offer_binary.dat", bin);

	return 0;
}


