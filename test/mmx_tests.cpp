/*
 * mmx_tests.cpp
 *
 *  Created on: Dec 16, 2022
 *      Author: mad
 */

#include <mmx/skey_t.hpp>
#include <mmx/addr_t.hpp>
#include <mmx/uint128.hpp>
#include <mmx/fixed128.hpp>
#include <mmx/memo_t.hpp>
#include <mmx/tree_hash.h>

#include <vnx/vnx.h>
#include <vnx/test/Test.h>

#include <map>
#include <iostream>

using namespace mmx;


int main(int argc, char** argv)
{
	vnx::test::init("mmx");

	VNX_TEST_BEGIN("uint128")
	{
		vnx::test::expect(uint128().to_double(), 0);
		vnx::test::expect(uint128(11).to_double(), 11);
		vnx::test::expect(uint128(1123456).to_double(), 1123456);
		vnx::test::expect((uint128(256) / 1).lower(), 256u);
		vnx::test::expect((uint128(256) / 2).lower(), 128u);
		vnx::test::expect((uint128(256) / 4).lower(), 64u);
		vnx::test::expect((uint128(256) / 8).lower(), 32u);
		vnx::test::expect((uint128(256) / 16).lower(), 16u);
	}
	VNX_TEST_END()

	VNX_TEST_BEGIN("addr_t")
	{
		std::map<std::pair<addr_t, addr_t>, uint128> balance;
		balance[std::make_pair(addr_t(std::string("mmx17uuqmktq33mmh278d3nlqy0mrgw9j2vtg4l5vrte3m06saed9yys2q5hrf")), addr_t())] = 1337;

		vnx::test::expect(balance[std::make_pair(addr_t(std::string("mmx17uuqmktq33mmh278d3nlqy0mrgw9j2vtg4l5vrte3m06saed9yys2q5hrf")), addr_t())], 1337);
		vnx::test::expect(balance[std::make_pair(addr_t(std::string("mmx1hfyq6t2jartw9f8fkkertepxef0f8egegd3m438ndfttrlhzzmks7c99tv")), addr_t())], 0);
	}
	VNX_TEST_END()

	VNX_TEST_BEGIN("fixed128")
	{
		vnx::test::expect(fixed128().to_amount(0), 0);
		vnx::test::expect(fixed128().to_amount(6), 0);
		vnx::test::expect(fixed128(1).to_amount(0), 1);
		vnx::test::expect(fixed128(11).to_amount(6), 11000000);
		vnx::test::expect(fixed128(111, 3).to_string(), "0.111");
		vnx::test::expect(fixed128(1111, 3).to_string(), "1.111");
		vnx::test::expect(fixed128(1.1).to_amount(1), 11);
		vnx::test::expect(fixed128(1.01).to_amount(4), 10100);
		vnx::test::expect(fixed128(1.001).to_amount(4), 10010);
		vnx::test::expect(fixed128(1.0001).to_amount(4), 10001);
		vnx::test::expect(fixed128(1.00001).to_amount(6), 1000010);
		vnx::test::expect(fixed128(1.000001).to_amount(6), 1000001);
		vnx::test::expect(fixed128("1").to_amount(0), 1);
		vnx::test::expect(fixed128("1.").to_amount(0), 1);
		vnx::test::expect(fixed128("1.0").to_amount(0), 1);
		vnx::test::expect(fixed128("1.000000").to_amount(0), 1);
		vnx::test::expect(fixed128("1.2").to_amount(0), 1);
		vnx::test::expect(fixed128("1,3").to_amount(1), 13);
		vnx::test::expect(fixed128("1e1").to_amount(1), 100);
		vnx::test::expect(fixed128("1,1e0").to_amount(2), 110);
		vnx::test::expect(fixed128("1,1E2").to_amount(3), 110000);
		vnx::test::expect(fixed128("1,4E-1").to_amount(2), 14);
		vnx::test::expect(fixed128("123.000000").to_amount(6), 123000000);
		vnx::test::expect(fixed128("123.000001").to_amount(6), 123000001);
		vnx::test::expect(fixed128("123.000011").to_amount(6), 123000011);
		vnx::test::expect(fixed128("0123.012345").to_amount(6), 123012345);
		vnx::test::expect(fixed128("0123.1123456789").to_amount(6), 123112345);
		vnx::test::expect(fixed128("1.1").to_string(), "1.1");
		vnx::test::expect(fixed128("1.01").to_string(), "1.01");
		vnx::test::expect(fixed128("1.000001").to_string(), "1.000001");
		vnx::test::expect(fixed128("1.0123456789").to_string(), "1.0123456789");
		vnx::test::expect(fixed128("1.200").to_string(), "1.2");
		vnx::test::expect(fixed128("001.3").to_string(), "1.3");
		vnx::test::expect(fixed128("1.4").to_value(), 1.4);
	}
	VNX_TEST_END()

	VNX_TEST_BEGIN("memo_t")
	{
		vnx::test::expect(memo_t().to_string(), "");
		vnx::test::expect(memo_t().to_uint().str(10), "0");
		vnx::test::expect(memo_t(1337).to_string(), "1337");
		vnx::test::expect(memo_t(1337).to_uint().str(10), "1337");
		vnx::test::expect(memo_t(1337133713371337).to_uint().str(10), "1337133713371337");
		vnx::test::expect(memo_t("sD1.-=&^%{]|+)$#@!<").to_string(), "sD1.-=&^%{]|+)$#@!<");
		{
			const hash_t hash("fjskdfjksdjf");
			const std::string text = "0x" + hash.to_string();
			vnx::test::expect(memo_t(hash).to_string(), text);
			vnx::test::expect(memo_t(text).to_string(), text);
			vnx::test::expect(memo_t::from_variant(vnx::Variant(hash)).to_string(), text);
		}
		vnx::test::expect(memo_t::from_variant(vnx::Variant(1337)).to_string(), "1337");
		vnx::test::expect(memo_t::from_variant(vnx::Variant("sD1.-=&^%{]|+)$#@!<")).to_string(), "sD1.-=&^%{]|+)$#@!<");
	}
	VNX_TEST_END()

	VNX_TEST_BEGIN("calc_tree_hash()")
	{
		std::vector<hash_t> list;
		auto hash = mmx::calc_btree_hash(list);
		vnx::test::expect(hash, hash_t());

		for(int i = 0; i < 1000; ++i) {
			list.push_back(hash_t(std::to_string(i)));
			auto next = mmx::calc_btree_hash(list);
			vnx::test::expect(next != hash, true);
			hash = next;
		}
	}
	VNX_TEST_END()

	return vnx::test::done();
}



