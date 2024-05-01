/*
 * test_transactions.cpp
 *
 *  Created on: Sep 23, 2022
 *      Author: mad
 */

#include <mmx/ECDSA_Wallet.h>
#include <mmx/NodeClient.hxx>
#include <mmx/KeyFile.hxx>
#include <mmx/utils.h>

#include <vnx/vnx.h>
#include <vnx/Proxy.h>

using namespace mmx;

void expect_fail(NodeClient& node, std::shared_ptr<Transaction> tx)
{
	try {
		node.add_transaction(tx, true);
		std::cout << "Didn't fail!" << std::endl;
		exit(-1);
	} catch(...) {}
}


int main(int argc, char** argv)
{
	mmx::secp256k1_init();

	std::map<std::string, std::string> options;
	options["w"] = "wallet";
	options["n"] = "node";

	vnx::init("test_transactions", argc, argv, options);

	std::string key_path = "wallet.dat";
	std::string node_url = ":11331";
	vnx::read_config("wallet", key_path);

	auto key_file = vnx::read_from_file<KeyFile>(key_path);
	if(!key_file) {
		throw std::logic_error("missing wallet: " + key_path);
	}
	const auto params = mmx::get_params();

	vnx::Handle<vnx::Proxy> proxy = new vnx::Proxy("Proxy", vnx::Endpoint::from_url(node_url));
	proxy->forward_list = {"Node"};
	proxy.start();

	NodeClient node("Node");

	account_t config;
	config.key_file = key_path;
	config.num_addresses = 100;

	ECDSA_Wallet wallet(key_file->seed_value, config, params);
	wallet.unlock();

	const auto height = node.get_height();
	const auto balances = node.get_all_balances(wallet.get_all_addresses());
	wallet.update_cache(balances, {}, height);

	{
		auto tx = Transaction::create();
		tx->sender = wallet.get_address(0);
		tx->max_fee_amount = 0;
		wallet.sign_off(tx);

		std::cout << tx->to_string() << std::endl;
		expect_fail(node, tx);
	}
	{
		auto tx = Transaction::create();
		tx->sender = wallet.get_address(0);
		tx->fee_ratio = 0;
		tx->max_fee_amount = 100000;
		wallet.sign_off(tx);

		std::cout << tx->to_string() << std::endl;
		expect_fail(node, tx);
	}
	{
		auto tx = Transaction::create();
		tx->sender = wallet.get_address(0);
		tx->fee_ratio = 100 * 1024;
		tx->max_fee_amount = 100000;
		wallet.sign_off(tx);

		std::cout << tx->to_string() << std::endl;
		expect_fail(node, tx);
	}
	{
		auto tx = Transaction::create();
		tx->sender = wallet.get_address(config.num_addresses - 1);
		tx->max_fee_amount = 100000;
		wallet.sign_off(tx);

		std::cout << tx->to_string() << std::endl;
		expect_fail(node, tx);
	}
	{
		auto tx = Transaction::create();
		tx->expires = 0;
		tx->sender = wallet.get_address(0);
		tx->max_fee_amount = 100000;
		wallet.sign_off(tx);

		std::cout << tx->to_string() << std::endl;
		expect_fail(node, tx);
	}
	{
		auto tx = Transaction::create();
		tx->sender = wallet.get_address(0);
		tx->add_output(addr_t(), addr_t(), -1);
		tx->max_fee_amount = 100000;
		wallet.sign_off(tx);

		std::cout << tx->to_string() << std::endl;
		expect_fail(node, tx);
	}
	{
		auto tx = Transaction::create();
		tx->sender = wallet.get_address(0);
		tx->add_input(addr_t(), wallet.get_address(0), -1);
		tx->add_output(addr_t(), addr_t(), -1);
		tx->max_fee_amount = 100000;
		wallet.sign_off(tx);

		std::cout << tx->to_string() << std::endl;
		expect_fail(node, tx);
	}
	{
		auto tx = Transaction::create();
		tx->sender = wallet.get_address(0);
		tx->add_input(addr_t(), wallet.get_address(0), 100000);
		for(int i = 0; i < 100000; ++i) {
			tx->add_output(addr_t(), addr_t(), 1);
		}
		tx->max_fee_amount = -1;
		wallet.sign_off(tx);

		expect_fail(node, tx);
	}
	{
		auto tx = Transaction::create();
		tx->sender = wallet.get_address(0);
		tx->add_input(addr_t(), wallet.get_address(0), 1);
		tx->add_output(addr_t(), addr_t(), 1);
		tx->outputs[0].memo = std::string(txio_t::MAX_MEMO_SIZE + 1, 'M');
		tx->max_fee_amount = -1;
		wallet.sign_off(tx);

		std::cout << tx->to_string() << std::endl;
		expect_fail(node, tx);
	}

	vnx::close();

	mmx::secp256k1_free();

	return 0;
}


