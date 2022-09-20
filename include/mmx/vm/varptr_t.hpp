/*
 * varptr_t.hpp
 *
 *  Created on: May 12, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_VM_VARPTR_T_HPP_
#define INCLUDE_MMX_VM_VARPTR_T_HPP_

#include <mmx/vm/var_t.h>

#include <vnx/Input.h>
#include <vnx/Output.h>
#include <vnx/Visitor.h>


namespace mmx {
namespace vm {

class varptr_t {
public:
	varptr_t() = default;

	varptr_t(var_t* var) {
		ptr = var;
		if(ptr) {
			ptr->addref();
		}
	}

	varptr_t(const varptr_t& other) : varptr_t(other.ptr) {}

	~varptr_t() {
		if(ptr) {
			if(ptr->unref()) {
				delete ptr;
			}
			ptr = nullptr;
		}
	}

	varptr_t& operator=(const varptr_t& other) {
		if(ptr) {
			ptr->unref();
		}
		ptr = other.ptr;
		if(ptr) {
			ptr->addref();
		}
		return *this;
	}

	var_t* get() const {
		return ptr;
	}

	std::string to_string() const {
		return vm::to_string(ptr);
	}

	uint256_t to_uint() const {
		return vm::to_uint(ptr);
	}

	operator bool() const {
		return ptr;
	}

	var_t& operator*() {
		return *ptr;
	}

	const var_t& operator*() const {
		return *ptr;
	}

	var_t* operator->() {
		return ptr;
	}

	const var_t* operator->() const {
		return ptr;
	}

private:
	var_t* ptr = nullptr;

};

inline bool operator<(const varptr_t& lhs, const varptr_t& rhs) {
	return compare(lhs.get(), rhs.get()) < 0;
}

inline bool operator>(const varptr_t& lhs, const varptr_t& rhs) {
	return compare(lhs.get(), rhs.get()) > 0;
}

inline bool operator==(const varptr_t& lhs, const varptr_t& rhs) {
	return compare(lhs.get(), rhs.get()) == 0;
}

inline bool operator!=(const varptr_t& lhs, const varptr_t& rhs) {
	return compare(lhs.get(), rhs.get()) != 0;
}


} // vm
} // mmx


namespace vnx {

void read(vnx::TypeInput& in, mmx::vm::varptr_t& value, const vnx::TypeCode* type_code, const uint16_t* code);
void write(vnx::TypeOutput& out, const mmx::vm::varptr_t& value, const vnx::TypeCode* type_code, const uint16_t* code);

void read(std::istream& in, mmx::vm::varptr_t& value);
void write(std::ostream& out, const mmx::vm::varptr_t& value);

void accept(vnx::Visitor& visitor, const mmx::vm::varptr_t& value);

} // vnx

#endif /* INCLUDE_MMX_VM_VARPTR_T_HPP_ */
