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

#include <mmx/Transaction.hxx>
#include <mmx/contract/Binary.hxx>

static constexpr int FRACT_BITS = 64;

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

	const_map["fail_owner"] = constant.size();
	constant.push_back(vm::binary_t::alloc("user != owner"));

	const_map["fail_partner"] = constant.size();
	constant.push_back(vm::binary_t::alloc("user != partner"));

	const_map["fail_currency"] = constant.size();
	constant.push_back(vm::binary_t::alloc("currency mismatch"));

	const_map["fail_bid_amount"] = constant.size();
	constant.push_back(vm::binary_t::alloc("not enough bid amount left"));

	{
		size_t off = 1;
		bin->fields["owner"] = vm::MEM_STATIC + (off++);
		bin->fields["partner"] = vm::MEM_STATIC + (off++);
		bin->fields["bid_currency"] = vm::MEM_STATIC + (off++);
		bin->fields["ask_currency"] = vm::MEM_STATIC + (off++);
		bin->fields["inv_price"] = vm::MEM_STATIC + (off++);
	}

	std::vector<vm::instr_t> code;
	{
		mmx::contract::method_t method;
		method.name = "init";
		method.args = {"owner", "bid_currency", "ask_currency", "inv_price", "partner"};
		method.entry_point = code.size();
		code.emplace_back(vm::OP_CONV, 0, bin->fields["owner"], vm::MEM_STACK + 1, vm::CONVTYPE_UINT, vm::CONVTYPE_ADDRESS);
		code.emplace_back(vm::OP_CONV, 0, bin->fields["bid_currency"], vm::MEM_STACK + 2, vm::CONVTYPE_UINT, vm::CONVTYPE_ADDRESS);
		code.emplace_back(vm::OP_CONV, 0, bin->fields["ask_currency"], vm::MEM_STACK + 3, vm::CONVTYPE_UINT, vm::CONVTYPE_ADDRESS);
		code.emplace_back(vm::OP_CONV, 0, bin->fields["inv_price"], vm::MEM_STACK + 4, vm::CONVTYPE_UINT, vm::CONVTYPE_BASE_10);
		code.emplace_back(vm::OP_CMP_EQ, 0, vm::MEM_STACK + 10, vm::MEM_STACK + 5, const_map["null"]);
		code.emplace_back(vm::OP_JUMPI, 0, code.size() + 3, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_CONV, 0, bin->fields["partner"], vm::MEM_STACK + 5, vm::CONVTYPE_UINT, vm::CONVTYPE_ADDRESS);
		code.emplace_back(vm::OP_JUMP, 0, code.size() + 2);
		code.emplace_back(vm::OP_COPY, 0, bin->fields["partner"], const_map["null"]);
		code.emplace_back(vm::OP_RET);
		bin->methods[method.name] = method;
	}
	{
		mmx::contract::method_t method;
		method.name = "deposit";
		method.is_public = true;
		method.is_payable = true;
		method.entry_point = code.size();
		code.emplace_back(vm::OP_CMP_EQ, 0, vm::MEM_STACK + 10, vm::MEM_EXTERN + vm::EXTERN_USER, bin->fields["owner"]);
		code.emplace_back(vm::OP_JUMPI, 0, code.size() + 2, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_FAIL, 0, const_map["fail_owner"]);
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
		code.emplace_back(vm::OP_GET, 0, vm::MEM_STACK + 11, vm::MEM_EXTERN + vm::EXTERN_BALANCE, bin->fields["bid_currency"]);
		code.emplace_back(vm::OP_CMP_GT, 0, vm::MEM_STACK + 10, vm::MEM_STACK + 11, const_map["zero"]);
		code.emplace_back(vm::OP_JUMPN, 0, code.size() + 2, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_SEND, 0, bin->fields["owner"], vm::MEM_STACK + 11, bin->fields["bid_currency"]);
		code.emplace_back(vm::OP_RET);
		bin->methods[method.name] = method;
	}
	{
		mmx::contract::method_t method;
		method.name = "withdraw";
		method.is_public = true;
		method.entry_point = code.size();
		code.emplace_back(vm::OP_CMP_EQ, 0, vm::MEM_STACK + 10, vm::MEM_EXTERN + vm::EXTERN_USER, bin->fields["owner"]);
		code.emplace_back(vm::OP_JUMPI, 0, code.size() + 2, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_FAIL, 0, const_map["fail_owner"]);
		code.emplace_back(vm::OP_GET, 0, vm::MEM_STACK + 11, vm::MEM_EXTERN + vm::EXTERN_BALANCE, bin->fields["ask_currency"]);
		code.emplace_back(vm::OP_CMP_GT, 0, vm::MEM_STACK + 10, vm::MEM_STACK + 11, const_map["zero"]);
		code.emplace_back(vm::OP_JUMPN, 0, code.size() + 2, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_SEND, 0, bin->fields["owner"], vm::MEM_STACK + 11, bin->fields["ask_currency"]);
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
		// check partner if set
		code.emplace_back(vm::OP_CMP_EQ, 0, vm::MEM_STACK + 10, bin->fields["partner"], const_map["null"]);
		code.emplace_back(vm::OP_JUMPI, 0, code.size() + 4, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_CMP_EQ, 0, vm::MEM_STACK + 10, bin->fields["partner"], vm::MEM_EXTERN + vm::EXTERN_USER);
		code.emplace_back(vm::OP_JUMPI, 0, code.size() + 2, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_FAIL, 0, const_map["fail_partner"]);
		// check ask_currency
		code.emplace_back(vm::OP_GET, 0, vm::MEM_STACK + 11, vm::MEM_EXTERN + vm::EXTERN_DEPOSIT, const_map["zero"]);
		code.emplace_back(vm::OP_CMP_EQ, 0, vm::MEM_STACK + 10, vm::MEM_STACK + 11, bin->fields["ask_currency"]);
		code.emplace_back(vm::OP_JUMPI, 0, code.size() + 2, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_FAIL, 0, const_map["fail_currency"]);
		// get ask amount
		code.emplace_back(vm::OP_GET, 0, vm::MEM_STACK + 11, vm::MEM_EXTERN + vm::EXTERN_DEPOSIT, const_map["one"]);
		// calc bid amount
		code.emplace_back(vm::OP_MUL, vm::OPFLAG_CATCH_OVERFLOW, vm::MEM_STACK + 12, vm::MEM_STACK + 11, bin->fields["inv_price"]);
		code.emplace_back(vm::OP_SHR, 0, vm::MEM_STACK + 12, vm::MEM_STACK + 12, FRACT_BITS);
		// send bid amount to dst_addr
		code.emplace_back(vm::OP_CONV, 0, vm::MEM_STACK + 13, vm::MEM_STACK + 1, vm::CONVTYPE_UINT, vm::CONVTYPE_ADDRESS);
		code.emplace_back(vm::OP_SEND, 0, vm::MEM_STACK + 13, vm::MEM_STACK + 12, bin->fields["bid_currency"]);
		code.emplace_back(vm::OP_RET);
		bin->methods[method.name] = method;
	}
	{
		mmx::contract::method_t method;
		method.name = "accept";
		method.args = {"dst_addr", "min_bid_amount"};
		method.is_public = true;
		method.is_payable = true;
		method.entry_point = code.size();
		// check partner if set
		code.emplace_back(vm::OP_CMP_EQ, 0, vm::MEM_STACK + 10, bin->fields["partner"], const_map["null"]);
		code.emplace_back(vm::OP_JUMPI, 0, code.size() + 4, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_CMP_EQ, 0, vm::MEM_STACK + 10, bin->fields["partner"], vm::MEM_EXTERN + vm::EXTERN_USER);
		code.emplace_back(vm::OP_JUMPI, 0, code.size() + 2, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_FAIL, 0, const_map["fail_partner"]);
		// check ask_currency
		code.emplace_back(vm::OP_GET, 0, vm::MEM_STACK + 11, vm::MEM_EXTERN + vm::EXTERN_DEPOSIT, const_map["zero"]);
		code.emplace_back(vm::OP_CMP_EQ, 0, vm::MEM_STACK + 10, vm::MEM_STACK + 11, bin->fields["ask_currency"]);
		code.emplace_back(vm::OP_JUMPI, 0, code.size() + 2, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_FAIL, 0, const_map["fail_currency"]);
		// check for sufficient bid amount
		code.emplace_back(vm::OP_GET, 0, vm::MEM_STACK + 13, vm::MEM_EXTERN + vm::EXTERN_BALANCE, bin->fields["bid_currency"]);
		code.emplace_back(vm::OP_CMP_LT, 0, vm::MEM_STACK + 10, vm::MEM_STACK + 13, vm::MEM_STACK + 2);
		code.emplace_back(vm::OP_JUMPN, 0, code.size() + 2, vm::MEM_STACK + 10);
		code.emplace_back(vm::OP_FAIL, 0, const_map["fail_bid_amount"], 1);
		// get ask amount
		code.emplace_back(vm::OP_GET, 0, vm::MEM_STACK + 11, vm::MEM_EXTERN + vm::EXTERN_DEPOSIT, const_map["one"]);
		// calc bid amount
		code.emplace_back(vm::OP_MUL, vm::OPFLAG_CATCH_OVERFLOW, vm::MEM_STACK + 12, vm::MEM_STACK + 11, bin->fields["inv_price"]);
		code.emplace_back(vm::OP_SHR, 0, vm::MEM_STACK + 12, vm::MEM_STACK + 12, FRACT_BITS);
		// clamp to bid balance
		code.emplace_back(vm::OP_MIN, 0, vm::MEM_STACK + 12, vm::MEM_STACK + 12, vm::MEM_STACK + 13);
		// send bid amount to dst_addr
		code.emplace_back(vm::OP_CONV, 0, vm::MEM_STACK + 14, vm::MEM_STACK + 1, vm::CONVTYPE_UINT, vm::CONVTYPE_ADDRESS);
		code.emplace_back(vm::OP_SEND, 0, vm::MEM_STACK + 14, vm::MEM_STACK + 12, bin->fields["bid_currency"]);
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

	auto tx = Transaction::create();
	tx->deploy = bin;
	tx->id = tx->calc_hash(true);
	tx->content_hash = tx->calc_hash(true);

	vnx::write_to_file("data/tx_offer_binary.dat", tx);

	std::cout << addr_t(tx->id).to_string() << std::endl;

	return 0;
}


