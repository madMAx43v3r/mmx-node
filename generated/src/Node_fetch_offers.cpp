
// AUTO GENERATED by vnxcppcodegen

#include <mmx/package.hxx>
#include <mmx/Node_fetch_offers.hxx>
#include <mmx/Node_fetch_offers_return.hxx>
#include <mmx/addr_t.hpp>
#include <vnx/Value.h>

#include <vnx/vnx.h>


namespace mmx {


const vnx::Hash64 Node_fetch_offers::VNX_TYPE_HASH(0xfca08ee41b997129ull);
const vnx::Hash64 Node_fetch_offers::VNX_CODE_HASH(0xb399f3337f3d628cull);

vnx::Hash64 Node_fetch_offers::get_type_hash() const {
	return VNX_TYPE_HASH;
}

std::string Node_fetch_offers::get_type_name() const {
	return "mmx.Node.fetch_offers";
}

const vnx::TypeCode* Node_fetch_offers::get_type_code() const {
	return mmx::vnx_native_type_code_Node_fetch_offers;
}

std::shared_ptr<Node_fetch_offers> Node_fetch_offers::create() {
	return std::make_shared<Node_fetch_offers>();
}

std::shared_ptr<vnx::Value> Node_fetch_offers::clone() const {
	return std::make_shared<Node_fetch_offers>(*this);
}

void Node_fetch_offers::read(vnx::TypeInput& _in, const vnx::TypeCode* _type_code, const uint16_t* _code) {
	vnx::read(_in, *this, _type_code, _code);
}

void Node_fetch_offers::write(vnx::TypeOutput& _out, const vnx::TypeCode* _type_code, const uint16_t* _code) const {
	vnx::write(_out, *this, _type_code, _code);
}

void Node_fetch_offers::accept(vnx::Visitor& _visitor) const {
	const vnx::TypeCode* _type_code = mmx::vnx_native_type_code_Node_fetch_offers;
	_visitor.type_begin(*_type_code);
	_visitor.type_field(_type_code->fields[0], 0); vnx::accept(_visitor, addresses);
	_visitor.type_field(_type_code->fields[1], 1); vnx::accept(_visitor, state);
	_visitor.type_field(_type_code->fields[2], 2); vnx::accept(_visitor, closed);
	_visitor.type_end(*_type_code);
}

void Node_fetch_offers::write(std::ostream& _out) const {
	_out << "{\"__type\": \"mmx.Node.fetch_offers\"";
	_out << ", \"addresses\": "; vnx::write(_out, addresses);
	_out << ", \"state\": "; vnx::write(_out, state);
	_out << ", \"closed\": "; vnx::write(_out, closed);
	_out << "}";
}

void Node_fetch_offers::read(std::istream& _in) {
	if(auto _json = vnx::read_json(_in)) {
		from_object(_json->to_object());
	}
}

vnx::Object Node_fetch_offers::to_object() const {
	vnx::Object _object;
	_object["__type"] = "mmx.Node.fetch_offers";
	_object["addresses"] = addresses;
	_object["state"] = state;
	_object["closed"] = closed;
	return _object;
}

void Node_fetch_offers::from_object(const vnx::Object& _object) {
	for(const auto& _entry : _object.field) {
		if(_entry.first == "addresses") {
			_entry.second.to(addresses);
		} else if(_entry.first == "closed") {
			_entry.second.to(closed);
		} else if(_entry.first == "state") {
			_entry.second.to(state);
		}
	}
}

vnx::Variant Node_fetch_offers::get_field(const std::string& _name) const {
	if(_name == "addresses") {
		return vnx::Variant(addresses);
	}
	if(_name == "state") {
		return vnx::Variant(state);
	}
	if(_name == "closed") {
		return vnx::Variant(closed);
	}
	return vnx::Variant();
}

void Node_fetch_offers::set_field(const std::string& _name, const vnx::Variant& _value) {
	if(_name == "addresses") {
		_value.to(addresses);
	} else if(_name == "state") {
		_value.to(state);
	} else if(_name == "closed") {
		_value.to(closed);
	}
}

/// \private
std::ostream& operator<<(std::ostream& _out, const Node_fetch_offers& _value) {
	_value.write(_out);
	return _out;
}

/// \private
std::istream& operator>>(std::istream& _in, Node_fetch_offers& _value) {
	_value.read(_in);
	return _in;
}

const vnx::TypeCode* Node_fetch_offers::static_get_type_code() {
	const vnx::TypeCode* type_code = vnx::get_type_code(VNX_TYPE_HASH);
	if(!type_code) {
		type_code = vnx::register_type_code(static_create_type_code());
	}
	return type_code;
}

std::shared_ptr<vnx::TypeCode> Node_fetch_offers::static_create_type_code() {
	auto type_code = std::make_shared<vnx::TypeCode>();
	type_code->name = "mmx.Node.fetch_offers";
	type_code->type_hash = vnx::Hash64(0xfca08ee41b997129ull);
	type_code->code_hash = vnx::Hash64(0xb399f3337f3d628cull);
	type_code->is_native = true;
	type_code->is_class = true;
	type_code->is_method = true;
	type_code->native_size = sizeof(::mmx::Node_fetch_offers);
	type_code->create_value = []() -> std::shared_ptr<vnx::Value> { return std::make_shared<Node_fetch_offers>(); };
	type_code->is_const = true;
	type_code->return_type = ::mmx::Node_fetch_offers_return::static_get_type_code();
	type_code->fields.resize(3);
	{
		auto& field = type_code->fields[0];
		field.is_extended = true;
		field.name = "addresses";
		field.code = {12, 11, 32, 1};
	}
	{
		auto& field = type_code->fields[1];
		field.data_size = 1;
		field.name = "state";
		field.code = {31};
	}
	{
		auto& field = type_code->fields[2];
		field.data_size = 1;
		field.name = "closed";
		field.code = {31};
	}
	type_code->permission = "mmx.permission_e.PUBLIC";
	type_code->build();
	return type_code;
}


} // namespace mmx


