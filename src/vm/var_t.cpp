/*
 * var_t.cpp
 *
 *  Created on: Apr 22, 2022
 *      Author: mad
 */

#include <mmx/vm/var_t.h>

#include <vnx/Util.hpp>


namespace mmx {
namespace vm {

var_t* clone(const var_t& src)
{
	switch(src.type) {
		case TYPE_NIL:
		case TYPE_TRUE:
		case TYPE_FALSE:
			return new var_t(src);
		case TYPE_REF:
			return new ref_t((const ref_t&)src);
		case TYPE_UINT:
			return new uint_t((const uint_t&)src);
		case TYPE_STRING:
		case TYPE_BINARY:
			return binary_t::clone((const binary_t&)src);
		case TYPE_ARRAY:
			return new array_t((const array_t&)src);
		case TYPE_MAP:
			return new map_t((const map_t&)src);
	}
	return nullptr;
}

var_t* clone(const var_t* var)
{
	if(var) {
		return clone(*var);
	}
	return nullptr;
}

int compare(const var_t& lhs, const var_t& rhs)
{
	if(lhs.type != rhs.type) {
		return lhs.type < rhs.type ? -1 : 1;
	}
	switch(lhs.type) {
		case TYPE_NIL:
		case TYPE_TRUE:
		case TYPE_FALSE:
			return 0;
		case TYPE_REF: {
			const auto& L = (const ref_t&)lhs;
			const auto& R = (const ref_t&)rhs;
			if(L.address == R.address) {
				return 0;
			}
			return L.address < R.address ? -1 : 1;
		}
		case TYPE_UINT: {
			const auto& L = ((const uint_t&)lhs).value;
			const auto& R = ((const uint_t&)rhs).value;
			if(L == R) {
				return 0;
			}
			return L < R ? -1 : 1;
		}
		case TYPE_STRING:
		case TYPE_BINARY: {
			const auto& L = (const binary_t&)lhs;
			const auto& R = (const binary_t&)rhs;
			if(L.size == R.size) {
				return ::memcmp(L.data(), R.data(), L.size);
			}
			return L.size < R.size ? -1 : 1;
		}
		case TYPE_ARRAY: {
			const auto& L = (const array_t&)lhs;
			const auto& R = (const array_t&)rhs;
			if(L.address == R.address) {
				return 0;
			}
			return L.address < R.address ? -1 : 1;
		}
		case TYPE_MAP: {
			const auto& L = (const map_t&)lhs;
			const auto& R = (const map_t&)rhs;
			if(L.address == R.address) {
				return 0;
			}
			return L.address < R.address ? -1 : 1;
		}
		default:
			return 0;
	}
}

std::pair<uint8_t*, size_t> serialize(const var_t& src, bool with_rc, bool with_vf)
{
	std::pair<uint8_t*, size_t> out = {nullptr, 1};
	if(with_rc) {
		out.second += 4;
	}
	if(with_vf) {
		out.second += 1;
	}
	switch(src.type) {
		case TYPE_REF:
			out.second += 8; break;
		case TYPE_UINT:
			out.second += 32; break;
		case TYPE_STRING:
		case TYPE_BINARY:
			out.second += 4 + size_t(((const binary_t&)src).size); break;
		case TYPE_ARRAY:
			out.second += 12; break;
		case TYPE_MAP:
			out.second += 8; break;
		default: break;
	}
	out.first = (uint8_t*)::malloc(out.second);

	size_t offset = 0;
	if(with_rc) {
		::memcpy(out.first + offset, &src.ref_count, 4); offset += 4;
	}
	if(with_vf) {
		::memcpy(out.first + offset, &src.flags, 1); offset += 1;
	}
	::memcpy(out.first + offset, &src.type, 1); offset += 1;

	switch(src.type) {
		case TYPE_REF:
			::memcpy(out.first + offset, &((const ref_t&)src).address, 8); offset += 8;
			break;
		case TYPE_UINT:
			::memcpy(out.first + offset, &((const uint_t&)src).value, 32); offset += 32;
			break;
		case TYPE_STRING:
		case TYPE_BINARY: {
			const auto& bin = (const binary_t&)src;
			::memcpy(out.first + offset, &bin.size, 4); offset += 4;
			::memcpy(out.first + offset, bin.data(), bin.size); offset += bin.size;
			break;
		}
		case TYPE_ARRAY:
			::memcpy(out.first + offset, &((const array_t&)src).address, 8); offset += 8;
			::memcpy(out.first + offset, &((const array_t&)src).size, 4); offset += 4;
			break;
		case TYPE_MAP:
			::memcpy(out.first + offset, &((const map_t&)src).address, 8); offset += 8;
			break;
		default: break;
	}
	return out;
}

std::pair<var_t*, size_t> deserialize(const void* data_, const size_t length, bool with_rc, bool with_vf)
{
	std::pair<var_t*, size_t> out = {nullptr, 0};

	size_t offset = 0;
	const uint8_t* data = (const uint8_t*)data_;

	uint8_t flags = 0;
	uint32_t ref_count = 0;
	if(with_rc) {
		if(length < offset + 4) {
			throw std::runtime_error("unexpected eof");
		}
		::memcpy(&ref_count, data + offset, 4); offset += 4;
	}
	if(with_vf) {
		if(length < offset + 1) {
			throw std::runtime_error("unexpected eof");
		}
		flags = data[offset]; offset += 1;
	}
	if(length < offset + 1) {
		throw std::runtime_error("unexpected eof");
	}
	const auto type = vartype_e(data[offset]); offset += 1;

	switch(type) {
		case TYPE_NIL:
		case TYPE_TRUE:
		case TYPE_FALSE:
			out.first = new var_t(type);
			break;
		case TYPE_REF: {
			if(length < offset + 8) {
				throw std::runtime_error("unexpected eof");
			}
			auto var = new ref_t();
			::memcpy(&var->address, data + offset, 8); offset += 8;
			out.first = var;
			break;
		}
		case TYPE_UINT: {
			if(length < offset + 32) {
				throw std::runtime_error("unexpected eof");
			}
			auto var = new uint_t();
			::memcpy(&var->value, data + offset, 32); offset += 32;
			out.first = var;
			break;
		}
		case TYPE_STRING:
		case TYPE_BINARY: {
			if(length < offset + 4) {
				throw std::runtime_error("unexpected eof");
			}
			uint32_t size = 0;
			::memcpy(&size, data + offset, 4); offset += 4;
			if(length < offset + size) {
				throw std::runtime_error("unexpected eof");
			}
			auto bin = binary_t::unsafe_alloc(size, type);
			bin->size = size;
			::memcpy(bin->data(), data + offset, bin->size);
			::memset(bin->data(bin->size), 0, bin->capacity - bin->size);
			offset += size;
			out.first = bin;
			break;
		}
		case TYPE_ARRAY: {
			if(length < offset + 12) {
				throw std::runtime_error("unexpected eof");
			}
			auto var = new array_t();
			::memcpy(&var->address, data + offset, 8); offset += 8;
			::memcpy(&var->size, data + offset, 8); offset += 4;
			out.first = var;
			break;
		}
		case TYPE_MAP: {
			if(length < offset + 8) {
				throw std::runtime_error("unexpected eof");
			}
			auto var = new map_t();
			::memcpy(&var->address, data + offset, 8); offset += 8;
			out.first = var;
			break;
		}
		default:
			throw std::runtime_error("invalid type");
	}
	if(auto var = out.first) {
		var->flags = flags;
		var->ref_count = ref_count;
	}
	out.second = offset;
	return out;
}

std::string to_string(const var_t* var)
{
	if(!var) {
		return "nullptr";
	}
	switch(var->type) {
		case TYPE_NIL:
			return "null";
		case TYPE_TRUE:
			return "true";
		case TYPE_FALSE:
			return "false";
		case TYPE_REF:
			return "<0x" + vnx::to_hex_string(((const ref_t*)var)->address) + ">";
		case TYPE_UINT:
			return ((const uint_t*)var)->value.str(10);
		case TYPE_STRING: {
			auto bin = (const binary_t*)var;
			return "\"" + std::string((const char*)bin->data(), bin->size) + "\"";
		}
		case TYPE_BINARY: {
			auto bin = (const binary_t*)var;
			return "0x" + vnx::to_hex_string(bin->data(), bin->size);
		}
		case TYPE_ARRAY: {
			auto array = (const array_t*)var;
			return "[0x" + vnx::to_hex_string(array->address) + "," + std::to_string(array->size) + "]";
		}
		case TYPE_MAP:
			return "{0x" + vnx::to_hex_string(((const map_t*)var)->address) + "}";
		default:
			return "?";
	}
}


} // vm
} // mmx
