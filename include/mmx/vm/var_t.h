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
	uint_t(const uint_t& src) : uint_t(src.value) {}
	uint_t(const uint256_t& value) : var_t(vartype_e::UINT) { this->value = value; }

	uint_t& operator=(const uint_t& src) {
		value = src.value;
		flags |= varflags_e::DIRTY;
		return *this;
	}

};

struct binary_t : var_t {

	uint32_t size = 0;
	uint32_t capacity = 0;

	void* data(const size_t offset = 0) {
		return ((char*)this) + sizeof(binary_t) + offset;
	}
	const void* data(const size_t offset = 0) const {
		return ((const char*)this) + sizeof(binary_t) + offset;
	}

	bool assign(const binary_t& src) {
		if(capacity < src.size + (src.type == vartype_e::STRING ? 1 : 0)) {
			return false;
		}
		type = src.type;
		size = src.size;
		::memcpy(data(), src.data(), size);
		::memset(data(size), 0, capacity - size);
		flags |= varflags_e::DIRTY;
		return true;
	}
	binary_t& operator=(const binary_t& src) {
		if(!assign(src)) {
			throw std::runtime_error("binary assignment overflow");
		}
		return *this;
	}

	static binary_t* alloc(const binary_t& src) {
		auto bin = alloc_unsafe(src.size, src.type);
		bin->size = src.size;
		::memcpy(bin->data(), src.data(), size);
		return bin;
	}
	static binary_t* alloc(size_t size, const vartype_e type) {
		auto bin = alloc_unsafe(size, type);
		::memset(bin->data(), 0, bin->capacity);
		return bin;
	}
	static binary_t* alloc(const std::string& src, const vartype_e type = vartype_e::STRING) {
		if(src.size() > std::numeric_limits<uint32_t>::max()) {
			throw std::runtime_error("binary size overflow");
		}
		auto bin = alloc_unsafe(src.size(), type);
		bin->size = src.size();
		::memcpy(bin->data(), src.c_str(), bin->size);
		::memset(bin->data(bin->size), 0, bin->capacity - bin->size);
		return bin;
	}

private:
	binary_t(const vartype_e& type) : var_t(type) {}

	static binary_t* alloc_unsafe(size_t size, const vartype_e type) {
		switch(type) {
			case vartype_e::BINARY: break;
			case vartype_e::STRING: size += 1; break;
			default: throw std::logic_error("invalid binary type");
		}
		auto bin = new(::operator new(sizeof(binary_t) + size)) binary_t(type);
		bin->capacity = size;
		return bin;
	}

};


} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_VAR_T_H_ */
