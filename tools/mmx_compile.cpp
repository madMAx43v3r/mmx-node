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
#include <mmx/skey_t.hpp>
#include <mmx/pubkey_t.hpp>
#include <mmx/signature_t.hpp>
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
	std::string output;
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
		if(file_names.empty()) {
			std::string source;
			std::vector<char> buffer(4096);
			while(std::cin.read(buffer.data(), buffer.size()) || std::cin.gcount() > 0) {
				const auto bytes_read = std::cin.gcount();
				source += std::string(buffer.data(), bytes_read);
			}
			binary = vm::compile(source, flags);
		} else {
			binary = vm::compile_files(file_names, flags);
		}
	}
	catch(const std::exception& ex) {
		std::cerr << "Compilation failed with:" << std::endl << "\t" << ex.what() << std::endl;
		vnx::close();
		return 1;
	}

	if(!output.empty()) {
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
	}

	if(execute) {
		if(verbose) {
			std::cerr << "-------------------------------------------" << std::endl;
		}
		const auto time_begin = vnx::get_wall_time_micros();

		auto storage = std::make_shared<vm::StorageRAM>();
		auto engine = std::make_shared<vm::Engine>(hash_t("__test"), storage, false);
		engine->is_debug = verbose;
		engine->gas_limit = gas_limit;

		uint32_t height = 0;
		std::map<addr_t, std::shared_ptr<const Contract>> contract_map;
		std::map<addr_t, std::shared_ptr<const contract::Binary>> binary_map;

		engine->log_func = [](uint32_t level, const std::string& msg) {
			std::cout << "LOG[" << level << "] " << msg << std::endl;
		};
		engine->event_func = [&engine](const std::string& name, const uint64_t data) {
			std::cout << "EVENT[" << name << "] " << vm::read(engine, data).to_string() << std::endl;
		};
		const auto main_engine = engine;
		std::function<void(std::weak_ptr<vm::Engine>, const std::string&, const std::string&, const uint32_t)> remote_call;

		remote_call = [verbose, network, storage, &main_engine, &height, &contract_map, &binary_map, &remote_call]
			(std::weak_ptr<vm::Engine> w_engine, const std::string& name, const std::string& method, const uint32_t nargs)
		{
			const auto engine = w_engine.lock();
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
				const auto cache = std::make_shared<vm::StorageCache>(storage);
				const auto child = std::make_shared<vm::Engine>(address, cache, false);
				child->gas_limit = engine->gas_limit;
				child->log_func = [](uint32_t level, const std::string& msg) {
					std::cout << "LOG[" << level << "] " << msg << std::endl;
				};
				child->event_func = [&child](const std::string& name, const uint64_t data) {
					std::cout << "EVENT[" << name << "] " << vm::read(child, data).to_string() << std::endl;
				};
				child->remote_call = std::bind(remote_call, child, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
				child->read_contract = [&child, &contract_map]
					(const addr_t& address, const std::string& field, const uint64_t dst)
				{
					auto iter = contract_map.find(address);
					if(iter != contract_map.end()) {
						vm::assign(child, dst, iter->second->read_field(field));
					} else {
						throw std::logic_error("no such contract: " + address.to_string());
					}
				};

				bool assert_fail = false;
				vnx::optional<addr_t> user = engine->contract;
				vnx::optional<std::pair<uint128, addr_t>> deposit;

				for(uint32_t i = 0; i < nargs; ++i) {
					const auto src = stack_ptr + 1 + i;
					const auto arg = vm::read(engine, src);
					if(arg.is_object()) {
						auto opt = arg.to_object();
						if(opt["__test"]) {
							const auto& value = opt["user"];
							if(!value.empty()) {
								value.to(user);
							}
							if(const auto& value = opt["deposit"]) {
								if(value.is_object()) {
									auto obj = value.to_object();
									deposit = std::make_pair(obj["amount"].to<uint128>(), obj["currency"].to<addr_t>());
								} else {
									value.to(deposit);
								}
							}
							opt["assert_fail"].to(assert_fail);
							break;
						}
					}
					if(engine == main_engine && !arg.is_json_strict(100)) {
						throw std::logic_error("argument not strict json: " + arg.to_string());
					}
					vm::copy(child, engine, vm::MEM_STACK + 1 + i, src);
				}

				if(verbose && (!user || *user != engine->contract)) {
					std::cout << "user = " << vnx::to_string(user) << std::endl;
				}
				if(deposit) {
					const auto& amount = deposit->first;
					const auto& currency = deposit->second;
					vm::set_deposit(child, currency, amount);
					if(verbose) {
						std::cout << "deposit = " << amount.to_string() << " [" << currency.to_string() << "]" << std::endl;
					}
					const auto balance = cache->get_balance(address, currency);
					cache->set_balance(address, currency, (balance ? *balance : uint128()) + amount);
				}
				if(user) {
					child->write(vm::MEM_EXTERN + vm::EXTERN_USER, vm::to_binary(*user));
				} else {
					child->write(vm::MEM_EXTERN + vm::EXTERN_USER, vm::var_t());
				}
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
				const auto func = vm::find_method(binary, method);
				if(!func) {
					throw std::logic_error("no such method: " + method);
				}
				if(!func->is_payable && deposit) {
					throw std::logic_error("method does not allow deposit: " + method);
				}
				vm::load(child, binary);

				bool did_fail = true;
				try {
					vm::execute(child, *func, true);
					did_fail = false;
				}
				catch(const std::exception& ex) {
					if(verbose) {
						if(assert_fail) {
							std::cout << name << "." << method << "() failed as expected: "
									<< ex.what()  << " (code " << child->error_code << ")" << std::endl;
						} else {
							std::cerr << name << "." << method << "() failed at " << vm::to_hex(child->error_addr)
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
					if(verbose) {
						std::cout << name << "." << method << "() => "
								<< vm::read(child, vm::MEM_STACK).to_string() << std::endl;
					}
					for(const auto& out : child->outputs) {
						if(verbose) {
							std::cout << ">>> " << out.amount.to_string()
									<< " [" << out.contract.to_string() << "] to " << out.address.to_string() << std::endl;
						}
						const auto src_bal = cache->get_balance(address, out.contract);
						if(!src_bal || (*src_bal) < out.amount) {
							throw std::logic_error("insufficient funds");
						}
						cache->set_balance(address, out.contract, (*src_bal) - out.amount);

						const auto dst_bal = cache->get_balance(out.address, out.contract);
						cache->set_balance(out.address, out.contract, (dst_bal ? *dst_bal : uint128()) + out.amount);
					}
					for(const auto& out : child->mint_outputs) {
						if(verbose) {
							std::cout << "--> " << out.amount.to_string()
									<< " [" << out.contract.to_string() << "] to " << out.address.to_string() << std::endl;
						}
						const auto dst_bal = cache->get_balance(out.address, out.contract);
						cache->set_balance(out.address, out.contract, (dst_bal ? *dst_bal : uint128()) + out.amount);
					}
					vm::copy(engine, child, stack_ptr, vm::MEM_STACK);
					cache->commit();
				}
				return;
			}
			if(name != "__test") {
				if(method == "__deploy") {
					const auto contract = vm::read(engine, stack_ptr + 1).to<std::shared_ptr<const Contract>>();
					if(!contract) {
						throw std::logic_error("invalid __deploy()");
					}
					const addr_t address = hash_t(name);
					contract_map[address] = contract;

					bool is_fail = false;
					if(auto exec = std::dynamic_pointer_cast<const contract::Executable>(contract)) {
						engine->call(0, 3);
						uint32_t i = 1;
						for(const auto& arg : exec->init_args) {
							if(arg.is_object()) {
								auto obj = arg.to_object();
								if(obj["__test"] && obj["assert_fail"].to<bool>()) {
									is_fail = true;
								}
							}
							vm::assign(engine, engine->get_stack_ptr() + i, arg); i++;
						}
						engine->remote_call(name, exec->init_method, exec->init_args.size());
						engine->ret();
					}
					if(is_fail) {
						if(verbose) {
							std::cout << "Deployment of '" << name << "' failed as expected" << std::endl;
						}
						contract_map.erase(address);
					} else {
						if(verbose) {
							std::cout << "Deployed '" << name << "' as " << address.to_string() << std::endl << vnx::to_pretty_string(contract);
						}
						engine->write(stack_ptr, vm::to_binary(address));
					}
					return;
				}
				throw std::logic_error("invalid remote call: " + name + "." + method + "()");
			}
			if(method == "set_height") {
				height = vm::read(engine, stack_ptr + 1).to<uint32_t>();
				if(verbose) {
					std::cout << "height = " << height << std::endl;
				}
			}
			else if(method == "inc_height") {
				height += vm::read(engine, stack_ptr + 1).to<uint32_t>();
				if(verbose) {
					std::cout << "height = " << height << std::endl;
				}
			}
			else if(method == "get_height") {
				engine->write(stack_ptr, vm::uint_t(height));
			}
			else if(method == "get_balance") {
				const auto address = vm::read(engine, stack_ptr + 1).to<addr_t>();
				const auto currency = vm::read(engine, stack_ptr + 2).to<addr_t>();
				if(auto balance = storage->get_balance(address, currency)) {
					engine->write(stack_ptr, vm::uint_t(*balance));
				} else {
					engine->write(stack_ptr, vm::uint_t());
				}
			}
			else if(method == "assert") {
				if(!vm::read(engine, stack_ptr + 1)) {
					if(auto msg = vm::read(engine, stack_ptr + 2)) {
						throw std::logic_error("assert failed (" + msg.to_string_value() + ")");
					} else {
						throw std::logic_error("assert failed");
					}
				}
			}
			else if(method == "get_public_key") {
				const auto skey = vm::read(engine, stack_ptr + 1).to<skey_t>();
				engine->write(stack_ptr, vm::to_binary(pubkey_t(skey)));
			}
			else if(method == "ecdsa_sign") {
				const auto skey = vm::read(engine, stack_ptr + 1).to<skey_t>();
				const auto hash = vm::read(engine, stack_ptr + 2).to<hash_t>();
				engine->write(stack_ptr, vm::to_binary(signature_t::sign(skey, hash)));
			}
			else if(method == "send") {
				const auto dst = vm::read(engine, stack_ptr + 1);
				const auto amount = engine->read_fail<vm::uint_t>(stack_ptr + 2, vm::TYPE_UINT).value;
				const auto currency = vm::read(engine, stack_ptr + 3).to<addr_t>();
				if(amount >> 128) {
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
				const auto balance = storage->get_balance(address, currency);
				storage->set_balance(address, currency, (balance ? *balance : uint128()) + amount.lower());
				if(verbose) {
					std::cout << "--> " << amount.str(10) << " [" << currency.to_string() << "] to " << dst_name << std::endl;
				}
			}
			else if(method == "compile") {
				const auto list = vm::read(engine, stack_ptr + 1).to<std::vector<std::string>>();
				const auto flags = vm::read(engine, stack_ptr + 2).to<compile_flags_t>();
				if(verbose) {
					std::cout << "Compiling " << vnx::to_string(list) << std::endl << vnx::to_pretty_string(flags);
				}
				const auto bin = vm::compile_files(list, flags);
				const addr_t addr = bin->calc_hash();
				if(verbose) {
					std::cout << "binary = " << addr.to_string() << std::endl;
				}
				binary_map[addr] = bin;
				engine->write(stack_ptr, vm::to_binary(addr));
			}
			else {
				throw std::logic_error("invalid __test method: " + method);
			}
		};

		engine->remote_call = std::bind(
				remote_call, std::weak_ptr<vm::Engine>(engine),
				std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

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

