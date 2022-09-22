/*
 * vm_interface.h
 *
 *  Created on: May 9, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_VM_INTERFACE_H_
#define INCLUDE_MMX_VM_INTERFACE_H_

#include <mmx/vm/Engine.h>
#include <mmx/contract/Binary.hxx>

#include <vnx/Visitor.h>
#include <vnx/Variant.hpp>


namespace mmx {
namespace vm {

const contract::method_t* find_method(std::shared_ptr<const contract::Binary> binary, const std::string& method_name);

void set_balance(std::shared_ptr<vm::Engine> engine, const std::map<addr_t, uint128>& balance);

void set_deposit(std::shared_ptr<vm::Engine> engine, const txout_t& deposit);

std::vector<vm::var_t*> read_constants(std::shared_ptr<const contract::Binary> binary);

void load(	std::shared_ptr<vm::Engine> engine,
			std::shared_ptr<const contract::Binary> binary);

void copy(std::shared_ptr<vm::Engine> dst, std::shared_ptr<vm::Engine> src, const uint64_t dst_addr, const uint64_t src_addr);

void assign(std::shared_ptr<vm::Engine> engine, const uint64_t dst, const vnx::Variant& value);

vnx::Variant read(std::shared_ptr<vm::Engine> engine, const uint64_t address);

void set_args(std::shared_ptr<vm::Engine> engine, const std::vector<vnx::Variant>& args);

void execute(std::shared_ptr<vm::Engine> engine, const contract::method_t& method);


} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_INTERFACE_H_ */
