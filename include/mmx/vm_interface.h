/*
 * vm_interface.h
 *
 *  Created on: May 9, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_VM_INTERFACE_H_
#define INCLUDE_MMX_VM_INTERFACE_H_

#include <mmx/vm/Engine.h>
#include <mmx/contract/Executable.hxx>

#include <vnx/Visitor.h>
#include <vnx/Variant.hpp>


namespace mmx {

const contract::method_t* find_method(std::shared_ptr<const contract::Executable> executable, const std::string& method_name);

void set_balance(std::shared_ptr<vm::Engine> engine, const std::map<addr_t, uint128>& balance);

void set_deposit(std::shared_ptr<vm::Engine> engine, const txout_t& deposit);

std::vector<vm::varptr_t> get_constants(const void* constant, const size_t constant_size);

std::vector<vm::varptr_t> get_constants(std::shared_ptr<const contract::Executable> exec);

void load(	std::shared_ptr<vm::Engine> engine,
			std::shared_ptr<const contract::Executable> exec);

void assign(std::shared_ptr<vm::Engine> engine, const uint64_t dst, const vnx::Variant& value);

void execute(	std::shared_ptr<vm::Engine> engine,
				const contract::method_t& method,
				const std::vector<vnx::Variant>& args);


} // mmx

#endif /* INCLUDE_MMX_VM_INTERFACE_H_ */
