/*
 * test_transactions.cpp
 *
 *  Created on: Sep 23, 2022
 *      Author: mad
 */

#include <mmx/ECDSA_Wallet.h>
#include <mmx/NodeClient.hxx>
#include <mmx/KeyFile.hxx>
#include <mmx/contract/MultiSig.hxx>
#include <mmx/contract/Executable.hxx>
#include <mmx/operation/Execute.hxx>
#include <mmx/solution/MultiSig.hxx>
#include <mmx/utils.h>

#include <vnx/vnx.h>
#include <vnx/Proxy.h>

using namespace mmx;

void expect_fail(NodeClient& node, std::shared_ptr<Transaction> tx)
{
	try {
		node.add_transaction(tx, true);
	} catch(...) {
		return;
	}
	throw std::logic_error("expected fail");
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
	if(params->network.empty()) {
		throw std::logic_error("no network configured");
	}

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
	{
		auto tx = Transaction::create();
		tx->sender = wallet.get_address(0);
		auto exec = mmx::contract::Executable::create();
		exec->binary = params->plot_nft_binary;
		exec->init_args.emplace_back(tx->sender->to_string());
		tx->deploy = exec;
		auto op = mmx::operation::Execute::create();
		op->method = "transfer";
		op->user = wallet.get_address(0);
		op->args.emplace_back(wallet.get_address(1).to_string());
		tx->execute.push_back(op);
		tx->max_fee_amount = -1;
		wallet.sign_off(tx);

		std::cout << tx->to_string() << std::endl;
		node.add_transaction(tx, true);
	}
	{
		auto tx = Transaction::create();
		tx->sender = wallet.get_address(0);
		auto multi = contract::MultiSig::create();
		multi->num_required = 3;
		for(int i = 0; i < 3; ++i) {
			multi->owners.insert(wallet.get_address(i));
		}
		tx->deploy = multi;
		tx->max_fee_amount = -1;
		wallet.sign_off(tx);

		std::cout << tx->to_string() << std::endl;
		node.add_transaction(tx, true);
	}

	vnx::close();

	mmx::secp256k1_free();

	return 0;
}








