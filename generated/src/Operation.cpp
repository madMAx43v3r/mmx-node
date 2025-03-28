
// AUTO GENERATED by vnxcppcodegen

#include <mmx/package.hxx>
#include <mmx/Operation.hxx>
#include <mmx/ChainParams.hxx>
#include <mmx/addr_t.hpp>
#include <mmx/hash_t.hpp>
#include <vnx/Value.h>

#include <vnx/vnx.h>


namespace mmx {

const uint16_t Operation::NO_SOLUTION;

const vnx::Hash64 Operation::VNX_TYPE_HASH(0xfd69dd82e906e619ull);
const vnx::Hash64 Operation::VNX_CODE_HASH(0xeb820061b22c3c4bull);

vnx::Hash64 Operation::get_type_hash() const {
	return VNX_TYPE_HASH;
}

std::string Operation::get_type_name() const {
	return "mmx.Operation";
}

const vnx::TypeCode* Operation::get_type_code() const {
	return mmx::vnx_native_type_code_Operation;
}

std::shared_ptr<Operation> Operation::create() {
	return std::make_shared<Operation>();
}

std::shared_ptr<vnx::Value> Operation::clone() const {
	return std::make_shared<Operation>(*this);
}

void Operation::read(vnx::TypeInput& _in, const vnx::TypeCode* _type_code, const uint16_t* _code) {
	vnx::read(_in, *this, _type_code, _code);
}

void Operation::write(vnx::TypeOutput& _out, const vnx::TypeCode* _type_code, const uint16_t* _code) const {
	vnx::write(_out, *this, _type_code, _code);
}

void Operation::accept(vnx::Visitor& _visitor) const {
	const vnx::TypeCode* _type_code = mmx::vnx_native_type_code_Operation;
	_visitor.type_begin(*_type_code);
	_visitor.type_field(_type_code->fields[0], 0); vnx::accept(_visitor, version);
	_visitor.type_field(_type_code->fields[1], 1); vnx::accept(_visitor, address);
	_visitor.type_field(_type_code->fields[2], 2); vnx::accept(_visitor, solution);
	_visitor.type_end(*_type_code);
}

void Operation::write(std::ostream& _out) const {
	_out << "{\"__type\": \"mmx.Operation\"";
	_out << ", \"version\": "; vnx::write(_out, version);
	_out << ", \"address\": "; vnx::write(_out, address);
	_out << ", \"solution\": "; vnx::write(_out, solution);
	_out << "}";
}

void Operation::read(std::istream& _in) {
	if(auto _json = vnx::read_json(_in)) {
		from_object(_json->to_object());
	}
}

vnx::Object Operation::to_object() const {
	vnx::Object _object;
	_object["__type"] = "mmx.Operation";
	_object["version"] = version;
	_object["address"] = address;
	_object["solution"] = solution;
	return _object;
}

void Operation::from_object(const vnx::Object& _object) {
	for(const auto& _entry : _object.field) {
		if(_entry.first == "address") {
			_entry.second.to(address);
		} else if(_entry.first == "solution") {
			_entry.second.to(solution);
		} else if(_entry.first == "version") {
			_entry.second.to(version);
		}
	}
}

vnx::Variant Operation::get_field(const std::string& _name) const {
	if(_name == "version") {
		return vnx::Variant(version);
	}
	if(_name == "address") {
		return vnx::Variant(address);
	}
	if(_name == "solution") {
		return vnx::Variant(solution);
	}
	return vnx::Variant();
}

void Operation::set_field(const std::string& _name, const vnx::Variant& _value) {
	if(_name == "version") {
		_value.to(version);
	} else if(_name == "address") {
		_value.to(address);
	} else if(_name == "solution") {
		_value.to(solution);
	}
}

/// \private
std::ostream& operator<<(std::ostream& _out, const Operation& _value) {
	_value.write(_out);
	return _out;
}

/// \private
std::istream& operator>>(std::istream& _in, Operation& _value) {
	_value.read(_in);
	return _in;
}

const vnx::TypeCode* Operation::static_get_type_code() {
	const vnx::TypeCode* type_code = vnx::get_type_code(VNX_TYPE_HASH);
	if(!type_code) {
		type_code = vnx::register_type_code(static_create_type_code());
	}
	return type_code;
}

std::shared_ptr<vnx::TypeCode> Operation::static_create_type_code() {
	auto type_code = std::make_shared<vnx::TypeCode>();
	type_code->name = "mmx.Operation";
	type_code->type_hash = vnx::Hash64(0xfd69dd82e906e619ull);
	type_code->code_hash = vnx::Hash64(0xeb820061b22c3c4bull);
	type_code->is_native = true;
	type_code->is_class = true;
	type_code->native_size = sizeof(::mmx::Operation);
	type_code->create_value = []() -> std::shared_ptr<vnx::Value> { return std::make_shared<Operation>(); };
	type_code->fields.resize(3);
	{
		auto& field = type_code->fields[0];
		field.data_size = 4;
		field.name = "version";
		field.code = {3};
	}
	{
		auto& field = type_code->fields[1];
		field.is_extended = true;
		field.name = "address";
		field.code = {11, 32, 1};
	}
	{
		auto& field = type_code->fields[2];
		field.data_size = 2;
		field.name = "solution";
		field.value = vnx::to_string(-1);
		field.code = {2};
	}
	type_code->build();
	return type_code;
}

std::shared_ptr<vnx::Value> Operation::vnx_call_switch(std::shared_ptr<const vnx::Value> _method) {
	switch(_method->get_type_hash()) {
	}
	return nullptr;
}


} // namespace mmx