namespace vnx {

void read(TypeInput& in, ::mmx::Node_fetch_offers& value, const TypeCode* type_code, const uint16_t* code) {
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
			vnx::read_value(_buf + _field->offset, value.state, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[2]) {
			vnx::read_value(_buf + _field->offset, value.closed, _field->code.data());
		}
	}
	for(const auto* _field : type_code->ext_fields) {
		switch(_field->native_index) {
			case 0: vnx::read(in, value.addresses, type_code, _field->code.data()); break;
			default: vnx::skip(in, type_code, _field->code.data());
		}
	}
}

void write(TypeOutput& out, const ::mmx::Node_fetch_offers& value, const TypeCode* type_code, const uint16_t* code) {
	if(code && code[0] == CODE_OBJECT) {
		vnx::write(out, value.to_object(), nullptr, code);
		return;
	}
	if(!type_code || (code && code[0] == CODE_ANY)) {
		type_code = mmx::vnx_native_type_code_Node_fetch_offers;
		out.write_type_code(type_code);
		vnx::write_class_header<::mmx::Node_fetch_offers>(out);
	}
	else if(code && code[0] == CODE_STRUCT) {
		type_code = type_code->depends[code[1]];
	}
	auto* const _buf = out.write(2);
	vnx::write_value(_buf + 0, value.state);
	vnx::write_value(_buf + 1, value.closed);
	vnx::write(out, value.addresses, type_code, type_code->fields[0].code.data());
}

void read(std::istream& in, ::mmx::Node_fetch_offers& value) {
	value.read(in);
}

void write(std::ostream& out, const ::mmx::Node_fetch_offers& value) {
	value.write(out);
}

void accept(Visitor& visitor, const ::mmx::Node_fetch_offers& value) {
	value.accept(visitor);
}

} // vnx
