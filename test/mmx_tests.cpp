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
#include <mmx/tree_hash.h>
#include <mmx/write_bytes.h>
#include <mmx/utils.h>

#include <mmx/ChainParams.hxx>
#include <mmx/contract/Data.hxx>
#include <mmx/contract/Binary.hxx>
#include <mmx/contract/MultiSig.hxx>
#include <mmx/contract/Executable.hxx>
#include <mmx/contract/WebData.hxx>
#include <mmx/contract/MultiSig.hxx>
#include <mmx/solution/MultiSig.hxx>
#include <mmx/solution/PubKey.hxx>

#include <mmx/ProofOfSpaceOG.hxx>
#include <mmx/pos/verify.h>

#include <vnx/vnx.h>
#include <vnx/test/Test.h>

#include <map>
#include <iostream>
#include <random>

void expect_throw(const std::function<void()>& code)
{
	bool did_throw = false;
	try {
		code();
	} catch(...) {
		did_throw = true;
	}
	if(!did_throw) {
		throw std::logic_error("expected failure");
	}
}

using namespace mmx;


int main(int argc, char** argv)
{
	mmx::secp256k1_init();

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
		vnx::test::expect(vnx::Variant(uint128((uint128_1 << 127) + 3)).to<uint128>(), uint128((uint128_1 << 127) + 3));
	}
	VNX_TEST_END()

	VNX_TEST_BEGIN("hash_t")
	{
		vnx::test::expect(hash_t().to_string(), "0000000000000000000000000000000000000000000000000000000000000000");
		vnx::test::expect(hash_t() < hash_t(), false);
		vnx::test::expect(hash_t() > hash_t(), false);
		vnx::test::expect(hash_t() < hash_t("1"), true);
		vnx::test::expect(hash_t() > hash_t("1"), false);
		vnx::test::expect(hash_t("1") > hash_t(), true);
		vnx::test::expect(hash_t("1") < hash_t(), false);
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

	VNX_TEST_BEGIN("to_amount()")
	{
		vnx::test::expect<uint128, uint128>(mmx::to_amount(1.0, int(0)), 1);
		vnx::test::expect<uint128, uint128>(mmx::to_amount(1337.0, int(0)), 1337);
		vnx::test::expect<uint128, uint128>(mmx::to_amount(1337.1337, int(4)), 13371337);
		vnx::test::expect<uint128, uint128>(mmx::to_amount(1337.1337, int(6)), 1337133700);
		vnx::test::expect<uint128, uint128>(mmx::to_amount(1180591620717411303424.0, int(0)), uint128_1 << 70);
		vnx::test::expect<uint128, uint128>(mmx::to_amount(10, int(18)), 10000000000000000000ull);
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

	VNX_TEST_BEGIN("is_json()")
	{
		vnx::test::expect(is_json(vnx::Variant()), true);
		vnx::test::expect(is_json(vnx::Variant(nullptr)), true);
		vnx::test::expect(is_json(vnx::Variant(false)), true);
		vnx::test::expect(is_json(vnx::Variant(true)), true);
		vnx::test::expect(is_json(vnx::Variant(1)), true);
		vnx::test::expect(is_json(vnx::Variant(1 << 16)), true);
		vnx::test::expect(is_json(vnx::Variant(uint64_t(1) << 32)), true);
		vnx::test::expect(is_json(vnx::Variant(-1)), true);
		vnx::test::expect(is_json(vnx::Variant(-256)), true);
		vnx::test::expect(is_json(vnx::Variant(-65536)), true);
		vnx::test::expect(is_json(vnx::Variant(-4294967296)), true);
		vnx::test::expect(is_json(vnx::Variant("")), true);
		vnx::test::expect(is_json(vnx::Variant("test")), true);
		vnx::test::expect(is_json(vnx::Variant(std::vector<vnx::Variant>{})), true);
		vnx::test::expect(is_json(vnx::Variant(std::vector<vnx::Variant>{vnx::Variant(1337), vnx::Variant("test")})), true);
		vnx::test::expect(is_json(vnx::Variant(vnx::Object())), true);
		vnx::test::expect(is_json(vnx::Variant(vnx::Object({{"test", vnx::Variant(1337)}, {"test1", vnx::Variant("test")}}))), true);

		vnx::test::expect(is_json(vnx::Variant(std::array<int16_t, 10>())), false);
		vnx::test::expect(is_json(vnx::Variant(std::vector<uint32_t>{1, 2, 3})), false);
		vnx::test::expect(is_json(vnx::Variant(std::map<std::string, uint32_t>())), false);
		vnx::test::expect(is_json(vnx::Variant(std::vector<vnx::Variant>{vnx::Variant(std::vector<uint32_t>{1, 2, 3})})), false);
	}
	VNX_TEST_END()

	VNX_TEST_BEGIN("Contract::num_bytes()")
	{
		auto params = mmx::ChainParams::create();
		params->min_txfee_byte = 100;
		const std::string str(1024 * 1024, 'A');
		{
			auto tmp = mmx::contract::Data::create();
			tmp->value = str;
			vnx::test::expect(tmp->num_bytes() > str.size(), true);
			vnx::test::expect(tmp->calc_cost(params) > str.size() * params->min_txfee_byte, true);
		}
		{
			auto tmp = mmx::contract::WebData::create();
			tmp->mime_type = str;
			tmp->payload = str;
			vnx::test::expect(tmp->num_bytes() > 2 * str.size(), true);
		}
		{
			auto tmp = mmx::contract::Binary::create();
			tmp->binary.resize(str.size());
			tmp->constant.resize(str.size());
			tmp->compiler = str;
			tmp->fields.emplace(str, 0);
			tmp->methods.emplace(str, mmx::contract::method_t());
			tmp->name = str;
			tmp->source = str;
			vnx::test::expect(tmp->num_bytes() > 7 * str.size(), true);
		}
		{
			auto tmp = mmx::contract::Binary::create();
			for(uint32_t i = 0; i < 1024; ++i) {
				tmp->line_info.emplace(i, i);
			}
			vnx::test::expect(tmp->num_bytes() > 8 * 1024, true);
		}
		{
			auto tmp = mmx::contract::TokenBase::create();
			tmp->meta_data = str;
			tmp->name = str;
			tmp->symbol = str;
			vnx::test::expect(tmp->num_bytes() > 3 * str.size(), true);
		}
		{
			auto tmp = mmx::contract::Executable::create();
			tmp->depends.emplace(str, mmx::addr_t());
			tmp->init_args.emplace_back(str);
			tmp->init_method = str;
			tmp->meta_data = str;
			tmp->name = str;
			tmp->symbol = str;
			vnx::test::expect(tmp->num_bytes() > 6 * str.size(), true);
		}
		{
			auto tmp = mmx::contract::MultiSig::create();
			for(uint32_t i = 0; i < 256; ++i) {
				tmp->owners.emplace(hash_t(std::to_string(i)));
			}
			vnx::test::expect(tmp->num_bytes() > 32 * 256, true);
		}
	}
	VNX_TEST_END()

	VNX_TEST_BEGIN("write_bytes()")
	{
		{
			const bool value = true;
			std::vector<uint8_t> tmp;
			vnx::VectorOutputStream stream(&tmp);
			vnx::OutputBuffer out(&stream);
			write_bytes(out, value);
			out.flush();
			vnx::test::expect(tmp.size(), 1u);
		}
		{
			const vnx::Variant value(true);
			std::vector<uint8_t> tmp;
			vnx::VectorOutputStream stream(&tmp);
			vnx::OutputBuffer out(&stream);
			write_bytes(out, value);
			out.flush();
			vnx::test::expect(tmp.size(), 1u);
		}
		{
			const vnx::Variant value(1337);
			std::vector<uint8_t> tmp;
			vnx::VectorOutputStream stream(&tmp);
			vnx::OutputBuffer out(&stream);
			write_bytes(out, value);
			out.flush();
			vnx::test::expect(tmp.size(), 8u);
		}
		{
			const vnx::Variant value(-1337);
			std::vector<uint8_t> tmp;
			vnx::VectorOutputStream stream(&tmp);
			vnx::OutputBuffer out(&stream);
			write_bytes(out, value);
			out.flush();
			vnx::test::expect(tmp.size(), 8u);
		}
	}
	VNX_TEST_END()

	VNX_TEST_BEGIN("MultiSig")
	{
		const hash_t txid;
		std::vector<std::pair<skey_t, pubkey_t>> keys;
		for(int i = 0; i < 10; ++i) {
			const auto skey = skey_t(hash_t(std::to_string(i)));
			keys.emplace_back(skey, pubkey_t(skey));
		}
		{
			auto tmp = mmx::contract::MultiSig::create();
			for(int i = 0; i < 3; ++i) {
				tmp->owners.insert(keys[i].second.get_addr());
			}
			tmp->num_required = 1;
			vnx::test::expect(tmp->is_valid(), true);

			for(int i = 0; i < 3; ++i) {
				auto sol = mmx::solution::PubKey::create();
				sol->pubkey = keys[i].second;
				sol->signature = signature_t::sign(keys[i].first, txid);
				tmp->validate(sol, txid);
			}
			{
				auto sol = mmx::solution::MultiSig::create();
				sol->num_required = tmp->num_required;
				for(int i = 0; i < 3; ++i) {
					auto tmp = mmx::solution::PubKey::create();
					tmp->pubkey = keys[i].second;
					tmp->signature = signature_t::sign(keys[i].first, txid);
					sol->solutions[tmp->pubkey.get_addr()] = tmp;
				}
				tmp->validate(sol, txid);
			}
			{
				auto sol = mmx::solution::PubKey::create();
				const auto skey = skey_t(hash_t::random());
				sol->pubkey = pubkey_t(skey);
				sol->signature = signature_t::sign(skey, txid);
				expect_throw([=]() {
					tmp->validate(sol, txid);
				});
			}
		}
		{
			auto tmp = mmx::contract::MultiSig::create();
			for(int i = 0; i < 5; ++i) {
				tmp->owners.insert(keys[i].second.get_addr());
			}
			tmp->num_required = 3;
			vnx::test::expect(tmp->is_valid(), true);
			{
				auto sol = mmx::solution::MultiSig::create();
				for(int i = 0; i < 3; ++i) {
					auto tmp = mmx::solution::PubKey::create();
					tmp->pubkey = keys[i].second;
					tmp->signature = signature_t::sign(keys[i].first, txid);
					sol->solutions[tmp->pubkey.get_addr()] = tmp;
				}
				tmp->validate(sol, txid);
			}
			{
				auto sol = mmx::solution::MultiSig::create();
				for(int i = 2; i < 5; ++i) {
					auto tmp = mmx::solution::PubKey::create();
					tmp->pubkey = keys[i].second;
					tmp->signature = signature_t::sign(keys[i].first, txid);
					sol->solutions[tmp->pubkey.get_addr()] = tmp;
				}
				tmp->validate(sol, txid);
			}
			{
				auto sol = mmx::solution::MultiSig::create();
				for(uint32_t i = 0; i < tmp->num_required; ++i) {
					auto tmp = mmx::solution::PubKey::create();
					const auto skey = skey_t(hash_t::random());
					tmp->pubkey = skey;
					tmp->signature = signature_t::sign(skey, txid);
					sol->solutions[tmp->pubkey.get_addr()] = tmp;
				}
				expect_throw([=]() {
					tmp->validate(sol, txid);
				});
			}
		}
	}
	VNX_TEST_END()

	VNX_TEST_BEGIN("proof_order")
	{
		mmx::ProofOfSpaceOG proof;
		vnx::from_string("{\
			\"plot_id\": \"8FA05447F4F0849B7875E0D0D25B99360F8A7981C67DE7CA5FCF1E1483089F82\",\
			\"challenge\": \"3BD3D3158D26678DBD4910F4F8975F5177C4C45A5B68D52686939566C5D88239\",\
			\"farmer_key\": \"027D7562FB5A8967E57A22F876302F75BA3AE3980607FB32E4439681F820AE398F\",\
			\"ksize\": 29,\
			\"seed\": \"B655BE5A884F7CC18FB3D423F6E9D48B229DD13DD1CC3B9D7A165012077D49D3\",\
			\"proof_xs\": [41526030, 190034280, 27487774, 360531876, 157328122, 533344899, 348912715, 158286857, 500512399, 286789283, 309409411, 33301427, 291496595, 292537, 431952279, 209584997, 111041179, 225686812, 377306265, 45918619, 136492725, 443847657, 63762825, 455772217, 299457809, 62819158, 423066079, 70184667, 116121877, 262069313, 94597380, 277096972, 178726801, 264024912, 497654815, 81149636, 201256170, 209529847, 86228594, 32668245, 122504482, 174029193, 404468875, 222875439, 241585134, 438378901, 310806690, 77808852, 506242593, 337807442, 171130081, 360833911, 483590771, 498133539, 78329797, 41557646, 163327753, 315798381, 131747531, 361264561, 216455451, 179665178, 51870911, 429035183, 295349671, 152798101, 253786116, 433103191, 476044027, 346642880, 47227825, 208846273, 206436033, 245874067, 427075796, 180779200, 410970246, 1143092, 50889398, 224426279, 375125060, 188487454, 319636984, 272259131, 377087291, 271901983, 531636482, 454874007, 274228377, 431843070, 523612467, 361408983, 423618927, 339345523, 86142409, 160801159, 22024508, 233678781, 241239177, 475088783, 400172747, 362038581, 429952975, 121634389, 179032692, 152394426, 223298082, 93030597, 63400229, 175256560, 530644023, 21993664, 153789041, 515107385, 135370091, 156498932, 72211704, 400919262, 136383461, 504380151, 239462247, 184409768, 245564618, 264419588, 183760152, 303573155, 280701899, 412267540, 281366400, 318612884, 281199257, 28786199, 280595286, 469254350, 516006656, 76648711, 534052766, 109825608, 383408610, 400602196, 201747007, 232103494, 225366639, 473932026, 373853689, 523521134, 385137715, 297896772, 460856009, 253065215, 191027004, 302714203, 102745112, 118857551, 531336719, 298547044, 308312588, 441150850, 177353289, 366287353, 117270343, 323760640, 436125237, 193628024, 496102441, 192425221, 174984738, 267393617, 119828474, 382207855, 157357834, 102830320, 69524625, 32453159, 470198640, 253550250, 12023423, 143563219, 38100930, 259685561, 358040000, 277554148, 502359901, 375665759, 268245745, 193785837, 21177576, 97132723, 237830009, 390049154, 424082856, 287383135, 482999386, 93334862, 211737164, 32954960, 379554381, 329067393, 322071582, 452967760, 223523925, 394112637, 91770475, 33515598, 41541163, 159354561, 448723432, 360714676, 298810627, 463092740, 8253330, 416722492, 172191294, 358486549, 286025427, 260690064, 290924466, 157109118, 14568701, 256902937, 225155064, 507728616, 21730277, 31804253, 365161673, 184388967, 36778686, 241694509, 71357500, 429209559, 275960351, 244822214, 345055770, 122399096, 93423247, 5590085, 299893246, 386457316, 300993063, 533350058, 313848074, 106703937, 516682805, 49602142, 321158224, 265493627, 329208949, 45211675, 112966538, 523299896, 80915641, 499055347, 124794444, 78543398, 155955534, 393135459]\
		}", proof);

		const auto plot_filter = 4;
		const auto plot_challenge = get_plot_challenge(proof.challenge, proof.plot_id);

		mmx::pos::verify(proof.proof_xs, plot_challenge, proof.plot_id, plot_filter, proof.ksize);

		std::default_random_engine engine;
		for(int i = 0; i < 100; ++i) {
			std::shuffle(proof.proof_xs.begin(), proof.proof_xs.end(), engine);
			expect_throw([=]() {
				mmx::pos::verify(proof.proof_xs, plot_challenge, proof.plot_id, plot_filter, proof.ksize);
			});
		}
	}
	VNX_TEST_END()

	mmx::secp256k1_free();

	return vnx::test::done();
}



