/*
 * utils.cpp
 *
 *  Created on: Oct 24, 2024
 *      Author: mad
 */

#include <mmx/utils.h>

#include <vnx/vnx.h>


namespace mmx {

static bool validate_json(vnx::TypeInput& in, const uint16_t* code, const size_t max_recursion, size_t call_depth)
{
	if(++call_depth > max_recursion) {
		return false;
	}
	switch(code[0]) {
		case vnx::CODE_NULL:
		case vnx::CODE_BOOL:
		case vnx::CODE_UINT8:
		case vnx::CODE_UINT16:
		case vnx::CODE_UINT32:
		case vnx::CODE_UINT64:
		case vnx::CODE_INT8:
		case vnx::CODE_INT16:
		case vnx::CODE_INT32:
		case vnx::CODE_INT64:
		case vnx::CODE_STRING:
		case vnx::CODE_ALT_BOOL:
		case vnx::CODE_ALT_UINT8:
		case vnx::CODE_ALT_UINT16:
		case vnx::CODE_ALT_UINT32:
		case vnx::CODE_ALT_UINT64:
		case vnx::CODE_ALT_INT8:
		case vnx::CODE_ALT_INT16:
		case vnx::CODE_ALT_INT32:
		case vnx::CODE_ALT_INT64:
		case vnx::CODE_ALT_STRING:
			vnx::skip(in, nullptr, code);
			return true;
		case vnx::CODE_LIST:
		case vnx::CODE_ALT_LIST: {
			switch(code[1]) {
				case vnx::CODE_DYNAMIC:
				case vnx::CODE_ALT_DYNAMIC:
					break;
				default:
					return false;
			}
			uint32_t size;
			read(in, size);
			if(code[0] == vnx::CODE_ALT_LIST) {
				size = vnx::flip_bytes(size);
			}
			for(uint32_t i = 0; i < size; ++i) {
				if(!validate_json(in, code + 1, max_recursion, call_depth)) {
					return false;
				}
			}
			return true;
		}
		case vnx::CODE_OBJECT:
		case vnx::CODE_ALT_OBJECT: {
			uint32_t size;
			read(in, size);
			if(code[0] == vnx::CODE_ALT_OBJECT) {
				size = vnx::flip_bytes(size);
			}
			for(uint32_t i = 0; i < size; ++i) {
				const uint16_t key_code = (code[0] == vnx::CODE_OBJECT ? vnx::CODE_STRING : vnx::CODE_ALT_STRING);
				const uint16_t value_code = (code[0] == vnx::CODE_OBJECT ? vnx::CODE_DYNAMIC : vnx::CODE_ALT_DYNAMIC);
				vnx::skip(in, nullptr, &key_code);
				if(!validate_json(in, &value_code, max_recursion, call_depth)) {
					return false;
				}
			}
			return true;
		}
		case vnx::CODE_DYNAMIC:
		case vnx::CODE_ALT_DYNAMIC: {
			uint16_t byte_code[VNX_MAX_BYTE_CODE_SIZE];
			vnx::read_byte_code(in, byte_code);
			return validate_json(in, byte_code, max_recursion, call_depth);
		}
	}
	return false;
}

bool is_json(const vnx::Variant& var)
{
	if(var.empty()) {
		return true;
	}
	vnx::VectorInputStream stream(&var.data);
	vnx::TypeInput in(&stream);
	const uint16_t code = vnx::CODE_DYNAMIC;
	return validate_json(in, &code, 100, 0);
}

uint64_t get_num_bytes(const vnx::Variant& var)
{
	if(var.is_null() || var.is_bool()) {
		return 1;
	} else if(var.is_long() || var.is_ulong()) {
		return 8;
	} else if(var.is_string()) {
		return 4 + var.to<std::string>().size();
	} else if(var.is_array()) {
		uint64_t total = 4;
		for(const auto& entry : var.to<std::vector<vnx::Variant>>()) {
			total += get_num_bytes(entry);
		}
		return total;
	} else if(var.is_object()) {
		uint64_t total = 4;
		for(const auto& entry : var.to_object().get_fields()) {
			total += entry.first.size() + get_num_bytes(entry.second);
		}
		return total;
	}
	return var.size();
}


} // mmx
