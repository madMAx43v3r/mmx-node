
// AUTO GENERATED by vnxcppcodegen

#include <mmx/package.hxx>
#include <mmx/Wallet_get_total_balances_for.hxx>
#include <mmx/Wallet_get_total_balances_for_return.hxx>
#include <mmx/addr_t.hpp>
#include <vnx/Value.h>

#include <vnx/vnx.h>


namespace mmx {


const vnx::Hash64 Wallet_get_total_balances_for::VNX_TYPE_HASH(0x784998cd7be76f95ull);
const vnx::Hash64 Wallet_get_total_balances_for::VNX_CODE_HASH(0xc75701c3b14a0a86ull);

vnx::Hash64 Wallet_get_total_balances_for::get_type_hash() const {
	return VNX_TYPE_HASH;
}

std::string Wallet_get_total_balances_for::get_type_name() const {
	return "mmx.Wallet.get_total_balances_for";
}

const vnx::TypeCode* Wallet_get_total_balances_for::get_type_code() const {
	return mmx::vnx_native_type_code_Wallet_get_total_balances_for;
}

std::shared_ptr<Wallet_get_total_balances_for> Wallet_get_total_balances_for::create() {
	return std::make_shared<Wallet_get_total_balances_for>();
}

std::shared_ptr<vnx::Value> Wallet_get_total_balances_for::clone() const {
	return std::make_shared<Wallet_get_total_balances_for>(*this);
}

void Wallet_get_total_balances_for::read(vnx::TypeInput& _in, const vnx::TypeCode* _type_code, const uint16_t* _code) {
	vnx::read(_in, *this, _type_code, _code);
}

void Wallet_get_total_balances_for::write(vnx::TypeOutput& _out, const vnx::TypeCode* _type_code, const uint16_t* _code) const {
	vnx::write(_out, *this, _type_code, _code);
}

void Wallet_get_total_balances_for::accept(vnx::Visitor& _visitor) const {
	const vnx::TypeCode* _type_code = mmx::vnx_native_type_code_Wallet_get_total_balances_for;
	_visitor.type_begin(*_type_code);
	_visitor.type_field(_type_code->fields[0], 0); vnx::accept(_visitor, addresses);
	_visitor.type_field(_type_code->fields[1], 1); vnx::accept(_visitor, min_confirm);
	_visitor.type_end(*_type_code);
}

void Wallet_get_total_balances_for::write(std::ostream& _out) const {
	_out << "{\"__type\": \"mmx.Wallet.get_total_balances_for\"";
	_out << ", \"addresses\": "; vnx::write(_out, addresses);
	_out << ", \"min_confirm\": "; vnx::write(_out, min_confirm);
	_out << "}";
}

void Wallet_get_total_balances_for::read(std::istream& _in) {
	if(auto _json = vnx::read_json(_in)) {
		from_object(_json->to_object());
	}
}

vnx::Object Wallet_get_total_balances_for::to_object() const {
	vnx::Object _object;
	_object["__type"] = "mmx.Wallet.get_total_balances_for";
	_object["addresses"] = addresses;
	_object["min_confirm"] = min_confirm;
	return _object;
}

void Wallet_get_total_balances_for::from_object(const vnx::Object& _object) {
	for(const auto& _entry : _object.field) {
		if(_entry.first == "addresses") {
			_entry.second.to(addresses);
		} else if(_entry.first == "min_confirm") {
			_entry.second.to(min_confirm);
		}
	}
}

vnx::Variant Wallet_get_total_balances_for::get_field(const std::string& _name) const {
	if(_name == "addresses") {
		return vnx::Variant(addresses);
	}
	if(_name == "min_confirm") {
		return vnx::Variant(min_confirm);
	}
	return vnx::Variant();
}

void Wallet_get_total_balances_for::set_field(const std::string& _name, const vnx::Variant& _value) {
	if(_name == "addresses") {
		_value.to(addresses);
	} else if(_name == "min_confirm") {
		_value.to(min_confirm);
	}
}

/// \private
std::ostream& operator<<(std::ostream& _out, const Wallet_get_total_balances_for& _value) {
	_value.write(_out);
	return _out;
}

/// \private
std::istream& operator>>(std::istream& _in, Wallet_get_total_balances_for& _value) {
	_value.read(_in);
	return _in;
}

const vnx::TypeCode* Wallet_get_total_balances_for::static_get_type_code() {
	const vnx::TypeCode* type_code = vnx::get_type_code(VNX_TYPE_HASH);
	if(!type_code) {
		type_code = vnx::register_type_code(static_create_type_code());
	}
	return type_code;
}

std::shared_ptr<vnx::TypeCode> Wallet_get_total_balances_for::static_create_type_code() {
	auto type_code = std::make_shared<vnx::TypeCode>();
	type_code->name = "mmx.Wallet.get_total_balances_for";
	type_code->type_hash = vnx::Hash64(0x784998cd7be76f95ull);
	type_code->code_hash = vnx::Hash64(0xc75701c3b14a0a86ull);
	type_code->is_native = true;
	type_code->is_class = true;
	type_code->is_method = true;
	type_code->native_size = sizeof(::mmx::Wallet_get_total_balances_for);
	type_code->create_value = []() -> std::shared_ptr<vnx::Value> { return std::make_shared<Wallet_get_total_balances_for>(); };
	type_code->is_const = true;
	type_code->return_type = ::mmx::Wallet_get_total_balances_for_return::static_get_type_code();
	type_code->fields.resize(2);
	{
		auto& field = type_code->fields[0];
		field.is_extended = true;
		field.name = "addresses";
		field.code = {12, 11, 32, 1};
	}
	{
		auto& field = type_code->fields[1];
		field.data_size = 4;
		field.name = "min_confirm";
		field.value = vnx::to_string(1);
		field.code = {3};
	}
	type_code->build();
	return type_code;
}


} // namespace mmx


namespace vnx {

void read(TypeInput& in, ::mmx::Wallet_get_total_balances_for& value, const TypeCode* type_code, const uint16_t* code) {
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
		if(const auto* const _field = type_code->field_map[1]) {
			vnx::read_value(_buf + _field->offset, value.min_confirm, _field->code.data());
		}
	}
	for(const auto* _field : type_code->ext_fields) {
		switch(_field->native_index) {
			case 0: vnx::read(in, value.addresses, type_code, _field->code.data()); break;
			default: vnx::skip(in, type_code, _field->code.data());
		}
	}
}

void write(TypeOutput& out, const ::mmx::Wallet_get_total_balances_for& value, const TypeCode* type_code, const uint16_t* code) {
	if(code && code[0] == CODE_OBJECT) {
		vnx::write(out, value.to_object(), nullptr, code);
		return;
	}
	if(!type_code || (code && code[0] == CODE_ANY)) {
		type_code = mmx::vnx_native_type_code_Wallet_get_total_balances_for;
		out.write_type_code(type_code);
		vnx::write_class_header<::mmx::Wallet_get_total_balances_for>(out);
	}
	else if(code && code[0] == CODE_STRUCT) {
		type_code = type_code->depends[code[1]];
	}
	char* const _buf = out.write(4);
	vnx::write_value(_buf + 0, value.min_confirm);
	vnx::write(out, value.addresses, type_code, type_code->fields[0].code.data());
}

void read(std::istream& in, ::mmx::Wallet_get_total_balances_for& value) {
	value.read(in);
}

void write(std::ostream& out, const ::mmx::Wallet_get_total_balances_for& value) {
	value.write(out);
}

void accept(Visitor& visitor, const ::mmx::Wallet_get_total_balances_for& value) {
	value.accept(visitor);
}

} // vnx
