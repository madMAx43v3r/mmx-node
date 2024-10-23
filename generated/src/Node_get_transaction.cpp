
// AUTO GENERATED by vnxcppcodegen

#include <mmx/package.hxx>
#include <mmx/Node_get_transaction.hxx>
#include <mmx/Node_get_transaction_return.hxx>
#include <mmx/hash_t.hpp>
#include <vnx/Value.h>

#include <vnx/vnx.h>


namespace mmx {


const vnx::Hash64 Node_get_transaction::VNX_TYPE_HASH(0x9c76ca142292750full);
const vnx::Hash64 Node_get_transaction::VNX_CODE_HASH(0x69a50e4f02b559dfull);

vnx::Hash64 Node_get_transaction::get_type_hash() const {
	return VNX_TYPE_HASH;
}

std::string Node_get_transaction::get_type_name() const {
	return "mmx.Node.get_transaction";
}

const vnx::TypeCode* Node_get_transaction::get_type_code() const {
	return mmx::vnx_native_type_code_Node_get_transaction;
}

std::shared_ptr<Node_get_transaction> Node_get_transaction::create() {
	return std::make_shared<Node_get_transaction>();
}

std::shared_ptr<vnx::Value> Node_get_transaction::clone() const {
	return std::make_shared<Node_get_transaction>(*this);
}

void Node_get_transaction::read(vnx::TypeInput& _in, const vnx::TypeCode* _type_code, const uint16_t* _code) {
	vnx::read(_in, *this, _type_code, _code);
}

void Node_get_transaction::write(vnx::TypeOutput& _out, const vnx::TypeCode* _type_code, const uint16_t* _code) const {
	vnx::write(_out, *this, _type_code, _code);
}

void Node_get_transaction::accept(vnx::Visitor& _visitor) const {
	const vnx::TypeCode* _type_code = mmx::vnx_native_type_code_Node_get_transaction;
	_visitor.type_begin(*_type_code);
	_visitor.type_field(_type_code->fields[0], 0); vnx::accept(_visitor, id);
	_visitor.type_field(_type_code->fields[1], 1); vnx::accept(_visitor, include_pending);
	_visitor.type_end(*_type_code);
}

void Node_get_transaction::write(std::ostream& _out) const {
	_out << "{\"__type\": \"mmx.Node.get_transaction\"";
	_out << ", \"id\": "; vnx::write(_out, id);
	_out << ", \"include_pending\": "; vnx::write(_out, include_pending);
	_out << "}";
}

void Node_get_transaction::read(std::istream& _in) {
	if(auto _json = vnx::read_json(_in)) {
		from_object(_json->to_object());
	}
}

vnx::Object Node_get_transaction::to_object() const {
	vnx::Object _object;
	_object["__type"] = "mmx.Node.get_transaction";
	_object["id"] = id;
	_object["include_pending"] = include_pending;
	return _object;
}

void Node_get_transaction::from_object(const vnx::Object& _object) {
	for(const auto& _entry : _object.field) {
		if(_entry.first == "id") {
			_entry.second.to(id);
		} else if(_entry.first == "include_pending") {
			_entry.second.to(include_pending);
		}
	}
}

vnx::Variant Node_get_transaction::get_field(const std::string& _name) const {
	if(_name == "id") {
		return vnx::Variant(id);
	}
	if(_name == "include_pending") {
		return vnx::Variant(include_pending);
	}
	return vnx::Variant();
}

void Node_get_transaction::set_field(const std::string& _name, const vnx::Variant& _value) {
	if(_name == "id") {
		_value.to(id);
	} else if(_name == "include_pending") {
		_value.to(include_pending);
	}
}

/// \private
std::ostream& operator<<(std::ostream& _out, const Node_get_transaction& _value) {
	_value.write(_out);
	return _out;
}

/// \private
std::istream& operator>>(std::istream& _in, Node_get_transaction& _value) {
	_value.read(_in);
	return _in;
}

const vnx::TypeCode* Node_get_transaction::static_get_type_code() {
	const vnx::TypeCode* type_code = vnx::get_type_code(VNX_TYPE_HASH);
	if(!type_code) {
		type_code = vnx::register_type_code(static_create_type_code());
	}
	return type_code;
}

std::shared_ptr<vnx::TypeCode> Node_get_transaction::static_create_type_code() {
	auto type_code = std::make_shared<vnx::TypeCode>();
	type_code->name = "mmx.Node.get_transaction";
	type_code->type_hash = vnx::Hash64(0x9c76ca142292750full);
	type_code->code_hash = vnx::Hash64(0x69a50e4f02b559dfull);
	type_code->is_native = true;
	type_code->is_class = true;
	type_code->is_method = true;
	type_code->native_size = sizeof(::mmx::Node_get_transaction);
	type_code->create_value = []() -> std::shared_ptr<vnx::Value> { return std::make_shared<Node_get_transaction>(); };
	type_code->is_const = true;
	type_code->return_type = ::mmx::Node_get_transaction_return::static_get_type_code();
	type_code->fields.resize(2);
	{
		auto& field = type_code->fields[0];
		field.is_extended = true;
		field.name = "id";
		field.code = {11, 32, 1};
	}
	{
		auto& field = type_code->fields[1];
		field.data_size = 1;
		field.name = "include_pending";
		field.code = {31};
	}
	type_code->permission = "mmx.permission_e.PUBLIC";
	type_code->build();
	return type_code;
}


} // namespace mmx


namespace vnx {

void read(TypeInput& in, ::mmx::Node_get_transaction& value, const TypeCode* type_code, const uint16_t* code) {
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
		if(const auto* const _field = type_code->field_map[1]) {
			vnx::read_value(_buf + _field->offset, value.include_pending, _field->code.data());
		}
	}
	for(const auto* _field : type_code->ext_fields) {
		switch(_field->native_index) {
			case 0: vnx::read(in, value.id, type_code, _field->code.data()); break;
			default: vnx::skip(in, type_code, _field->code.data());
		}
	}
}

void write(TypeOutput& out, const ::mmx::Node_get_transaction& value, const TypeCode* type_code, const uint16_t* code) {
	if(code && code[0] == CODE_OBJECT) {
		vnx::write(out, value.to_object(), nullptr, code);
		return;
	}
	if(!type_code || (code && code[0] == CODE_ANY)) {
		type_code = mmx::vnx_native_type_code_Node_get_transaction;
		out.write_type_code(type_code);
		vnx::write_class_header<::mmx::Node_get_transaction>(out);
	}
	else if(code && code[0] == CODE_STRUCT) {
		type_code = type_code->depends[code[1]];
	}
	auto* const _buf = out.write(1);
	vnx::write_value(_buf + 0, value.include_pending);
	vnx::write(out, value.id, type_code, type_code->fields[0].code.data());
}

void read(std::istream& in, ::mmx::Node_get_transaction& value) {
	value.read(in);
}

void write(std::ostream& out, const ::mmx::Node_get_transaction& value) {
	value.write(out);
}

void accept(Visitor& visitor, const ::mmx::Node_get_transaction& value) {
	value.accept(visitor);
}

} // vnx
