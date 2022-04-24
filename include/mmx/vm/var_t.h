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

enum class vartype_e : uint8_t {

	NIL,
	TRUE,
	FALSE,
	REF,
	UINT,
	STRING,
	BINARY,
	ARRAY,
	MAP,

};

enum varflags_e : uint8_t {

	DIRTY = 1,
	DIRTY_REF = 2,
	CONST = 4,
	STORED = 8,
	DELETED = 16,
	KEY = 32,

};

enum constvar_e : uint32_t {

	NIL,
	TRUE,
	FALSE,
	ZERO,
	ONE,
	STRING,
	BINARY,
	ARRAY,
	MAP,

	END = 0x100

};

struct var_t {

	uint32_t ref_count = 0;

	varflags_e flags = 0;

	vartype_e type = vartype_e::NIL;

	var_t() = default;
	var_t(const var_t&) = delete;
	var_t(const vartype_e& type) : type(type) {}

	void addref() {
		if(ref_count == std::numeric_limits<typeof(ref_count)>::max()) {
			throw std::runtime_error("ref_count overflow");
		}
		ref_count++;
		flags |= varflags_e::DIRTY_REF;
	}
	void unref() {
		if(!ref_count) {
			throw std::logic_error("unref underflow");
		}
		ref_count--;
		flags |= varflags_e::DIRTY_REF;
	}
	var_t* pin() {
		if(!ref_count) {
			ref_count = 1;
		}
		return this;
	}

};

struct ref_t : var_t {

	uint64_t address = 0;

	ref_t() : var_t(vartype_e::REF) {}
	ref_t(uint64_t address) : ref_t(), address(address) {}

	ref_t& operator=(const ref_t& rhs) {
		address = rhs.address;
		flags |= varflags_e::DIRTY;
		return *this;
	}

};

struct uint_t : var_t {

	uint256_t value = uint256_0;

	uint_t() : var_t(vartype_e::UINT) {}
	uint_t(const uint256_t& value) : uint_t() { this->value = value; }

	uint_t& operator=(const uint_t& rhs) {
		value = rhs.value;
		flags |= varflags_e::DIRTY;
		return *this;
	}

};

struct binary_t : var_t {

	size_t size = 0;
	size_t capacity = 0;

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
	binary_t& operator=(const binary_t& rhs) {
		if(!assign(rhs)) {
			throw std::runtime_error("binary assignment overflow");
		}
		return *this;
	}

	static binary_t* alloc(const binary_t& src) {
		auto bin = unsafe_alloc(src.size, src.type);
		bin->size = src.size;
		::memcpy(bin->data(), src.data(), size);
		return bin;
	}
	static binary_t* alloc(size_t size, const vartype_e type) {
		auto bin = unsafe_alloc(size, type);
		::memset(bin->data(), 0, bin->capacity);
		return bin;
	}
	static binary_t* alloc(const std::string& src, const vartype_e type = vartype_e::STRING) {
		auto bin = unsafe_alloc(src.size(), type);
		bin->size = src.size();
		::memcpy(bin->data(), src.c_str(), bin->size);
		::memset(bin->data(bin->size), 0, bin->capacity - bin->size);
		return bin;
	}
	static binary_t* unsafe_alloc(size_t size, const vartype_e type) {
		switch(type) {
			case vartype_e::BINARY: break;
			case vartype_e::STRING: size += 1; break;
			default: throw std::logic_error("invalid binary type");
		}
		auto bin = new(::operator new(sizeof(binary_t) + size)) binary_t(type);
		bin->capacity = size;
		return bin;
	}

private:
	binary_t(const vartype_e& type) : var_t(type) {}

};

struct array_t : ref_t {

	uint32_t size = 0;

	array_t() : var_t(vartype_e::ARRAY) {}
	array_t(uint32_t size) : array_t(), size(size) {}

};

struct map_t : ref_t {

	map_t() : var_t(vartype_e::MAP) {}

};

struct varptr_t {

	var_t* ptr = nullptr;

	varptr_t() = default;
	varptr_t(var_t* var) {
		ptr = var;
		if(ptr) {
			ptr->addref();
		}
	}
	varptr_t(const varptr_t& lhs) : varptr_t(lhs.ptr) {}

	~varptr_t() {
		if(ptr) {
			ptr->unref();
			if(!ptr->ref_count) {
				delete ptr;
			}
			ptr = nullptr;
		}
	}
	varptr_t& operator=(const varptr_t& lhs) {
		if(ptr) {
			ptr->unref();
		}
		ptr = lhs.ptr;
		if(ptr) {
			ptr->addref();
		}
		return *this;
	}
	var_t* get() const {
		return ptr;
	}

};


inline int compare(const var_t& lhs, const var_t& rhs)
{
	if(lhs.type != rhs.type) {
		return lhs.type < rhs.type ? -1 : 1;
	}
	switch(lhs.type) {
		case vartype_e::NIL:
		case vartype_e::TRUE:
		case vartype_e::FALSE:
			return 0;
		case vartype_e::REF: {
			const auto& L = (const ref_t&)lhs;
			const auto& R = (const ref_t&)rhs;
			if(L.address == R.address) {
				return 0;
			}
			return L.address < R.address ? -1 : 1;
		}
		case vartype_e::UINT: {
			const auto& L = ((const uint_t&)lhs).value;
			const auto& R = ((const uint_t&)rhs).value;
			if(L == R) {
				return 0;
			}
			return L < R ? -1 : 1;
		}
		case vartype_e::STRING:
		case vartype_e::BINARY: {
			const auto& L = (const binary_t&)lhs;
			const auto& R = (const binary_t&)rhs;
			if(L.size == R.size) {
				return ::memcmp(L.data(), R.data(), L.size);
			}
			return L.size < R.size ? -1 : 1;
		}
		default:
			return 0;
	}
}

struct varptr_less_t {
	bool operator()(const var_t*& L, const var_t*& R) const {
		if(!L) { return R; }
		if(!R) { return false; }
		return compare(*L, *R) < 0;
	}
};

inline bool operator<(const var_t& L, const var_t& R) const {
	return compare(L, R) < 0;
}
inline bool operator>(const var_t& L, const var_t& R) const {
	return compare(L, R) > 0;
}
inline bool operator==(const var_t& L, const var_t& R) const {
	return compare(L, R) == 0;
}
inline bool operator!=(const var_t& L, const var_t& R) const {
	return compare(L, R) != 0;
}
inline bool operator<(const varptr_t& L, const varptr_t& R) const {
	return varptr_less_t{}(L.ptr, R.ptr);
}

inline size_t num_bytes(const var_t& var)
{
	switch(var.type) {
		case vartype_e::REF:
			return 8;
		case vartype_e::UINT:
			return sizeof(uint256_t);
		case vartype_e::STRING:
		case vartype_e::BINARY:
			return ((const binary_t&)var).size;
		case vartype_e::ARRAY:
			return 4;
	}
	return 0;
}

inline size_t num_bytes(const var_t* var)
{
	if(!var) {
		return 0;
	}
	return num_bytes(*var);
}




} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_VAR_T_H_ */
