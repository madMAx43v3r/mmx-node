
// AUTO GENERATED by vnxcppcodegen

#include <mmx/package.hxx>
#include <mmx/Transaction_is_signed.hxx>
#include <mmx/Transaction_is_signed_return.hxx>
#include <vnx/Value.h>

#include <vnx/vnx.h>


namespace mmx {


const vnx::Hash64 Transaction_is_signed::VNX_TYPE_HASH(0x3273a3ea7264e4f8ull);
const vnx::Hash64 Transaction_is_signed::VNX_CODE_HASH(0xa2406c0ffc94947ull);

vnx::Hash64 Transaction_is_signed::get_type_hash() const {
	return VNX_TYPE_HASH;
}

std::string Transaction_is_signed::get_type_name() const {
	return "mmx.Transaction.is_signed";
}

const vnx::TypeCode* Transaction_is_signed::get_type_code() const {
	return mmx::vnx_native_type_code_Transaction_is_signed;
}

std::shared_ptr<Transaction_is_signed> Transaction_is_signed::create() {
	return std::make_shared<Transaction_is_signed>();
}

std::shared_ptr<vnx::Value> Transaction_is_signed::clone() const {
	return std::make_shared<Transaction_is_signed>(*this);
}

void Transaction_is_signed::read(vnx::TypeInput& _in, const vnx::TypeCode* _type_code, const uint16_t* _code) {
	vnx::read(_in, *this, _type_code, _code);
}

void Transaction_is_signed::write(vnx::TypeOutput& _out, const vnx::TypeCode* _type_code, const uint16_t* _code) const {
	vnx::write(_out, *this, _type_code, _code);
}

void Transaction_is_signed::accept(vnx::Visitor& _visitor) const {
	const vnx::TypeCode* _type_code = mmx::vnx_native_type_code_Transaction_is_signed;
	_visitor.type_begin(*_type_code);
	_visitor.type_end(*_type_code);
}

void Transaction_is_signed::write(std::ostream& _out) const {
	_out << "{\"__type\": \"mmx.Transaction.is_signed\"";
	_out << "}";
}

void Transaction_is_signed::read(std::istream& _in) {
	if(auto _json = vnx::read_json(_in)) {
		from_object(_json->to_object());
	}
}

vnx::Object Transaction_is_signed::to_object() const {
	vnx::Object _object;
	_object["__type"] = "mmx.Transaction.is_signed";
	return _object;
}

void Transaction_is_signed::from_object(const vnx::Object& _object) {
}

vnx::Variant Transaction_is_signed::get_field(const std::string& _name) const {
	return vnx::Variant();
}

void Transaction_is_signed::set_field(const std::string& _name, const vnx::Variant& _value) {
}

/// \private
std::ostream& operator<<(std::ostream& _out, const Transaction_is_signed& _value) {
	_value.write(_out);
	return _out;
}

/// \private
std::istream& operator>>(std::istream& _in, Transaction_is_signed& _value) {
	_value.read(_in);
	return _in;
}

const vnx::TypeCode* Transaction_is_signed::static_get_type_code() {
	const vnx::TypeCode* type_code = vnx::get_type_code(VNX_TYPE_HASH);
	if(!type_code) {
		type_code = vnx::register_type_code(static_create_type_code());
	}
	return type_code;
}

std::shared_ptr<vnx::TypeCode> Transaction_is_signed::static_create_type_code() {
	auto type_code = std::make_shared<vnx::TypeCode>();
	type_code->name = "mmx.Transaction.is_signed";
	type_code->type_hash = vnx::Hash64(0x3273a3ea7264e4f8ull);
	type_code->code_hash = vnx::Hash64(0xa2406c0ffc94947ull);
	type_code->is_native = true;
	type_code->is_class = true;
	type_code->is_method = true;
	type_code->native_size = sizeof(::mmx::Transaction_is_signed);
	type_code->create_value = []() -> std::shared_ptr<vnx::Value> { return std::make_shared<Transaction_is_signed>(); };
	type_code->is_const = true;
	type_code->return_type = ::mmx::Transaction_is_signed_return::static_get_type_code();
	type_code->build();
	return type_code;
}


} // namespace mmx


namespace vnx {

void read(TypeInput& in, ::mmx::Transaction_is_signed& value, const TypeCode* type_code, const uint16_t* code) {
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
	in.read(type_code->total_field_size);
	if(type_code->is_matched) {
	}
	for(const auto* _field : type_code->ext_fields) {
		switch(_field->native_index) {
			default: vnx::skip(in, type_code, _field->code.data());
		}
	}
}

void write(TypeOutput& out, const ::mmx::Transaction_is_signed& value, const TypeCode* type_code, const uint16_t* code) {
	if(code && code[0] == CODE_OBJECT) {
		vnx::write(out, value.to_object(), nullptr, code);
		return;
	}
	if(!type_code || (code && code[0] == CODE_ANY)) {
		type_code = mmx::vnx_native_type_code_Transaction_is_signed;
		out.write_type_code(type_code);
		vnx::write_class_header<::mmx::Transaction_is_signed>(out);
	}
	else if(code && code[0] == CODE_STRUCT) {
		type_code = type_code->depends[code[1]];
	}
}

void read(std::istream& in, ::mmx::Transaction_is_signed& value) {
	value.read(in);
}

void write(std::ostream& out, const ::mmx::Transaction_is_signed& value) {
	value.write(out);
}

void accept(Visitor& visitor, const ::mmx::Transaction_is_signed& value) {
	value.accept(visitor);
}

} // vnx
