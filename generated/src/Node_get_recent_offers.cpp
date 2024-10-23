
// AUTO GENERATED by vnxcppcodegen

#include <mmx/package.hxx>
#include <mmx/Node_get_recent_offers.hxx>
#include <mmx/Node_get_recent_offers_return.hxx>
#include <vnx/Value.h>

#include <vnx/vnx.h>


namespace mmx {


const vnx::Hash64 Node_get_recent_offers::VNX_TYPE_HASH(0xfcd729efd0504fdeull);
const vnx::Hash64 Node_get_recent_offers::VNX_CODE_HASH(0x349317a0a5d82726ull);

vnx::Hash64 Node_get_recent_offers::get_type_hash() const {
	return VNX_TYPE_HASH;
}

std::string Node_get_recent_offers::get_type_name() const {
	return "mmx.Node.get_recent_offers";
}

const vnx::TypeCode* Node_get_recent_offers::get_type_code() const {
	return mmx::vnx_native_type_code_Node_get_recent_offers;
}

std::shared_ptr<Node_get_recent_offers> Node_get_recent_offers::create() {
	return std::make_shared<Node_get_recent_offers>();
}

std::shared_ptr<vnx::Value> Node_get_recent_offers::clone() const {
	return std::make_shared<Node_get_recent_offers>(*this);
}

void Node_get_recent_offers::read(vnx::TypeInput& _in, const vnx::TypeCode* _type_code, const uint16_t* _code) {
	vnx::read(_in, *this, _type_code, _code);
}

void Node_get_recent_offers::write(vnx::TypeOutput& _out, const vnx::TypeCode* _type_code, const uint16_t* _code) const {
	vnx::write(_out, *this, _type_code, _code);
}

void Node_get_recent_offers::accept(vnx::Visitor& _visitor) const {
	const vnx::TypeCode* _type_code = mmx::vnx_native_type_code_Node_get_recent_offers;
	_visitor.type_begin(*_type_code);
	_visitor.type_field(_type_code->fields[0], 0); vnx::accept(_visitor, limit);
	_visitor.type_field(_type_code->fields[1], 1); vnx::accept(_visitor, state);
	_visitor.type_end(*_type_code);
}

void Node_get_recent_offers::write(std::ostream& _out) const {
	_out << "{\"__type\": \"mmx.Node.get_recent_offers\"";
	_out << ", \"limit\": "; vnx::write(_out, limit);
	_out << ", \"state\": "; vnx::write(_out, state);
	_out << "}";
}

void Node_get_recent_offers::read(std::istream& _in) {
	if(auto _json = vnx::read_json(_in)) {
		from_object(_json->to_object());
	}
}

vnx::Object Node_get_recent_offers::to_object() const {
	vnx::Object _object;
	_object["__type"] = "mmx.Node.get_recent_offers";
	_object["limit"] = limit;
	_object["state"] = state;
	return _object;
}

void Node_get_recent_offers::from_object(const vnx::Object& _object) {
	for(const auto& _entry : _object.field) {
		if(_entry.first == "limit") {
			_entry.second.to(limit);
		} else if(_entry.first == "state") {
			_entry.second.to(state);
		}
	}
}

vnx::Variant Node_get_recent_offers::get_field(const std::string& _name) const {
	if(_name == "limit") {
		return vnx::Variant(limit);
	}
	if(_name == "state") {
		return vnx::Variant(state);
	}
	return vnx::Variant();
}

void Node_get_recent_offers::set_field(const std::string& _name, const vnx::Variant& _value) {
	if(_name == "limit") {
		_value.to(limit);
	} else if(_name == "state") {
		_value.to(state);
	}
}

/// \private
std::ostream& operator<<(std::ostream& _out, const Node_get_recent_offers& _value) {
	_value.write(_out);
	return _out;
}

/// \private
std::istream& operator>>(std::istream& _in, Node_get_recent_offers& _value) {
	_value.read(_in);
	return _in;
}

const vnx::TypeCode* Node_get_recent_offers::static_get_type_code() {
	const vnx::TypeCode* type_code = vnx::get_type_code(VNX_TYPE_HASH);
	if(!type_code) {
		type_code = vnx::register_type_code(static_create_type_code());
	}
	return type_code;
}

std::shared_ptr<vnx::TypeCode> Node_get_recent_offers::static_create_type_code() {
	auto type_code = std::make_shared<vnx::TypeCode>();
	type_code->name = "mmx.Node.get_recent_offers";
	type_code->type_hash = vnx::Hash64(0xfcd729efd0504fdeull);
	type_code->code_hash = vnx::Hash64(0x349317a0a5d82726ull);
	type_code->is_native = true;
	type_code->is_class = true;
	type_code->is_method = true;
	type_code->native_size = sizeof(::mmx::Node_get_recent_offers);
	type_code->create_value = []() -> std::shared_ptr<vnx::Value> { return std::make_shared<Node_get_recent_offers>(); };
	type_code->is_const = true;
	type_code->return_type = ::mmx::Node_get_recent_offers_return::static_get_type_code();
	type_code->fields.resize(2);
	{
		auto& field = type_code->fields[0];
		field.data_size = 4;
		field.name = "limit";
		field.value = vnx::to_string(100);
		field.code = {7};
	}
	{
		auto& field = type_code->fields[1];
		field.data_size = 1;
		field.name = "state";
		field.value = vnx::to_string(true);
		field.code = {31};
	}
	type_code->permission = "mmx.permission_e.PUBLIC";
	type_code->build();
	return type_code;
}


} // namespace mmx


namespace vnx {

void read(TypeInput& in, ::mmx::Node_get_recent_offers& value, const TypeCode* type_code, const uint16_t* code) {
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
			vnx::read_value(_buf + _field->offset, value.limit, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[1]) {
			vnx::read_value(_buf + _field->offset, value.state, _field->code.data());
		}
	}
	for(const auto* _field : type_code->ext_fields) {
		switch(_field->native_index) {
			default: vnx::skip(in, type_code, _field->code.data());
		}
	}
}

void write(TypeOutput& out, const ::mmx::Node_get_recent_offers& value, const TypeCode* type_code, const uint16_t* code) {
	if(code && code[0] == CODE_OBJECT) {
		vnx::write(out, value.to_object(), nullptr, code);
		return;
	}
	if(!type_code || (code && code[0] == CODE_ANY)) {
		type_code = mmx::vnx_native_type_code_Node_get_recent_offers;
		out.write_type_code(type_code);
		vnx::write_class_header<::mmx::Node_get_recent_offers>(out);
	}
	else if(code && code[0] == CODE_STRUCT) {
		type_code = type_code->depends[code[1]];
	}
	auto* const _buf = out.write(5);
	vnx::write_value(_buf + 0, value.limit);
	vnx::write_value(_buf + 4, value.state);
}

void read(std::istream& in, ::mmx::Node_get_recent_offers& value) {
	value.read(in);
}

void write(std::ostream& out, const ::mmx::Node_get_recent_offers& value) {
	value.write(out);
}

void accept(Visitor& visitor, const ::mmx::Node_get_recent_offers& value) {
	value.accept(visitor);
}

} // vnx
