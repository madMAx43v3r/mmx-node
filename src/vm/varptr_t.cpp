/*
 * varptr_t.cpp
 *
 *  Created on: May 12, 2022
 *      Author: mad
 */

#include <mmx/vm/varptr_t.hpp>

#include <vnx/vnx.h>


namespace vnx {

void read(vnx::TypeInput& in, mmx::vm::varptr_t& value, const vnx::TypeCode* type_code, const uint16_t* code)
{
	switch(code[0]) {
		case CODE_LIST:
		case CODE_ALT_LIST: {
			std::vector<uint8_t> data;
			vnx::read(in, data, type_code, code);
			std::unique_ptr<mmx::vm::var_t> var;
			deserialize(var, data.data(), data.size(), false, false);
			value = std::move(var);
			break;
		}
		case CODE_STRING:
		case CODE_ALT_STRING: {
			std::string tmp;
			vnx::read(in, tmp, type_code, code);
			value = mmx::vm::to_binary(tmp);
			break;
		}
		case CODE_DYNAMIC:
		case CODE_ALT_DYNAMIC: {
			vnx::Variant tmp;
			vnx::read(in, tmp, type_code, code);
			std::unique_ptr<mmx::vm::var_t> var;
			const auto data = tmp.to<std::vector<uint8_t>>();
			deserialize(var, data.data(), data.size(), false, false);
			value = std::move(var);
			break;
		}
		default:
			vnx::skip(in, type_code, code);
	}
}

void write(vnx::TypeOutput& out, const mmx::vm::varptr_t& value, const vnx::TypeCode* type_code, const uint16_t* code)
{
	auto var = value;
	if(!var) {
		var = std::make_unique<mmx::vm::var_t>();
	}
	const auto data = mmx::vm::serialize(*var, false, false);
	write(out, uint32_t(data.second));
	out.write(data.first.get(), data.second);
}

void read(std::istream& in, mmx::vm::varptr_t& value)
{
	vnx::Variant tmp;
	vnx::read(in, tmp);
	if(tmp.is_null()) {
		value = std::make_unique<mmx::vm::var_t>();
	} else if(tmp.is_bool()) {
		value = std::make_unique<mmx::vm::var_t>(tmp.to<bool>() ? mmx::vm::TYPE_TRUE : mmx::vm::TYPE_FALSE);
	} else if(tmp.is_ulong()) {
		value = std::make_unique<mmx::vm::uint_t>(tmp.to<uint64_t>());
	} else if(tmp.is_long() || tmp.is_double()) {
		value = std::make_unique<mmx::vm::uint_t>(tmp.to<int64_t>());
	} else if(tmp.is_string()) {
		value = mmx::vm::binary_t::alloc(tmp.to<std::string>());
	} else {
		value = nullptr;
	}
}

void write(std::ostream& out, const mmx::vm::varptr_t& value)
{
	vnx::DefaultPrinter visitor(out);
	vnx::accept(visitor, value);
}

void accept(vnx::Visitor& visitor, const mmx::vm::varptr_t& value)
{
	const auto var = value.get();
	switch(var ? var->type : mmx::vm::TYPE_NIL) {
		case mmx::vm::TYPE_NIL:
			visitor.visit_null();
			break;
		case mmx::vm::TYPE_TRUE:
			visitor.visit(true);
			break;
		case mmx::vm::TYPE_FALSE:
			visitor.visit(false);
			break;
		case mmx::vm::TYPE_UINT: {
			const auto& value = ((const mmx::vm::uint_t*)var)->value;
			if(value >> 64) {
				visitor.visit("0x" + value.str(16));
			} else {
				visitor.visit(value.lower().lower());
			}
			break;
		}
		case mmx::vm::TYPE_STRING: {
			visitor.visit(((const mmx::vm::binary_t*)var)->to_string());
			break;
		}
		case mmx::vm::TYPE_BINARY: {
			const auto* bin = ((const mmx::vm::binary_t*)var);
			visitor.visit(bin->data(), bin->size);
			break;
		}
		default:
			visitor.visit("vartype_e(" + std::to_string(int(var->type)) + ")");
	}
}


} // vnx
