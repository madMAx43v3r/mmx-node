
// AUTO GENERATED by vnxcppcodegen

#include <mmx/package.hxx>
#include <mmx/Wallet_send_from.hxx>
#include <mmx/Wallet_send_from_return.hxx>
#include <mmx/addr_t.hpp>
#include <mmx/spend_options_t.hxx>
#include <mmx/uint128.hpp>
#include <vnx/Value.h>

#include <vnx/vnx.h>


namespace mmx {


const vnx::Hash64 Wallet_send_from::VNX_TYPE_HASH(0x40c3c88665341592ull);
const vnx::Hash64 Wallet_send_from::VNX_CODE_HASH(0xf1235b64d0ec4f61ull);

vnx::Hash64 Wallet_send_from::get_type_hash() const {
	return VNX_TYPE_HASH;
}

std::string Wallet_send_from::get_type_name() const {
	return "mmx.Wallet.send_from";
}

const vnx::TypeCode* Wallet_send_from::get_type_code() const {
	return mmx::vnx_native_type_code_Wallet_send_from;
}

std::shared_ptr<Wallet_send_from> Wallet_send_from::create() {
	return std::make_shared<Wallet_send_from>();
}

std::shared_ptr<vnx::Value> Wallet_send_from::clone() const {
	return std::make_shared<Wallet_send_from>(*this);
}

void Wallet_send_from::read(vnx::TypeInput& _in, const vnx::TypeCode* _type_code, const uint16_t* _code) {
	vnx::read(_in, *this, _type_code, _code);
}

void Wallet_send_from::write(vnx::TypeOutput& _out, const vnx::TypeCode* _type_code, const uint16_t* _code) const {
	vnx::write(_out, *this, _type_code, _code);
}

void Wallet_send_from::accept(vnx::Visitor& _visitor) const {
	const vnx::TypeCode* _type_code = mmx::vnx_native_type_code_Wallet_send_from;
	_visitor.type_begin(*_type_code);
	_visitor.type_field(_type_code->fields[0], 0); vnx::accept(_visitor, index);
	_visitor.type_field(_type_code->fields[1], 1); vnx::accept(_visitor, amount);
	_visitor.type_field(_type_code->fields[2], 2); vnx::accept(_visitor, dst_addr);
	_visitor.type_field(_type_code->fields[3], 3); vnx::accept(_visitor, src_addr);
	_visitor.type_field(_type_code->fields[4], 4); vnx::accept(_visitor, currency);
	_visitor.type_field(_type_code->fields[5], 5); vnx::accept(_visitor, options);
	_visitor.type_end(*_type_code);
}

void Wallet_send_from::write(std::ostream& _out) const {
	_out << "{\"__type\": \"mmx.Wallet.send_from\"";
	_out << ", \"index\": "; vnx::write(_out, index);
	_out << ", \"amount\": "; vnx::write(_out, amount);
	_out << ", \"dst_addr\": "; vnx::write(_out, dst_addr);
	_out << ", \"src_addr\": "; vnx::write(_out, src_addr);
	_out << ", \"currency\": "; vnx::write(_out, currency);
	_out << ", \"options\": "; vnx::write(_out, options);
	_out << "}";
}

void Wallet_send_from::read(std::istream& _in) {
	if(auto _json = vnx::read_json(_in)) {
		from_object(_json->to_object());
	}
}

vnx::Object Wallet_send_from::to_object() const {
	vnx::Object _object;
	_object["__type"] = "mmx.Wallet.send_from";
	_object["index"] = index;
	_object["amount"] = amount;
	_object["dst_addr"] = dst_addr;
	_object["src_addr"] = src_addr;
	_object["currency"] = currency;
	_object["options"] = options;
	return _object;
}

void Wallet_send_from::from_object(const vnx::Object& _object) {
	for(const auto& _entry : _object.field) {
		if(_entry.first == "amount") {
			_entry.second.to(amount);
		} else if(_entry.first == "currency") {
			_entry.second.to(currency);
		} else if(_entry.first == "dst_addr") {
			_entry.second.to(dst_addr);
		} else if(_entry.first == "index") {
			_entry.second.to(index);
		} else if(_entry.first == "options") {
			_entry.second.to(options);
		} else if(_entry.first == "src_addr") {
			_entry.second.to(src_addr);
		}
	}
}

vnx::Variant Wallet_send_from::get_field(const std::string& _name) const {
	if(_name == "index") {
		return vnx::Variant(index);
	}
	if(_name == "amount") {
		return vnx::Variant(amount);
	}
	if(_name == "dst_addr") {
		return vnx::Variant(dst_addr);
	}
	if(_name == "src_addr") {
		return vnx::Variant(src_addr);
	}
	if(_name == "currency") {
		return vnx::Variant(currency);
	}
	if(_name == "options") {
		return vnx::Variant(options);
	}
	return vnx::Variant();
}

void Wallet_send_from::set_field(const std::string& _name, const vnx::Variant& _value) {
	if(_name == "index") {
		_value.to(index);
	} else if(_name == "amount") {
		_value.to(amount);
	} else if(_name == "dst_addr") {
		_value.to(dst_addr);
	} else if(_name == "src_addr") {
		_value.to(src_addr);
	} else if(_name == "currency") {
		_value.to(currency);
	} else if(_name == "options") {
		_value.to(options);
	}
}

/// \private
std::ostream& operator<<(std::ostream& _out, const Wallet_send_from& _value) {
	_value.write(_out);
	return _out;
}

/// \private
std::istream& operator>>(std::istream& _in, Wallet_send_from& _value) {
	_value.read(_in);
	return _in;
}

const vnx::TypeCode* Wallet_send_from::static_get_type_code() {
	const vnx::TypeCode* type_code = vnx::get_type_code(VNX_TYPE_HASH);
	if(!type_code) {
		type_code = vnx::register_type_code(static_create_type_code());
	}
	return type_code;
}

std::shared_ptr<vnx::TypeCode> Wallet_send_from::static_create_type_code() {
	auto type_code = std::make_shared<vnx::TypeCode>();
	type_code->name = "mmx.Wallet.send_from";
	type_code->type_hash = vnx::Hash64(0x40c3c88665341592ull);
	type_code->code_hash = vnx::Hash64(0xf1235b64d0ec4f61ull);
	type_code->is_native = true;
	type_code->is_class = true;
	type_code->is_method = true;
	type_code->native_size = sizeof(::mmx::Wallet_send_from);
	type_code->create_value = []() -> std::shared_ptr<vnx::Value> { return std::make_shared<Wallet_send_from>(); };
	type_code->depends.resize(1);
	type_code->depends[0] = ::mmx::spend_options_t::static_get_type_code();
	type_code->is_const = true;
	type_code->return_type = ::mmx::Wallet_send_from_return::static_get_type_code();
	type_code->fields.resize(6);
	{
		auto& field = type_code->fields[0];
		field.data_size = 4;
		field.name = "index";
		field.code = {3};
	}
	{
		auto& field = type_code->fields[1];
		field.is_extended = true;
		field.name = "amount";
		field.code = {11, 16, 1};
	}
	{
		auto& field = type_code->fields[2];
		field.is_extended = true;
		field.name = "dst_addr";
		field.code = {11, 32, 1};
	}
	{
		auto& field = type_code->fields[3];
		field.is_extended = true;
		field.name = "src_addr";
		field.code = {11, 32, 1};
	}
	{
		auto& field = type_code->fields[4];
		field.is_extended = true;
		field.name = "currency";
		field.code = {11, 32, 1};
	}
	{
		auto& field = type_code->fields[5];
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

void read(TypeInput& in, ::mmx::Wallet_send_from& value, const TypeCode* type_code, const uint16_t* code) {
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
			vnx::read_value(_buf + _field->offset, value.index, _field->code.data());
		}
	}
	for(const auto* _field : type_code->ext_fields) {
		switch(_field->native_index) {
			case 1: vnx::read(in, value.amount, type_code, _field->code.data()); break;
			case 2: vnx::read(in, value.dst_addr, type_code, _field->code.data()); break;
			case 3: vnx::read(in, value.src_addr, type_code, _field->code.data()); break;
			case 4: vnx::read(in, value.currency, type_code, _field->code.data()); break;
			case 5: vnx::read(in, value.options, type_code, _field->code.data()); break;
			default: vnx::skip(in, type_code, _field->code.data());
		}
	}
}

void write(TypeOutput& out, const ::mmx::Wallet_send_from& value, const TypeCode* type_code, const uint16_t* code) {
	if(code && code[0] == CODE_OBJECT) {
		vnx::write(out, value.to_object(), nullptr, code);
		return;
	}
	if(!type_code || (code && code[0] == CODE_ANY)) {
		type_code = mmx::vnx_native_type_code_Wallet_send_from;
		out.write_type_code(type_code);
		vnx::write_class_header<::mmx::Wallet_send_from>(out);
	}
	else if(code && code[0] == CODE_STRUCT) {
		type_code = type_code->depends[code[1]];
	}
	auto* const _buf = out.write(4);
	vnx::write_value(_buf + 0, value.index);
	vnx::write(out, value.amount, type_code, type_code->fields[1].code.data());
	vnx::write(out, value.dst_addr, type_code, type_code->fields[2].code.data());
	vnx::write(out, value.src_addr, type_code, type_code->fields[3].code.data());
	vnx::write(out, value.currency, type_code, type_code->fields[4].code.data());
	vnx::write(out, value.options, type_code, type_code->fields[5].code.data());
}

void read(std::istream& in, ::mmx::Wallet_send_from& value) {
	value.read(in);
}

void write(std::ostream& out, const ::mmx::Wallet_send_from& value) {
	value.write(out);
}

void accept(Visitor& visitor, const ::mmx::Wallet_send_from& value) {
	value.accept(visitor);
}

} // vnx
