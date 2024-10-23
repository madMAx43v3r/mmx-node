
// AUTO GENERATED by vnxcppcodegen

#include <mmx/package.hxx>
#include <mmx/Wallet_plotnft_exec.hxx>
#include <mmx/Wallet_plotnft_exec_return.hxx>
#include <mmx/addr_t.hpp>
#include <mmx/spend_options_t.hxx>
#include <vnx/Value.h>
#include <vnx/Variant.hpp>

#include <vnx/vnx.h>


namespace mmx {


const vnx::Hash64 Wallet_plotnft_exec::VNX_TYPE_HASH(0xca9e9d0dfde795d1ull);
const vnx::Hash64 Wallet_plotnft_exec::VNX_CODE_HASH(0x8964a8e5cd090ae4ull);

vnx::Hash64 Wallet_plotnft_exec::get_type_hash() const {
	return VNX_TYPE_HASH;
}

std::string Wallet_plotnft_exec::get_type_name() const {
	return "mmx.Wallet.plotnft_exec";
}

const vnx::TypeCode* Wallet_plotnft_exec::get_type_code() const {
	return mmx::vnx_native_type_code_Wallet_plotnft_exec;
}

std::shared_ptr<Wallet_plotnft_exec> Wallet_plotnft_exec::create() {
	return std::make_shared<Wallet_plotnft_exec>();
}

std::shared_ptr<vnx::Value> Wallet_plotnft_exec::clone() const {
	return std::make_shared<Wallet_plotnft_exec>(*this);
}

void Wallet_plotnft_exec::read(vnx::TypeInput& _in, const vnx::TypeCode* _type_code, const uint16_t* _code) {
	vnx::read(_in, *this, _type_code, _code);
}

void Wallet_plotnft_exec::write(vnx::TypeOutput& _out, const vnx::TypeCode* _type_code, const uint16_t* _code) const {
	vnx::write(_out, *this, _type_code, _code);
}

void Wallet_plotnft_exec::accept(vnx::Visitor& _visitor) const {
	const vnx::TypeCode* _type_code = mmx::vnx_native_type_code_Wallet_plotnft_exec;
	_visitor.type_begin(*_type_code);
	_visitor.type_field(_type_code->fields[0], 0); vnx::accept(_visitor, address);
	_visitor.type_field(_type_code->fields[1], 1); vnx::accept(_visitor, method);
	_visitor.type_field(_type_code->fields[2], 2); vnx::accept(_visitor, args);
	_visitor.type_field(_type_code->fields[3], 3); vnx::accept(_visitor, options);
	_visitor.type_end(*_type_code);
}

void Wallet_plotnft_exec::write(std::ostream& _out) const {
	_out << "{\"__type\": \"mmx.Wallet.plotnft_exec\"";
	_out << ", \"address\": "; vnx::write(_out, address);
	_out << ", \"method\": "; vnx::write(_out, method);
	_out << ", \"args\": "; vnx::write(_out, args);
	_out << ", \"options\": "; vnx::write(_out, options);
	_out << "}";
}

void Wallet_plotnft_exec::read(std::istream& _in) {
	if(auto _json = vnx::read_json(_in)) {
		from_object(_json->to_object());
	}
}

vnx::Object Wallet_plotnft_exec::to_object() const {
	vnx::Object _object;
	_object["__type"] = "mmx.Wallet.plotnft_exec";
	_object["address"] = address;
	_object["method"] = method;
	_object["args"] = args;
	_object["options"] = options;
	return _object;
}

void Wallet_plotnft_exec::from_object(const vnx::Object& _object) {
	for(const auto& _entry : _object.field) {
		if(_entry.first == "address") {
			_entry.second.to(address);
		} else if(_entry.first == "args") {
			_entry.second.to(args);
		} else if(_entry.first == "method") {
			_entry.second.to(method);
		} else if(_entry.first == "options") {
			_entry.second.to(options);
		}
	}
}

vnx::Variant Wallet_plotnft_exec::get_field(const std::string& _name) const {
	if(_name == "address") {
		return vnx::Variant(address);
	}
	if(_name == "method") {
		return vnx::Variant(method);
	}
	if(_name == "args") {
		return vnx::Variant(args);
	}
	if(_name == "options") {
		return vnx::Variant(options);
	}
	return vnx::Variant();
}

void Wallet_plotnft_exec::set_field(const std::string& _name, const vnx::Variant& _value) {
	if(_name == "address") {
		_value.to(address);
	} else if(_name == "method") {
		_value.to(method);
	} else if(_name == "args") {
		_value.to(args);
	} else if(_name == "options") {
		_value.to(options);
	}
}

/// \private
std::ostream& operator<<(std::ostream& _out, const Wallet_plotnft_exec& _value) {
	_value.write(_out);
	return _out;
}

/// \private
std::istream& operator>>(std::istream& _in, Wallet_plotnft_exec& _value) {
	_value.read(_in);
	return _in;
}

const vnx::TypeCode* Wallet_plotnft_exec::static_get_type_code() {
	const vnx::TypeCode* type_code = vnx::get_type_code(VNX_TYPE_HASH);
	if(!type_code) {
		type_code = vnx::register_type_code(static_create_type_code());
	}
	return type_code;
}

std::shared_ptr<vnx::TypeCode> Wallet_plotnft_exec::static_create_type_code() {
	auto type_code = std::make_shared<vnx::TypeCode>();
	type_code->name = "mmx.Wallet.plotnft_exec";
	type_code->type_hash = vnx::Hash64(0xca9e9d0dfde795d1ull);
	type_code->code_hash = vnx::Hash64(0x8964a8e5cd090ae4ull);
	type_code->is_native = true;
	type_code->is_class = true;
	type_code->is_method = true;
	type_code->native_size = sizeof(::mmx::Wallet_plotnft_exec);
	type_code->create_value = []() -> std::shared_ptr<vnx::Value> { return std::make_shared<Wallet_plotnft_exec>(); };
	type_code->depends.resize(1);
	type_code->depends[0] = ::mmx::spend_options_t::static_get_type_code();
	type_code->is_const = true;
	type_code->return_type = ::mmx::Wallet_plotnft_exec_return::static_get_type_code();
	type_code->fields.resize(4);
	{
		auto& field = type_code->fields[0];
		field.is_extended = true;
		field.name = "address";
		field.code = {11, 32, 1};
	}
	{
		auto& field = type_code->fields[1];
		field.is_extended = true;
		field.name = "method";
		field.code = {32};
	}
	{
		auto& field = type_code->fields[2];
		field.is_extended = true;
		field.name = "args";
		field.code = {12, 17};
	}
	{
		auto& field = type_code->fields[3];
		field.is_extended = true;
		field.name = "options";
		field.code = {19, 0};
	}
	type_code->permission = "mmx.permission_e.SPENDING";
	type_code->build();
	return type_code;
}


} // namespace mmx


