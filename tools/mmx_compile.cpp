/*
 * mmx_compile.cpp
 *
 *  Created on: Apr 19, 2023
 *      Author: mad
 */

#include <mmx/Transaction.hxx>
#include <mmx/contract/Binary.hxx>
#include <mmx/contract/Executable.hxx>
#include <mmx/vm/Compiler.h>
#include <mmx/vm/Engine.h>
#include <mmx/vm/StorageRAM.h>
#include <mmx/vm/StorageCache.h>
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
	options["n"] = "network";
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
	std::string network = "mainnet";
	std::string output = "binary.dat";
	std::vector<std::string> file_names;
	vnx::read_config("verbose", verbose);
	vnx::read_config("opt-level", opt_level);
	vnx::read_config("txmode", txmode);
	vnx::read_config("execute", execute);
	vnx::read_config("commit", commit);
	vnx::read_config("network", network);
	vnx::read_config("output", output);
	vnx::read_config("files", file_names);
	vnx::read_config("gas", gas_limit);
	vnx::read_config("assert-fail", assert_fail);

	flags.verbose = verbose;
	flags.opt_level = opt_level;

	int ret_value = 0;
	std::shared_ptr<const contract::Binary> binary;

	try {
		binary = vm::compile_files(file_names, flags);
	}
	catch(const std::exception& ex) {
		std::cerr << "Compilation failed with:" << std::endl << "\t" << ex.what() << std::endl;
		vnx::close();
		return 1;
	}

	if(txmode) {
		auto tx = Transaction::create();
		tx->deploy = binary;
		tx->network = network;
		tx->id = tx->calc_hash(false);
		tx->content_hash = tx->calc_hash(true);
		vnx::write_to_file(output, tx);
		std::cout << addr_t(tx->id).to_string() << std::endl;
	} else {
		vnx::write_to_file(output, binary);
	}

	if(execute) {
		const auto time_begin = vnx::get_wall_time_micros();

		auto storage = std::make_shared<vm::StorageRAM>();
		auto engine = std::make_shared<vm::Engine>(hash_t("__test"), storage, false);
		engine->is_debug = verbose;
		engine->gas_limit = gas_limit;

		uint32_t height = 1;
		std::map<addr_t, std::shared_ptr<const Contract>> contract_map;
		std::map<addr_t, std::shared_ptr<const contract::Binary>> binary_map;
		std::map<std::pair<addr_t, addr_t>, uint128> balance_map;

		engine->log_func = [](uint32_t level, const std::string& msg) {
			std::cout << "LOG[" << level << "] " << msg << std::endl;
		};
		engine->event_func = [&engine](const std::string& name, const uint64_t data) {
			std::cout << "EVENT[" << name << "] " << vm::read(engine, data).to_string() << std::endl;
		};
		engine->remote_call = [verbose, network, &engine, &storage, &height, &contract_map, &binary_map, &balance_map]
			(const std::string& name, const std::string& method, const uint32_t nargs)
		{
			const addr_t address = hash_t(name);
			std::shared_ptr<const contract::Executable> exec;
			{
				auto iter = contract_map.find(address);
				if(iter != contract_map.end()) {
					exec = std::dynamic_pointer_cast<const contract::Executable>(iter->second);
				}
			}
			const auto stack_ptr = engine->get_stack_ptr();
			engine->write(stack_ptr, vm::var_t());

			if(exec) {
				const auto child = std::make_shared<vm::Engine>(address, storage, false);
				child->gas_limit = engine->gas_limit;
				child->log_func = engine->log_func;
				child->event_func = engine->event_func;
				child->remote_call = engine->remote_call;
				child->read_contract = [&child, &contract_map]
					(const addr_t& address, const std::string& field, const uint64_t dst)
				{
					auto iter = contract_map.find(address);
					if(iter != contract_map.end()) {
						vm::assign(child, dst, iter->second->read_field(field));
					}
					throw std::logic_error("no such contract: " + address.to_string());
				};

				auto method_name = method;
				const bool assert_fail = !method_name.empty() && method_name[0] == '!';
				if(assert_fail) {
					method_name = method_name.substr(1);
				}
				const bool is_deposit = !method_name.empty() && method_name[0] == '$';
				if(is_deposit) {
					method_name = method_name.substr(1);
				}

				for(uint32_t i = 0; i + (is_deposit ? 2 : 0) < nargs; ++i) {
					vm::copy(child, engine, vm::MEM_STACK + 1 + i, stack_ptr + 1 + i);
				}
				if(is_deposit) {
					if(nargs < 2) {
						throw std::logic_error("missing deposit arguments [amount, currency]");
					}
					const auto currency = vm::read(engine, stack_ptr + nargs - 1).to<addr_t>();
					const auto amount = engine->read_fail<vm::uint_t>(stack_ptr + nargs - 2, vm::TYPE_UINT).value;
					if(amount >> 80) {
						throw std::logic_error("amount too large");
					}
					if(!assert_fail) {
						balance_map[std::make_pair(address, currency)] += amount;
					}
					vm::set_deposit(child, currency, amount);
				}
				child->write(vm::MEM_EXTERN + vm::EXTERN_USER, vm::to_binary(engine->contract));
				child->write(vm::MEM_EXTERN + vm::EXTERN_ADDRESS, vm::to_binary(address));
				child->write(vm::MEM_EXTERN + vm::EXTERN_NETWORK, vm::to_binary(network));
				child->write(vm::MEM_EXTERN + vm::EXTERN_HEIGHT, vm::uint_t(height));
				child->write(vm::MEM_EXTERN + vm::EXTERN_TXID, vm::to_binary(addr_t()));

				std::shared_ptr<const contract::Binary> binary;
				{
					auto iter = binary_map.find(exec->binary);
					if(iter != binary_map.end()) {
						binary = iter->second;
					}
				}
				if(!binary) {
					throw std::logic_error("no such binary: " + exec->binary.to_string());
				}
				const auto func = vm::find_method(binary, method_name);
				if(!func) {
					throw std::logic_error("no such method: " + method_name);
				}
				// TODO: check is_public || is_init
				vm::load(child, binary);

				bool did_fail = true;
				try {
					vm::execute(child, *func, true);
					did_fail = false;
				}
				catch(const std::exception& ex) {
					if(verbose) {
						if(assert_fail) {
							std::cerr << "Call to " << name << "." << method_name << "() failed as expected with: "
									<< ex.what()  << " (code " << child->error_code << ")" << std::endl;
						} else {
							std::cerr << "Call to " << name << "." << method_name << "() failed at " << vm::to_hex(child->error_addr)
									<< " line " << vnx::to_string(binary->find_line(child->error_addr))
									<< " with: " << ex.what() << " (code " << child->error_code << ")" << std::endl;
						}
					}
					if(!assert_fail) {
						throw;
					}
				}
				if(assert_fail) {
					if(!did_fail) {
						throw std::logic_error("expected call to fail");
					}
				} else {
					vm::copy(engine, child, stack_ptr, vm::MEM_STACK);
				}
				return;
			}
			if(name != "__test") {
				throw std::logic_error("invalid remote call to '" + name + "'");
			}
			if(method == "set_height") {
				height = vm::read(engine, stack_ptr + 1).to<uint32_t>();
				if(verbose) {
					std::cout << "Set height to " << height << std::endl;
				}
			}
			else if(method == "get_balance") {
				const auto src = vm::read(engine, stack_ptr + 1);
				const auto currency = vm::read(engine, stack_ptr + 2).to<addr_t>();
				addr_t address;
				if(src.is_string()) {
					address = hash_t(src.to_string_value());
				} else {
					address = src.to<addr_t>();
				}
				auto iter = balance_map.find(std::make_pair(address, currency));
				if(iter != balance_map.end()) {
					engine->write(stack_ptr, vm::uint_t(iter->second));
				} else {
					engine->write(stack_ptr, vm::uint_t());
				}
			}
			else if(method == "send") {
				const auto dst = vm::read(engine, stack_ptr + 1);
				const auto currency = vm::read(engine, stack_ptr + 2).to<addr_t>();
				const auto amount = engine->read_fail<vm::uint_t>(stack_ptr + 3, vm::TYPE_UINT).value;
				if(amount >> 80) {
					throw std::logic_error("amount too large");
				}
				addr_t address;
				std::string dst_name;
				if(dst.is_string()) {
					dst_name = dst.to_string_value();
					address = hash_t(dst_name);
				} else {
					address = dst.to<addr_t>();
					dst_name = address.to_string();
				}
				balance_map[std::make_pair(address, currency)] += amount.lower();
				if(verbose) {
					std::cout << "Sent " << amount.str(10) << "[" << currency.to_string() << "] to " << dst_name << std::endl;
				}
			}
			else if(method == "compile") {
				const auto list = vm::read(engine, stack_ptr + 1).to<std::vector<std::string>>();
				const auto flags = vm::read(engine, stack_ptr + 2).to<compile_flags_t>();
				if(verbose) {
					std::cout << "Compiling " << vnx::to_string(list) << std::endl << vnx::to_pretty_string(flags);
				}
				const auto bin = vm::compile_files(list, flags);
				const auto addr = bin->calc_hash();
				if(verbose) {
					std::cout << "Done: address = " << addr.to_string() << std::endl;
				}
				binary_map[addr] = bin;
				engine->write(stack_ptr, vm::to_binary(addr));
			}
			else if(method == "deploy") {
				const auto name = vm::read(engine, stack_ptr + 1).to<std::string>();
				const auto contract = vm::read(engine, stack_ptr + 2).to<std::shared_ptr<const Contract>>();
				if(contract) {
					const addr_t address = hash_t(name);
					if(verbose) {
						std::cout << "Deployed '" << name << "' as " << address.to_string() << std::endl << vnx::to_pretty_string(contract);
					}
					contract_map[address] = contract;

					if(auto exec = std::dynamic_pointer_cast<const contract::Executable>(contract)) {
						engine->call(0, 3);
						uint32_t i = 1;
						for(const auto& arg : exec->init_args) {
							vm::assign(engine, engine->get_stack_ptr() + i, arg); i++;
						}
						engine->remote_call(name, exec->init_method, exec->init_args.size());
						engine->ret();
					}
				}
			}
			else {
				throw std::logic_error("invalid __test method: " + method);
			}
		};

		vm::load(engine, binary);

		engine->write(vm::MEM_EXTERN + vm::EXTERN_USER, vm::var_t());
		engine->write(vm::MEM_EXTERN + vm::EXTERN_ADDRESS, vm::to_binary(engine->contract));
		engine->write(vm::MEM_EXTERN + vm::EXTERN_NETWORK, vm::to_binary(network));
		engine->begin(0);

		try {
			engine->run();
			if(commit) {
				engine->commit();
				if(verbose) {
					std::cerr << "-------------------------------------------" << std::endl;
					storage->dump_memory(std::cerr);
				}
			}
		} catch(const std::exception& ex) {
			if(verbose || !assert_fail) {
				std::cerr << "Failed at " << vm::to_hex(engine->error_addr)
						<< " line " << vnx::to_string(binary->find_line(engine->error_addr))
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
			std::cerr << "-------------------------------------------" << std::endl;
			std::cerr << "Total cost: " << engine->gas_used << std::endl;
			std::cerr << "Execution time: " << exec_time_ms << " ms" << std::endl;
			std::cerr << "Execution time cost: " << exec_time_ms / (engine->gas_used / 1e6) << " ms/MMX" << std::endl;
		}
	}

	vnx::close();

	mmx::secp256k1_free();

	return ret_value;
}

