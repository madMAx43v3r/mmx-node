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
#include <mmx/secp256k1.hpp>

#include <vnx/vnx.h>

using namespace mmx;


int main(int argc, char** argv)
{
	mmx::secp256k1_init();

	std::map<std::string, std::string> options;
	options["f"] = "files";
	options["o"] = "output";
	options["t"] = "txmode";
	options["v"] = "verbose";
	options["e"] = "execute";
	options["w"] = "commit";
	options["g"] = "gas";
	options["O"] = "opt-level";
	options["files"] = "source files";
	options["output"] = "output name";
	options["gas"] = "gas limit";
	options["assert-fail"] = "assert fail";

	vnx::write_config("log_level", 2);

	vnx::init("mmx_compile", argc, argv, options);

	compile_flags_t flags;

	int verbose = 0;
	int opt_level = 3;
	bool txmode = false;
	bool execute = false;
	bool commit = false;
	bool assert_fail = false;
	uint64_t gas_limit = -1;
	std::string output = "binary.dat";
	std::vector<std::string> file_names;
	vnx::read_config("verbose", verbose);
	vnx::read_config("opt-level", opt_level);
	vnx::read_config("txmode", txmode);
	vnx::read_config("execute", execute);
	vnx::read_config("commit", commit);
	vnx::read_config("output", output);
	vnx::read_config("files", file_names);
	vnx::read_config("gas", gas_limit);
	vnx::read_config("assert-fail", assert_fail);

	flags.verbose = verbose;
	flags.opt_level = opt_level;

	int ret_value = 0;
	std::shared_ptr<const contract::Binary> bin;

	try {
		bin = vm::compile_files(file_names, flags);
	}
	catch(const std::exception& ex) {
		std::cerr << "Compilation failed with:" << std::endl << "\t" << ex.what() << std::endl;
		vnx::close();
		return 1;
	}

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
		const auto time_begin = vnx::get_wall_time_micros();

		auto storage = std::make_shared<vm::StorageRAM>();
		auto engine = std::make_shared<vm::Engine>(addr_t(), storage, false);
		engine->total_gas = gas_limit;
		vm::load(engine, bin);
		engine->begin(0);

		try {
			engine->run();
			if(commit) {
				engine->commit();
			}
		} catch(const std::exception& ex) {
			if(verbose || !assert_fail) {
				std::cerr << "Failed at 0x" << vnx::to_hex_string(engine->error_addr)
						<< " with: " << ex.what() << " (code " << engine->error_code << ")" << std::endl;
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
			const auto exec_time_ms = (vnx::get_wall_time_micros() - time_begin) / 1e3;
			std::cerr << "Total Cost: " << engine->total_cost << std::endl;
			std::cerr << "Execution Time: " << exec_time_ms << " ms" << std::endl;
			std::cerr << "Execution Time Cost: " << exec_time_ms / (engine->total_cost / 1e6) << " ms/MMX" << std::endl;
		}
	}

	vnx::close();

	mmx::secp256k1_free();

	return ret_value;
}

