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

std::unique_ptr<var_t> clone(const var_t& src)
{
	switch(src.type) {
		case TYPE_NIL:
		case TYPE_TRUE:
		case TYPE_FALSE:
			return std::make_unique<var_t>(src);
		case TYPE_REF:
			return std::make_unique<ref_t>((const ref_t&)src);
		case TYPE_UINT:
			return std::make_unique<uint_t>((const uint_t&)src);
		case TYPE_STRING:
		case TYPE_BINARY:
			return binary_t::clone((const binary_t&)src);
		case TYPE_ARRAY:
			return std::make_unique<array_t>((const array_t&)src);
		case TYPE_MAP:
			return std::make_unique<map_t>((const map_t&)src);
		default:
			return nullptr;
	}
}

std::unique_ptr<var_t> clone(const var_t* var)
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

int compare(const var_t* lhs, const var_t* rhs)
{
	if(lhs && rhs) {
		return compare(*lhs, *rhs);
	}
	if(!lhs && !rhs) {
		return 0;
	}
	return rhs ? -1 : 1;
}

std::pair<std::unique_ptr<uint8_t[]>, size_t> serialize(const var_t& src, bool with_rc, bool with_vf)
{
	size_t length = 1;
	if(with_rc) {
		length += 4;
	}
	if(with_vf) {
		length += 1;
	}
	switch(src.type) {
		case TYPE_REF:
			length += 8; break;
		case TYPE_UINT:
			length += 32; break;
		case TYPE_STRING:
		case TYPE_BINARY:
			length += 4 + size_t(((const binary_t&)src).size); break;
		case TYPE_ARRAY:
			length += 12; break;
		case TYPE_MAP:
			length += 8; break;
		default: break;
	}
	auto data = std::make_unique<uint8_t[]>(length);

	size_t offset = 0;
	if(with_rc) {
		::memcpy(data.get() + offset, &src.ref_count, 4); offset += 4;
	}
	if(with_vf) {
		::memcpy(data.get() + offset, &src.flags, 1); offset += 1;
	}
	::memcpy(data.get() + offset, &src.type, 1); offset += 1;

	switch(src.type) {
		case TYPE_REF:
			::memcpy(data.get() + offset, &((const ref_t&)src).address, 8); offset += 8;
			break;
		case TYPE_UINT: {
			const auto& val_256 = ((const uint_t&)src).value;
			if(val_256.upper()) {
				data[offset - 1] = TYPE_UINT256;
				::memcpy(data.get() + offset, &val_256, 32); offset += 32;
			} else {
				const auto& val_128 = val_256.lower();
				if(val_128.upper()) {
					data[offset - 1] = TYPE_UINT128;
					::memcpy(data.get() + offset, &val_128, 16); offset += 16;
				} else {
					const auto& val_64 = val_128.lower();
					if(val_64 >> 32) {
						data[offset - 1] = TYPE_UINT64;
						::memcpy(data.get() + offset, &val_64, 8); offset += 8;
					} else {
						const uint32_t val_32 = val_64;
						if(val_32 >> 16) {
							data[offset - 1] = TYPE_UINT32;
							::memcpy(data.get() + offset, &val_32, 4); offset += 4;
						} else {
							const uint16_t val_16 = val_32;
							data[offset - 1] = TYPE_UINT16;
							::memcpy(data.get() + offset, &val_16, 2); offset += 2;
						}
					}
				}
			}
			break;
		}
		case TYPE_STRING:
		case TYPE_BINARY: {
			const auto& bin = (const binary_t&)src;
			::memcpy(data.get() + offset, &bin.size, 4); offset += 4;
			::memcpy(data.get() + offset, bin.data(), bin.size); offset += bin.size;
			break;
		}
		case TYPE_ARRAY:
			::memcpy(data.get() + offset, &((const array_t&)src).address, 8); offset += 8;
			::memcpy(data.get() + offset, &((const array_t&)src).size, 4); offset += 4;
			break;
		case TYPE_MAP:
			::memcpy(data.get() + offset, &((const map_t&)src).address, 8); offset += 8;
			break;
		default:
			break;
	}
	return std::make_pair(std::move(data), offset);
}

size_t deserialize(std::unique_ptr<var_t>& out, const void* data_, const size_t length, bool with_rc, bool with_vf)
{
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
			out = std::make_unique<var_t>(type);
			break;
		case TYPE_REF: {
			if(length < offset + 8) {
				throw std::runtime_error("unexpected eof");
			}
			auto var = std::make_unique<ref_t>();
			::memcpy(&var->address, data + offset, 8); offset += 8;
			out = std::move(var);
			break;
		}
		case TYPE_UINT:
		case TYPE_UINT16:
		case TYPE_UINT32:
		case TYPE_UINT64:
		case TYPE_UINT128:
		case TYPE_UINT256: {
			size_t size = 32;
			switch(type) {
				case TYPE_UINT16:  size = 2; break;
				case TYPE_UINT32:  size = 4; break;
				case TYPE_UINT64:  size = 8; break;
				case TYPE_UINT128: size = 16; break;
				default: break;
			}
			if(length < offset + size) {
				throw std::runtime_error("unexpected eof");
			}
			auto var = std::make_unique<uint_t>();
			switch(type) {
				case TYPE_UINT16: {
					uint16_t tmp = 0;
					::memcpy(&tmp, data + offset, size);
					var->value = tmp;
					break;
				}
				case TYPE_UINT32: {
					uint32_t tmp = 0;
					::memcpy(&tmp, data + offset, size);
					var->value = tmp;
					break;
				}
				case TYPE_UINT64: {
					uint64_t tmp = 0;
					::memcpy(&tmp, data + offset, size);
					var->value = tmp;
					break;
				}
				case TYPE_UINT128: {
					uint128_t tmp = 0;
					::memcpy(&tmp, data + offset, size);
					var->value = uint256_t(tmp);
					break;
				}
				default:
					::memcpy(&var->value, data + offset, size);
			}
			offset += size;
			out = std::move(var);
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
			::memcpy(bin->data(), data + offset, bin->size); offset += size;
			::memset(bin->data(bin->size), 0, bin->capacity - bin->size);
			out = std::move(bin);
			break;
		}
		case TYPE_ARRAY: {
			if(length < offset + 12) {
				throw std::runtime_error("unexpected eof");
			}
			auto var = std::make_unique<array_t>();
			::memcpy(&var->address, data + offset, 8); offset += 8;
			::memcpy(&var->size, data + offset, 4); offset += 4;
			out = std::move(var);
			break;
		}
		case TYPE_MAP: {
			if(length < offset + 8) {
				throw std::runtime_error("unexpected eof");
			}
			auto var = std::make_unique<map_t>();
			::memcpy(&var->address, data + offset, 8); offset += 8;
			out = std::move(var);
			break;
		}
		default:
			throw std::runtime_error("invalid type: " + std::to_string(int(type)));
	}
	if(out) {
		out->flags = flags;
		out->ref_count = ref_count;
	}
	return offset;
}


} // vm
} // mmx