namespace vnx {

void read(TypeInput& in, ::mmx::Operation& value, const TypeCode* type_code, const uint16_t* code) {
	TypeInput::recursion_t tag(in);
	if(code) {
		switch(code[0]) {
			case CODE_OBJECT:
			case CODE_ALT_OBJECT: {
				Object tmp;
				vnx::read(in, tmp, type_code, code);
				value.from_object(tmp);
				return;
			}
			case CODE_DYNAMIC:
			case CODE_ALT_DYNAMIC:
				vnx::read_dynamic(in, value);
				return;
		}
	}
	if(!type_code) {
		vnx::skip(in, type_code, code);
		return;
	}
	if(code) {
		switch(code[0]) {
			case CODE_STRUCT: type_code = type_code->depends[code[1]]; break;
			case CODE_ALT_STRUCT: type_code = type_code->depends[vnx::flip_bytes(code[1])]; break;
			default: {
				vnx::skip(in, type_code, code);
				return;
			}
		}
	}
	const auto* const _buf = in.read(type_code->total_field_size);
	if(type_code->is_matched) {
		if(const auto* const _field = type_code->field_map[0]) {
			vnx::read_value(_buf + _field->offset, value.version, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[2]) {
			vnx::read_value(_buf + _field->offset, value.solution, _field->code.data());
		}
	}
	for(const auto* _field : type_code->ext_fields) {
		switch(_field->native_index) {
			case 1: vnx::read(in, value.address, type_code, _field->code.data()); break;
			default: vnx::skip(in, type_code, _field->code.data());
		}
	}
}

void write(TypeOutput& out, const ::mmx::Operation& value, const TypeCode* type_code, const uint16_t* code) {
	if(code && code[0] == CODE_OBJECT) {
		vnx::write(out, value.to_object(), nullptr, code);
		return;
	}
	if(!type_code || (code && code[0] == CODE_ANY)) {
		type_code = mmx::vnx_native_type_code_Operation;
		out.write_type_code(type_code);
		vnx::write_class_header<::mmx::Operation>(out);
	}
	else if(code && code[0] == CODE_STRUCT) {
		type_code = type_code->depends[code[1]];
	}
	auto* const _buf = out.write(6);
	vnx::write_value(_buf + 0, value.version);
	vnx::write_value(_buf + 4, value.solution);
	vnx::write(out, value.address, type_code, type_code->fields[1].code.data());
}

void read(std::istream& in, ::mmx::Operation& value) {
	value.read(in);
}

void write(std::ostream& out, const ::mmx::Operation& value) {
	value.write(out);
}

void accept(Visitor& visitor, const ::mmx::Operation& value) {
	value.accept(visitor);
}

} // vnx
