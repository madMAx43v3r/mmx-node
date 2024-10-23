/*
 * test_execute.cpp
 *
 *  Created on: Oct 23, 2024
 *      Author: mad
 */

#include <mmx/vm/Engine.h>
#include <mmx/vm/StorageRAM.h>
#include <mmx/contract/Binary.hxx>
#include <mmx/vm_interface.h>
#include <mmx/secp256k1.hpp>

#include <vnx/vnx.h>

using namespace mmx;


int main(int argc, char** argv)
{
	mmx::secp256k1_init();

	std::map<std::string, std::string> options;
	options["f"] = "file";
	options["n"] = "repeat";
	options["file"] = "binary file";
	options["gas"] = "gas limit";
	options["assert-fail"] = "assert fail";

	vnx::write_config("log_level", 2);

	vnx::init("test_execute", argc, argv, options);

	uint64_t gas_limit = 25 * 1000 * 1000;
	std::string network = "mainnet";
	std::string file_name = "@";
	vnx::read_config("file", file_name);
	vnx::read_config("gas", gas_limit);

	try {
		std::shared_ptr<const contract::Binary> binary;
		if(file_name == "@") {
			vnx::Memory data;
			std::vector<char> buffer(4096);
			while(std::cin.read(buffer.data(), buffer.size()) || std::cin.gcount() > 0) {
				const auto bytes_read = std::cin.gcount();
				::memcpy(data.add_chunk(bytes_read), buffer.data(), bytes_read);
			}
			vnx::MemoryInputStream stream(&data);
			vnx::TypeInput in(&stream);
			vnx::read(in, binary);
		} else {
			binary = vnx::read_from_file<contract::Binary>(file_name);
		}
		if(!binary) {
			throw std::logic_error("failed to read file: " + file_name);
		}
		auto storage = std::make_shared<vm::StorageRAM>();

		auto engine = std::make_shared<vm::Engine>(hash_t("__test"), storage, false);
		engine->gas_limit = gas_limit;

		vm::load(engine, binary);

		engine->write(vm::MEM_EXTERN + vm::EXTERN_USER, vm::var_t());
		engine->write(vm::MEM_EXTERN + vm::EXTERN_ADDRESS, vm::to_binary(engine->contract));
		engine->write(vm::MEM_EXTERN + vm::EXTERN_NETWORK, vm::to_binary(network));
		engine->write(vm::MEM_EXTERN + vm::EXTERN_HEIGHT, vm::uint_t(0));
		engine->begin(0);

		try {
			engine->run();
			engine->commit();
		}
		catch(const std::exception& ex) {
			std::cerr << "Failed at " << vm::to_hex(engine->error_addr)
					<< " line " << vnx::to_string(binary->find_line(engine->error_addr))
					<< " with: " << ex.what() << " (code " << engine->error_code << ")" << std::endl;
		}
	}
	catch(const std::exception& ex) {
		std::cerr << ex.what() << std::endl;
	}

	vnx::close();

	mmx::secp256k1_free();

	return 0;
}


