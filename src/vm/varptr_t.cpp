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
			mmx::vm::var_t* var = nullptr;
			deserialize(var, data.data(), data.size(), false, false);
			value = var;
			break;
		}
		case CODE_STRING:
		case CODE_ALT_STRING: {
			std::string tmp;
			vnx::read(in, tmp, type_code, code);
			value = mmx::vm::binary_t::alloc(tmp);
			break;
		}
		case CODE_DYNAMIC:
		case CODE_ALT_DYNAMIC: {
			vnx::Variant tmp;
			vnx::read(in, tmp, type_code, code);
			mmx::vm::var_t* var = nullptr;
			const auto data = tmp.to<std::vector<uint8_t>>();
			deserialize(var, data.data(), data.size(), false, false);
			value = var;
			break;
		}
		default:
			vnx::skip(in, type_code, code);
	}
}

void write(vnx::TypeOutput& out, const mmx::vm::varptr_t& value, const vnx::TypeCode* type_code, const uint16_t* code)
{
	if(auto var = value.get()) {
		const auto data = mmx::vm::serialize(*var, false, false);
		write(out, (uint32_t)data.second);
		out.write(data.first, data.second);
		::free(data.first);
	} else {
		write(out, (uint32_t)0);
	}
}

void read(std::istream& in, mmx::vm::varptr_t& value)
{
	vnx::Variant tmp;
	vnx::read(in, tmp);
	if(tmp.is_null()) {
		value = new mmx::vm::var_t();
	} else if(tmp.is_bool()) {
		value = new mmx::vm::var_t(tmp.to<bool>() ? mmx::vm::TYPE_TRUE : mmx::vm::TYPE_FALSE);
	} else if(tmp.is_ulong()) {
		value = new mmx::vm::uint_t(tmp.to<uint64_t>());
	} else if(tmp.is_long() || tmp.is_double()) {
		value = new mmx::vm::uint_t(tmp.to<int64_t>());
	} else if(tmp.is_string()) {
		value = mmx::vm::binary_t::alloc(tmp.to<std::string>());
	} else {
		value = nullptr;
	}
}

void write(std::ostream& out, const mmx::vm::varptr_t& value)
{
	vnx::write(out, mmx::vm::to_string(value.get()));
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
		default:
			visitor.visit(mmx::vm::to_string(var));
	}
}


} // vnx