namespace vnx {

void read(TypeInput& in, ::mmx::Wallet_plotnft_exec& value, const TypeCode* type_code, const uint16_t* code) {
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
	in.read(type_code->total_field_size);
	if(type_code->is_matched) {
	}
	for(const auto* _field : type_code->ext_fields) {
		switch(_field->native_index) {
			case 0: vnx::read(in, value.address, type_code, _field->code.data()); break;
			case 1: vnx::read(in, value.method, type_code, _field->code.data()); break;
			case 2: vnx::read(in, value.args, type_code, _field->code.data()); break;
			case 3: vnx::read(in, value.options, type_code, _field->code.data()); break;
			default: vnx::skip(in, type_code, _field->code.data());
		}
	}
}

void write(TypeOutput& out, const ::mmx::Wallet_plotnft_exec& value, const TypeCode* type_code, const uint16_t* code) {
	if(code && code[0] == CODE_OBJECT) {
		vnx::write(out, value.to_object(), nullptr, code);
		return;
	}
	if(!type_code || (code && code[0] == CODE_ANY)) {
		type_code = mmx::vnx_native_type_code_Wallet_plotnft_exec;
		out.write_type_code(type_code);
		vnx::write_class_header<::mmx::Wallet_plotnft_exec>(out);
	}
	else if(code && code[0] == CODE_STRUCT) {
		type_code = type_code->depends[code[1]];
	}
	vnx::write(out, value.address, type_code, type_code->fields[0].code.data());
	vnx::write(out, value.method, type_code, type_code->fields[1].code.data());
	vnx::write(out, value.args, type_code, type_code->fields[2].code.data());
	vnx::write(out, value.options, type_code, type_code->fields[3].code.data());
}

void read(std::istream& in, ::mmx::Wallet_plotnft_exec& value) {
	value.read(in);
}

void write(std::ostream& out, const ::mmx::Wallet_plotnft_exec& value) {
	value.write(out);
}

void accept(Visitor& visitor, const ::mmx::Wallet_plotnft_exec& value) {
	value.accept(visitor);
}

} // vnx
