/*
 * mmx_compile.cpp
 *
 *  Created on: Apr 19, 2023
 *      Author: mad
 */

#include <mmx/Transaction.hxx>
#include <mmx/vm/Compiler.h>

#include <vnx/vnx.h>

using namespace mmx;


int main(int argc, char** argv)
{
	std::map<std::string, std::string> options;
	options["f"] = "files";
	options["o"] = "output";
	options["t"] = "txmode";
	options["d"] = "debug";
	options["files"] = "source files";
	options["output"] = "output name";

	vnx::write_config("log_level", 2);

	vnx::init("mmx_compile", argc, argv, options);

	vm::compile_flags_t flags;

	bool txmode = false;
	std::string output = "binary.dat";
	std::vector<std::string> file_names;
	vnx::read_config("debug", flags.debug);
	vnx::read_config("txmode", txmode);
	vnx::read_config("output", output);
	vnx::read_config("files", file_names);

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

	vnx::close();

	return 0;
}

