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
#include <mmx/mnemonic.h>
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
		vnx::test::expect(hash_t(std::string()).to_string(), "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855");
		vnx::test::expect(hash_t::empty().to_string(), "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855");
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

	VNX_TEST_BEGIN("proof_verify")
	{
		mmx::hash_t plot_id;
		mmx::hash_t challenge;
		std::vector<uint32_t> x_values;
		vnx::from_string("156EBEA58EF0E25E9647C87380238F45E4E446114ED78F6B40C626319EE7DD21", challenge);
		vnx::from_string("60C67427E27FBD77C8BDFF5EB1CC3696C08926FA12BCE8B1AE48E87099F9366B", plot_id);
		vnx::from_string("[3684070240, 4088919472, 4255854024, 2557705011, 2826775019, 784116104, 3002057429, 1544955509, 1600690361, 1551417277, 2728397286, 1771107893, 1187799433, 1044600637, 1880286022, 1921473397, 4059605908, 3921758377, 1663788388, 2187951111, 1469314847, 2221602546, 3049849478, 2483049477, 783906790, 1769349661, 627534699, 1664365119, 3108470239, 4050165503, 1719994423, 2296755013, 3689388662, 173076783, 178304083, 3395522713, 1454472817, 3194528037, 1512291786, 1987638418, 1143780087, 3120478614, 2855051906, 693584283, 3818941816, 4127002689, 2989697755, 3891407791, 2509655266, 4258376726, 1843252036, 3191219821, 2088352375, 213252018, 4115819364, 3175076101, 3561808706, 1948511669, 3322128563, 3750559212, 2272209440, 1293941526, 437177217, 2820197201, 3807400022, 7496129, 2683510002, 3807839482, 2233243744, 2411092355, 1331204979, 2038088672, 2167782813, 3830146105, 537456114, 2401528407, 2652250456, 2221908904, 890566783, 708924513, 2596290458, 340917615, 3250050811, 3771386335, 3494357838, 1179822413, 2341748577, 602106011, 1122065619, 252962133, 278550961, 1768804869, 669497081, 1990086308, 2491380917, 51625349, 4207300886, 3095591768, 1852131389, 609642249, 2918683512, 2059312217, 3335572914, 2736167997, 1528047374, 4124848408, 902683345, 4263117025, 108772979, 485864815, 2410357795, 908723453, 1183430568, 2815414658, 2737238764, 2669408162, 2850938826, 2890536155, 491707862, 2553723643, 1034532861, 3214153497, 3097594346, 2701020101, 678153046, 2932267943, 1365864923, 532310940, 2351720145, 4080824906, 2893128375, 2595930727, 1064911548, 3834810248, 3565525092, 163085774, 685730942, 2810511962, 2540444228, 1857924416, 4174369771, 2145288036, 587439552, 718732787, 1169724691, 3254817017, 3946843084, 954721517, 3373230078, 3637676521, 1331632982, 4086478124, 4116294100, 4120279824, 476981752, 2430423022, 220679215, 4117454286, 4166904409, 2171504903, 2001727739, 3383221100, 2596677253, 988991329, 2638232513, 1095569023, 3303068778, 1931231583, 2226576220, 1480273400, 3911488963, 1608929138, 1376754282, 61629307, 3957255986, 514129543, 2169952004, 3308565016, 3572656193, 1573653323, 961047589, 2310528924, 2099635716, 817068327, 3653864895, 4168803514, 2147588334, 2794673138, 492271828, 3268899536, 1623851505, 1985375222, 396546240, 3609981176, 2079278511, 2161006185, 2007577134, 2954139926, 1277553298, 3205868414, 4257573912, 3376256719, 4227206123, 585150605, 2904436845, 2619013042, 3125718317, 3035845111, 2049651560, 161957643, 393761167, 2783560380, 1211203226, 3313590097, 479870197, 830535414, 3613172362, 3953259697, 265953611, 1493207470, 955279780, 2133758876, 2884678190, 3234737433, 1365475005, 2908229202, 2072744826, 4263640821, 2236131683, 894009793, 307201858, 3291293763, 2184230914, 776021200, 867100221, 4032840254, 2454442006, 1289389606, 1247346023, 386849697, 1757849095, 973341783, 3958516371, 1277136670, 205471439, 2514111789, 1777386820, 1436581469, 1494466641, 512070215, 2845825157, 329403080, 1115243938, 4009264613, 3806285763, 3170487941]", x_values);

		const auto plot_challenge = get_plot_challenge(challenge, plot_id);
		mmx::pos::verify(x_values, plot_challenge, plot_id, 4, 0, 32, false);
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

		mmx::pos::verify(proof.proof_xs, plot_challenge, proof.plot_id, plot_filter, 0, proof.ksize, false);

		std::default_random_engine engine;
		for(int i = 0; i < 100; ++i) {
			std::shuffle(proof.proof_xs.begin(), proof.proof_xs.end(), engine);
			expect_throw([=]() {
				mmx::pos::verify(proof.proof_xs, plot_challenge, proof.plot_id, plot_filter, 0, proof.ksize, false);
			});
		}
	}
	VNX_TEST_END()

	VNX_TEST_BEGIN("mnemonic")
	{
		vnx::test::expect(
				mmx::mnemonic::words_to_string(mmx::mnemonic::seed_to_words(hash_t(), mmx::mnemonic::wordlist_en)),
				"abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon art");
		vnx::test::expect(
				mmx::mnemonic::words_to_seed(mmx::mnemonic::string_to_words("void come effort suffer camp survey warrior heavy shoot primary clutch crush open amazing screen patrol group space point ten exist slush involve unfold"), mmx::mnemonic::wordlist_en).to_string(),
				"8F9D98FD746F9D0AFA660965B00FB2891AB25495C653D37DB50D52EC1AC185F5");
		hash_t seed;
		vnx::from_string("8F9D98FD746F9D0AFA660965B00FB2891AB25495C653D37DB50D52EC1AC185F5", seed);
		vnx::test::expect(seed.to_string(), "8F9D98FD746F9D0AFA660965B00FB2891AB25495C653D37DB50D52EC1AC185F5");
		vnx::test::expect(
				mmx::mnemonic::words_to_string(mmx::mnemonic::seed_to_words(seed, mmx::mnemonic::wordlist_en)),
				"void come effort suffer camp survey warrior heavy shoot primary clutch crush open amazing screen patrol group space point ten exist slush involve unfold");
	}
	VNX_TEST_END()

	mmx::secp256k1_free();

	return vnx::test::done();
}



