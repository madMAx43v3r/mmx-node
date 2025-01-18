
// AUTO GENERATED by vnxcppcodegen

#include <mmx/package.hxx>
#include <mmx/Wallet_set_address_count.hxx>
#include <mmx/Wallet_set_address_count_return.hxx>
#include <vnx/Value.h>

#include <vnx/vnx.h>


namespace mmx {


const vnx::Hash64 Wallet_set_address_count::VNX_TYPE_HASH(0x9638ddc0c1d52b15ull);
const vnx::Hash64 Wallet_set_address_count::VNX_CODE_HASH(0x7621e48cd485e3b8ull);

vnx::Hash64 Wallet_set_address_count::get_type_hash() const {
	return VNX_TYPE_HASH;
}

std::string Wallet_set_address_count::get_type_name() const {
	return "mmx.Wallet.set_address_count";
}

const vnx::TypeCode* Wallet_set_address_count::get_type_code() const {
	return mmx::vnx_native_type_code_Wallet_set_address_count;
}

std::shared_ptr<Wallet_set_address_count> Wallet_set_address_count::create() {
	return std::make_shared<Wallet_set_address_count>();
}

std::shared_ptr<vnx::Value> Wallet_set_address_count::clone() const {
	return std::make_shared<Wallet_set_address_count>(*this);
}

void Wallet_set_address_count::read(vnx::TypeInput& _in, const vnx::TypeCode* _type_code, const uint16_t* _code) {
	vnx::read(_in, *this, _type_code, _code);
}

void Wallet_set_address_count::write(vnx::TypeOutput& _out, const vnx::TypeCode* _type_code, const uint16_t* _code) const {
	vnx::write(_out, *this, _type_code, _code);
}

void Wallet_set_address_count::accept(vnx::Visitor& _visitor) const {
	const vnx::TypeCode* _type_code = mmx::vnx_native_type_code_Wallet_set_address_count;
	_visitor.type_begin(*_type_code);
	_visitor.type_field(_type_code->fields[0], 0); vnx::accept(_visitor, index);
	_visitor.type_field(_type_code->fields[1], 1); vnx::accept(_visitor, count);
	_visitor.type_end(*_type_code);
}

void Wallet_set_address_count::write(std::ostream& _out) const {
	_out << "{\"__type\": \"mmx.Wallet.set_address_count\"";
	_out << ", \"index\": "; vnx::write(_out, index);
	_out << ", \"count\": "; vnx::write(_out, count);
	_out << "}";
}

void Wallet_set_address_count::read(std::istream& _in) {
	if(auto _json = vnx::read_json(_in)) {
		from_object(_json->to_object());
	}
}

vnx::Object Wallet_set_address_count::to_object() const {
	vnx::Object _object;
	_object["__type"] = "mmx.Wallet.set_address_count";
	_object["index"] = index;
	_object["count"] = count;
	return _object;
}

void Wallet_set_address_count::from_object(const vnx::Object& _object) {
	for(const auto& _entry : _object.field) {
		if(_entry.first == "count") {
			_entry.second.to(count);
		} else if(_entry.first == "index") {
			_entry.second.to(index);
		}
	}
}

vnx::Variant Wallet_set_address_count::get_field(const std::string& _name) const {
	if(_name == "index") {
		return vnx::Variant(index);
	}
	if(_name == "count") {
		return vnx::Variant(count);
	}
	return vnx::Variant();
}

void Wallet_set_address_count::set_field(const std::string& _name, const vnx::Variant& _value) {
	if(_name == "index") {
		_value.to(index);
	} else if(_name == "count") {
		_value.to(count);
	}
}

/// \private
std::ostream& operator<<(std::ostream& _out, const Wallet_set_address_count& _value) {
	_value.write(_out);
	return _out;
}

/// \private
std::istream& operator>>(std::istream& _in, Wallet_set_address_count& _value) {
	_value.read(_in);
	return _in;
}

const vnx::TypeCode* Wallet_set_address_count::static_get_type_code() {
	const vnx::TypeCode* type_code = vnx::get_type_code(VNX_TYPE_HASH);
	if(!type_code) {
		type_code = vnx::register_type_code(static_create_type_code());
	}
	return type_code;
}

std::shared_ptr<vnx::TypeCode> Wallet_set_address_count::static_create_type_code() {
	auto type_code = std::make_shared<vnx::TypeCode>();
	type_code->name = "mmx.Wallet.set_address_count";
	type_code->type_hash = vnx::Hash64(0x9638ddc0c1d52b15ull);
	type_code->code_hash = vnx::Hash64(0x7621e48cd485e3b8ull);
	type_code->is_native = true;
	type_code->is_class = true;
	type_code->is_method = true;
	type_code->native_size = sizeof(::mmx::Wallet_set_address_count);
	type_code->create_value = []() -> std::shared_ptr<vnx::Value> { return std::make_shared<Wallet_set_address_count>(); };
	type_code->return_type = ::mmx::Wallet_set_address_count_return::static_get_type_code();
	type_code->fields.resize(2);
	{
		auto& field = type_code->fields[0];
		field.data_size = 4;
		field.name = "index";
		field.code = {3};
	}
	{
		auto& field = type_code->fields[1];
		field.data_size = 4;
		field.name = "count";
		field.code = {3};
	}
	type_code->build();
	return type_code;
}


} // namespace mmx


namespace vnx {

void read(TypeInput& in, ::mmx::Wallet_set_address_count& value, const TypeCode* type_code, const uint16_t* code) {
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
		if(const auto* const _field = type_code->field_map[1]) {
			vnx::read_value(_buf + _field->offset, value.count, _field->code.data());
		}
	}
	for(const auto* _field : type_code->ext_fields) {
		switch(_field->native_index) {
			default: vnx::skip(in, type_code, _field->code.data());
		}
	}
}

void write(TypeOutput& out, const ::mmx::Wallet_set_address_count& value, const TypeCode* type_code, const uint16_t* code) {
	if(code && code[0] == CODE_OBJECT) {
		vnx::write(out, value.to_object(), nullptr, code);
		return;
	}
	if(!type_code || (code && code[0] == CODE_ANY)) {
		type_code = mmx::vnx_native_type_code_Wallet_set_address_count;
		out.write_type_code(type_code);
		vnx::write_class_header<::mmx::Wallet_set_address_count>(out);
	}
	else if(code && code[0] == CODE_STRUCT) {
		type_code = type_code->depends[code[1]];
	}
	auto* const _buf = out.write(8);
	vnx::write_value(_buf + 0, value.index);
	vnx::write_value(_buf + 4, value.count);
}

void read(std::istream& in, ::mmx::Wallet_set_address_count& value) {
	value.read(in);
}

void write(std::ostream& out, const ::mmx::Wallet_set_address_count& value) {
	value.write(out);
}

void accept(Visitor& visitor, const ::mmx::Wallet_set_address_count& value) {
	value.accept(visitor);
}

} // vnx
