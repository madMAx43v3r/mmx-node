/*
 * mmx_compile.cpp
 *
 *  Created on: Apr 19, 2023
 *      Author: mad
 */

#include <mmx/Transaction.hxx>
#include <mmx/vm/Compiler.h>
#include <mmx/vm/Engine.h>
#include <mmx/vm/StorageRAM.h>
#include <mmx/vm_interface.h>

#include <vnx/vnx.h>

using namespace mmx;


int main(int argc, char** argv)
{
	std::map<std::string, std::string> options;
	options["f"] = "files";
	options["o"] = "output";
	options["t"] = "txmode";
	options["v"] = "verbose";
	options["e"] = "execute";
	options["g"] = "gas";
	options["files"] = "source files";
	options["output"] = "output name";
	options["gas"] = "gas limit";
	options["assert-fail"] = "assert fail";

	vnx::write_config("log_level", 2);

	vnx::init("mmx_compile", argc, argv, options);

	vm::compile_flags_t flags;

	int verbose = 0;
	bool txmode = false;
	bool execute = false;
	bool assert_fail = false;
	uint64_t gas_limit = -1;
	std::string output = "binary.dat";
	std::vector<std::string> file_names;
	vnx::read_config("verbose", verbose);
	vnx::read_config("txmode", txmode);
	vnx::read_config("execute", execute);
	vnx::read_config("output", output);
	vnx::read_config("files", file_names);
	vnx::read_config("gas", gas_limit);
	vnx::read_config("assert-fail", assert_fail);

	flags.debug = verbose;

	int ret_value = 0;

	const auto bin = vm::compile_files(file_names, flags);

	if(txmode) {
		auto tx = Transaction::create();
		tx->deploy = bin;
		tx->id = tx->calc_hash(false);
		tx->content_hash = tx->calc_hash(true);
		vnx::write_to_file(output, tx);
		std::cout << addr_t(tx->id).to_string() << std::endl;
	} else {
		vnx::write_to_file(output, bin);
	}

	if(execute) {
		auto storage = std::make_shared<vm::StorageRAM>();
		auto engine = std::make_shared<vm::Engine>(addr_t(), storage, false);
		engine->total_gas = gas_limit;
		vm::load(engine, bin);
		engine->begin(0);

		try {
			engine->run();
			engine->commit();
		} catch(const std::exception& ex) {
			if(verbose || !assert_fail) {
				std::cerr << "Failed at 0x" << vnx::to_hex_string(engine->error_addr) << " with: " << ex.what() << std::endl;
			}
			ret_value = 1;
		}
		if(assert_fail) {
			if(ret_value) {
				ret_value = 0;
			} else {
				std::cerr << "Expected execution failure on " << vnx::to_string(file_names) << std::endl;
				ret_value = 1;
			}
		}
		if(verbose) {
			std::cerr << "Total Cost: " << engine->total_cost << std::endl;
		}
	}

	vnx::close();

	return ret_value;
}

