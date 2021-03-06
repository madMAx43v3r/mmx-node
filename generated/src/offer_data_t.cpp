
// AUTO GENERATED by vnxcppcodegen

#include <mmx/package.hxx>
#include <mmx/offer_data_t.hxx>
#include <mmx/Transaction.hxx>
#include <mmx/addr_t.hpp>

#include <vnx/vnx.h>


namespace mmx {


const vnx::Hash64 offer_data_t::VNX_TYPE_HASH(0xc97a08a709a5f1efull);
const vnx::Hash64 offer_data_t::VNX_CODE_HASH(0x9496e81bf8f5bcfaull);

vnx::Hash64 offer_data_t::get_type_hash() const {
	return VNX_TYPE_HASH;
}

std::string offer_data_t::get_type_name() const {
	return "mmx.offer_data_t";
}

const vnx::TypeCode* offer_data_t::get_type_code() const {
	return mmx::vnx_native_type_code_offer_data_t;
}

std::shared_ptr<offer_data_t> offer_data_t::create() {
	return std::make_shared<offer_data_t>();
}

std::shared_ptr<offer_data_t> offer_data_t::clone() const {
	return std::make_shared<offer_data_t>(*this);
}

void offer_data_t::read(vnx::TypeInput& _in, const vnx::TypeCode* _type_code, const uint16_t* _code) {
	vnx::read(_in, *this, _type_code, _code);
}

void offer_data_t::write(vnx::TypeOutput& _out, const vnx::TypeCode* _type_code, const uint16_t* _code) const {
	vnx::write(_out, *this, _type_code, _code);
}

void offer_data_t::accept(vnx::Visitor& _visitor) const {
	const vnx::TypeCode* _type_code = mmx::vnx_native_type_code_offer_data_t;
	_visitor.type_begin(*_type_code);
	_visitor.type_field(_type_code->fields[0], 0); vnx::accept(_visitor, height);
	_visitor.type_field(_type_code->fields[1], 1); vnx::accept(_visitor, address);
	_visitor.type_field(_type_code->fields[2], 2); vnx::accept(_visitor, offer);
	_visitor.type_field(_type_code->fields[3], 3); vnx::accept(_visitor, is_open);
	_visitor.type_field(_type_code->fields[4], 4); vnx::accept(_visitor, is_revoked);
	_visitor.type_field(_type_code->fields[5], 5); vnx::accept(_visitor, is_covered);
	_visitor.type_end(*_type_code);
}

void offer_data_t::write(std::ostream& _out) const {
	_out << "{";
	_out << "\"height\": "; vnx::write(_out, height);
	_out << ", \"address\": "; vnx::write(_out, address);
	_out << ", \"offer\": "; vnx::write(_out, offer);
	_out << ", \"is_open\": "; vnx::write(_out, is_open);
	_out << ", \"is_revoked\": "; vnx::write(_out, is_revoked);
	_out << ", \"is_covered\": "; vnx::write(_out, is_covered);
	_out << "}";
}

void offer_data_t::read(std::istream& _in) {
	if(auto _json = vnx::read_json(_in)) {
		from_object(_json->to_object());
	}
}

vnx::Object offer_data_t::to_object() const {
	vnx::Object _object;
	_object["__type"] = "mmx.offer_data_t";
	_object["height"] = height;
	_object["address"] = address;
	_object["offer"] = offer;
	_object["is_open"] = is_open;
	_object["is_revoked"] = is_revoked;
	_object["is_covered"] = is_covered;
	return _object;
}

void offer_data_t::from_object(const vnx::Object& _object) {
	for(const auto& _entry : _object.field) {
		if(_entry.first == "address") {
			_entry.second.to(address);
		} else if(_entry.first == "height") {
			_entry.second.to(height);
		} else if(_entry.first == "is_covered") {
			_entry.second.to(is_covered);
		} else if(_entry.first == "is_open") {
			_entry.second.to(is_open);
		} else if(_entry.first == "is_revoked") {
			_entry.second.to(is_revoked);
		} else if(_entry.first == "offer") {
			_entry.second.to(offer);
		}
	}
}

vnx::Variant offer_data_t::get_field(const std::string& _name) const {
	if(_name == "height") {
		return vnx::Variant(height);
	}
	if(_name == "address") {
		return vnx::Variant(address);
	}
	if(_name == "offer") {
		return vnx::Variant(offer);
	}
	if(_name == "is_open") {
		return vnx::Variant(is_open);
	}
	if(_name == "is_revoked") {
		return vnx::Variant(is_revoked);
	}
	if(_name == "is_covered") {
		return vnx::Variant(is_covered);
	}
	return vnx::Variant();
}

void offer_data_t::set_field(const std::string& _name, const vnx::Variant& _value) {
	if(_name == "height") {
		_value.to(height);
	} else if(_name == "address") {
		_value.to(address);
	} else if(_name == "offer") {
		_value.to(offer);
	} else if(_name == "is_open") {
		_value.to(is_open);
	} else if(_name == "is_revoked") {
		_value.to(is_revoked);
	} else if(_name == "is_covered") {
		_value.to(is_covered);
	}
}

/// \private
std::ostream& operator<<(std::ostream& _out, const offer_data_t& _value) {
	_value.write(_out);
	return _out;
}

/// \private
std::istream& operator>>(std::istream& _in, offer_data_t& _value) {
	_value.read(_in);
	return _in;
}

const vnx::TypeCode* offer_data_t::static_get_type_code() {
	const vnx::TypeCode* type_code = vnx::get_type_code(VNX_TYPE_HASH);
	if(!type_code) {
		type_code = vnx::register_type_code(static_create_type_code());
	}
	return type_code;
}

std::shared_ptr<vnx::TypeCode> offer_data_t::static_create_type_code() {
	auto type_code = std::make_shared<vnx::TypeCode>();
	type_code->name = "mmx.offer_data_t";
	type_code->type_hash = vnx::Hash64(0xc97a08a709a5f1efull);
	type_code->code_hash = vnx::Hash64(0x9496e81bf8f5bcfaull);
	type_code->is_native = true;
	type_code->native_size = sizeof(::mmx::offer_data_t);
	type_code->create_value = []() -> std::shared_ptr<vnx::Value> { return std::make_shared<vnx::Struct<offer_data_t>>(); };
	type_code->fields.resize(6);
	{
		auto& field = type_code->fields[0];
		field.data_size = 4;
		field.name = "height";
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
		field.is_extended = true;
		field.name = "offer";
		field.code = {16};
	}
	{
		auto& field = type_code->fields[3];
		field.data_size = 1;
		field.name = "is_open";
		field.code = {31};
	}
	{
		auto& field = type_code->fields[4];
		field.data_size = 1;
		field.name = "is_revoked";
		field.code = {31};
	}
	{
		auto& field = type_code->fields[5];
		field.data_size = 1;
		field.name = "is_covered";
		field.code = {31};
	}
	type_code->build();
	return type_code;
}


} // namespace mmx


