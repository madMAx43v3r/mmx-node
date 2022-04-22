/*
 * var_t.h
 *
 *  Created on: Apr 21, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_VM_VAR_T_H_
#define INCLUDE_MMX_VM_VAR_T_H_

#include <uint256_t.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>


namespace mmx {
namespace vm {

enum class vartype_e : uint16_t {

	NIL,
	REF,
	UINT,
	TRUE,
	FALSE,
	STRING,
	BINARY,
	ARRAY,
	MAP,

};

enum class varflags_e : uint16_t {

	NONE = 0,
	DIRTY = 1,

};

enum class constvar_e : uint32_t {

	NIL,
	ONE,
	ZERO,
	TRUE,
	FALSE,
	STRING,
	BINARY,
	ARRAY,
	MAP,

	END = 0x100

};

struct var_t {

	vartype_e type = vartype_e::NIL;
	varflags_e flags = varflags_e::NONE;

	union {
		uint32_t address;
		uint32_t ref_count;
	};

	var_t() { ref_count = 0; }
	var_t(const var_t&) = delete;
	var_t(const vartype_e& type) : type(type) {}

	var_t& operator=(const var_t&) = delete;

};

struct uint_t : var_t {

	uint256_t value = uint256_0;

	uint_t() : var_t(vartype_e::UINT) {}
	uint_t(const uint256_t& value) : var_t(vartype_e::UINT) { this->value = value; }

};

struct string_t : var_t {

	uint32_t size = 0;

	char* data(const size_t offset = 0) {
		return ((char*)this) + sizeof(string_t) + offset;
	}
	const char* c_str(const size_t offset = 0) const {
		return ((const char*)this) + sizeof(string_t) + offset;
	}

	static string_t* alloc(const size_t size) {
		auto str = new(::operator new(sizeof(string_t) + size + 1)) string_t();
		str->size = size;
		::memset(str->data(), 0, size + 1);
		return str;
	}
	static string_t* alloc(const std::string& src) {
		auto str = alloc(src.size());
		::memcpy(str->data(), src.c_str(), src.size());
		return str;
	}

private:
	string_t() : var_t(vartype_e::STRING) {}

};

struct binary_t : var_t {

	uint32_t size = 0;

	void* data(const size_t offset = 0) {
		return ((char*)this) + sizeof(binary_t) + offset;
	}
	const void* data(const size_t offset = 0) const {
		return ((const char*)this) + sizeof(binary_t) + offset;
	}

	static binary_t* alloc(const size_t size) {
		auto bin = new(::operator new(sizeof(binary_t) + size)) binary_t();
		bin->size = size;
		::memset(bin->data(), 0, size);
		return bin;
	}

private:
	binary_t() : var_t(vartype_e::BINARY) {}

};


} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_VAR_T_H_ */
