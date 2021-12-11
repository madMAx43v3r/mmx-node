/*
 * mmx_node.cpp
 *
 *  Created on: Dec 10, 2021
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/Wallet.h>
#include <mmx/TimeLord.h>
#include <mmx/secp256k1.hpp>

#include <vnx/vnx.h>
#include <vnx/Terminal.h>


int main(int argc, char** argv)
{
	mmx::secp256k1_init();

	vnx::init("mmx_node", argc, argv);

	{
		vnx::Handle<vnx::Terminal> module = new vnx::Terminal("Terminal");
		module.start_detached();
	}
	{
		vnx::Handle<mmx::TimeLord> module = new mmx::TimeLord("TimeLord");
		module.start_detached();
	}
	{
		vnx::Handle<mmx::Wallet> module = new mmx::Wallet("Wallet");
		module.start_detached();
	}
	{
		vnx::Handle<mmx::Node> module = new mmx::Node("Node");
		module.start_detached();
	}

	vnx::wait();

	mmx::secp256k1_free();

	return 0;
}


