/*
 * var_t.h
 *
 *  Created on: Apr 21, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_VM_VAR_T_H_
#define INCLUDE_MMX_VM_VAR_T_H_

#include <vnx/Util.h>
#include <uint256_t.h>

#include <limits>
#include <cstdint>
#include <cstdlib>
#include <cstring>


namespace mmx {
namespace vm {

static constexpr uint64_t SEG_SIZE = 0x4000000;
static constexpr uint64_t STACK_SIZE = 16 * SEG_SIZE;

static constexpr uint64_t MEM_CONST = 0;
static constexpr uint64_t MEM_EXTERN = MEM_CONST + SEG_SIZE;
static constexpr uint64_t MEM_STACK = MEM_EXTERN + SEG_SIZE;
static constexpr uint64_t MEM_STATIC = MEM_STACK + STACK_SIZE;
static constexpr uint64_t MEM_HEAP = uint64_t(1) << 32;

static constexpr uint64_t STATIC_SIZE = MEM_HEAP - MEM_STATIC;

static constexpr uint8_t FLAG_DIRTY = (1 << 0);
static constexpr uint8_t FLAG_CONST = (1 << 1);
static constexpr uint8_t FLAG_STORED = (1 << 2);
static constexpr uint8_t FLAG_DELETED = (1 << 3);
static constexpr uint8_t FLAG_KEY = (1 << 4);

enum vartype_e : uint8_t {

	TYPE_NIL,
	TYPE_FALSE,
	TYPE_TRUE,
	TYPE_REF,
	TYPE_UINT,
	TYPE_STRING,
	TYPE_BINARY,
	TYPE_ARRAY,
	TYPE_MAP,

	TYPE_UINT16,
	TYPE_UINT32,
	TYPE_UINT64,
	TYPE_UINT128,
	TYPE_UINT256,

};

struct var_t {

	uint32_t ref_count = 0;

	uint8_t flags = 0;

	vartype_e type = TYPE_NIL;

	var_t() = default;
	var_t(const vartype_e& type) : type(type) {}
	var_t(const vartype_e& type, const uint8_t& flags) : flags(flags), type(type) {}

	explicit var_t(bool value) {
		type = value ? TYPE_TRUE : TYPE_FALSE;
	}
	void addref() {
		if(ref_count == std::numeric_limits<decltype(ref_count)>::max()) {
			throw std::runtime_error("addref() overflow");
		}
		ref_count++;
		flags |= FLAG_DIRTY;
	}
	bool unref() {
		if(!ref_count) {
			throw std::logic_error("unref() underflow");
		}
		ref_count--;
		flags |= FLAG_DIRTY;
		return ref_count == 0;
	}
	var_t* pin() {
		if(!ref_count) {
			ref_count = 1;
			flags |= FLAG_DIRTY;
		}
		return this;
	}

};

struct ref_t : var_t {

	uint64_t address = 0;

	ref_t() : var_t(TYPE_REF) {}
	ref_t(const ref_t&) = default;
	ref_t(uint64_t address) : ref_t() { this->address = address; }

};

struct uint_t : var_t {

	uint256_t value = uint256_0;

	uint_t() : var_t(TYPE_UINT) {}
	uint_t(const uint_t&) = default;
	uint_t(const uint256_t& value) : uint_t() { this->value = value; }

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
	const char* c_str() const {
		return (const char*)data();
	}
	std::string to_string() const {
		return std::string(c_str(), size);
	}
	std::string to_hex_string() const {
		return vnx::to_hex_string(c_str(), size, false);
	}

	static binary_t* clone(const binary_t& src) {
		auto bin = alloc(src);
		bin->ref_count = src.ref_count;
		bin->flags = src.flags;
		return bin;
	}
	static binary_t* alloc(const binary_t& src) {
		return alloc(src, src.type);
	}
	static binary_t* alloc(const binary_t& src, const vartype_e type) {
		auto bin = unsafe_alloc(src.size, type);
		bin->size = src.size;
		::memcpy(bin->data(), src.data(), bin->size);
		::memset(bin->data(bin->size), 0, bin->capacity - bin->size);
		return bin;
	}
	static binary_t* alloc(const std::string& src, const vartype_e type = TYPE_STRING) {
		auto bin = unsafe_alloc(src.size(), type);
		bin->size = src.size();
		::memcpy(bin->data(), src.c_str(), bin->size);
		::memset(bin->data(bin->size), 0, bin->capacity - bin->size);
		return bin;
	}
	static binary_t* alloc(const void* data, const size_t size, const vartype_e type = TYPE_BINARY) {
		auto bin = unsafe_alloc(size, type);
		bin->size = size;
		::memcpy(bin->data(), data, bin->size);
		::memset(bin->data(bin->size), 0, bin->capacity - bin->size);
		return bin;
	}
	static binary_t* alloc(size_t size, const vartype_e type) {
		auto bin = unsafe_alloc(size, type);
		::memset(bin->data(), 0, bin->capacity);
		return bin;
	}
	static binary_t* unsafe_alloc(size_t size, const vartype_e type) {
		if(size >= std::numeric_limits<uint32_t>::max()) {
			throw std::logic_error("binary size overflow");
		}
		switch(type) {
			case TYPE_BINARY: break;
			case TYPE_STRING: size += 1; break;
			default: throw std::logic_error("invalid binary type");
		}
		auto bin = new(::operator new(sizeof(binary_t) + size)) binary_t(type);
		bin->capacity = size;
		return bin;
	}

private:
	binary_t(const vartype_e& type) : var_t(type) {}

};

struct array_t : var_t {

	uint64_t address = 0;
	uint32_t size = 0;

	array_t() : var_t(TYPE_ARRAY) {}
	array_t(const array_t&) = default;
	array_t(uint32_t size) : array_t() { this->size = size; }

};

struct map_t : var_t {

	uint64_t address = 0;

	map_t() : var_t(TYPE_MAP) {}
	map_t(const map_t&) = default;

};

var_t* clone(const var_t& src);

var_t* clone(const var_t* var);

int compare(const var_t& lhs, const var_t& rhs);

int compare(const var_t* lhs, const var_t* rhs);

std::pair<uint8_t*, size_t> serialize(const var_t& src, bool with_rc = true, bool with_vf = true);

size_t deserialize(var_t*& var, const void* data, const size_t length, bool with_rc = true, bool with_vf = true);

std::string to_string(const var_t* var);

uint256_t to_uint(const var_t* var);

struct varptr_less_t {
	bool operator()(const var_t* const& lhs, const var_t* const& rhs) const {
		return compare(lhs, rhs) < 0;
	}
};

inline bool operator<(const var_t& lhs, const var_t& rhs) {
	return compare(lhs, rhs) < 0;
}
inline bool operator>(const var_t& lhs, const var_t& rhs) {
	return compare(lhs, rhs) > 0;
}
inline bool operator==(const var_t& lhs, const var_t& rhs) {
	return compare(lhs, rhs) == 0;
}
inline bool operator!=(const var_t& lhs, const var_t& rhs) {
	return compare(lhs, rhs) != 0;
}

inline size_t num_bytes(const var_t& var) {
	switch(var.type) {
		case TYPE_REF:
			return 8;
		case TYPE_UINT:
			return 32;
		case TYPE_STRING:
		case TYPE_BINARY:
			return ((const binary_t&)var).size;
		case TYPE_ARRAY:
			return 8 + 4;
		case TYPE_MAP:
			return 8;
		default:
			return 0;
	}
}

inline size_t num_bytes(const var_t* var) {
	return var ? num_bytes(*var) : 0;
}




} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_VAR_T_H_ */