namespace vnx {

void read(TypeInput& in, ::mmx::offer_data_t& value, const TypeCode* type_code, const uint16_t* code) {
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
	const char* const _buf = in.read(type_code->total_field_size);
	if(type_code->is_matched) {
		if(const auto* const _field = type_code->field_map[0]) {
			vnx::read_value(_buf + _field->offset, value.height, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[3]) {
			vnx::read_value(_buf + _field->offset, value.is_open, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[4]) {
			vnx::read_value(_buf + _field->offset, value.is_revoked, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[5]) {
			vnx::read_value(_buf + _field->offset, value.is_covered, _field->code.data());
		}
	}
	for(const auto* _field : type_code->ext_fields) {
		switch(_field->native_index) {
			case 1: vnx::read(in, value.address, type_code, _field->code.data()); break;
			case 2: vnx::read(in, value.offer, type_code, _field->code.data()); break;
			default: vnx::skip(in, type_code, _field->code.data());
		}
	}
}

void write(TypeOutput& out, const ::mmx::offer_data_t& value, const TypeCode* type_code, const uint16_t* code) {
	if(code && code[0] == CODE_OBJECT) {
		vnx::write(out, value.to_object(), nullptr, code);
		return;
	}
	if(!type_code || (code && code[0] == CODE_ANY)) {
		type_code = mmx::vnx_native_type_code_offer_data_t;
		out.write_type_code(type_code);
		vnx::write_class_header<::mmx::offer_data_t>(out);
	}
	else if(code && code[0] == CODE_STRUCT) {
		type_code = type_code->depends[code[1]];
	}
	char* const _buf = out.write(7);
	vnx::write_value(_buf + 0, value.height);
	vnx::write_value(_buf + 4, value.is_open);
	vnx::write_value(_buf + 5, value.is_revoked);
	vnx::write_value(_buf + 6, value.is_covered);
	vnx::write(out, value.address, type_code, type_code->fields[1].code.data());
	vnx::write(out, value.offer, type_code, type_code->fields[2].code.data());
}

void read(std::istream& in, ::mmx::offer_data_t& value) {
	value.read(in);
}

void write(std::ostream& out, const ::mmx::offer_data_t& value) {
	value.write(out);
}

void accept(Visitor& visitor, const ::mmx::offer_data_t& value) {
	value.accept(visitor);
}

bool is_equivalent<::mmx::offer_data_t>::operator()(const uint16_t* code, const TypeCode* type_code) {
	if(code[0] != CODE_STRUCT || !type_code) {
		return false;
	}
	type_code = type_code->depends[code[1]];
	return type_code->type_hash == ::mmx::offer_data_t::VNX_TYPE_HASH && type_code->is_equivalent;
}

} // vnx
