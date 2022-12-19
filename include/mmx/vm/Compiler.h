/*
 * Compiler.h
 *
 *  Created on: Dec 19, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_VM_COMPILER_H_
#define INCLUDE_MMX_VM_COMPILER_H_

#include <mmx/contract/Binary.hxx>


namespace mmx {
namespace vm {

std::shared_ptr<const contract::Binary> compile(const std::string& source);



} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_COMPILER_H_ */
