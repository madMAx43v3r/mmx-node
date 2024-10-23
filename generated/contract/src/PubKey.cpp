
// AUTO GENERATED by vnxcppcodegen

#include <mmx/contract/package.hxx>
#include <mmx/contract/PubKey.hxx>
#include <mmx/Contract.hxx>
#include <mmx/Solution.hxx>
#include <mmx/addr_t.hpp>
#include <mmx/hash_t.hpp>
#include <vnx/Variant.hpp>

#include <vnx/vnx.h>


namespace mmx {
namespace contract {


const vnx::Hash64 PubKey::VNX_TYPE_HASH(0x9b3cd508d7f41423ull);
const vnx::Hash64 PubKey::VNX_CODE_HASH(0xd94be3121579efceull);

vnx::Hash64 PubKey::get_type_hash() const {
	return VNX_TYPE_HASH;
}

std::string PubKey::get_type_name() const {
	return "mmx.contract.PubKey";
}

const vnx::TypeCode* PubKey::get_type_code() const {
	return mmx::contract::vnx_native_type_code_PubKey;
}

std::shared_ptr<PubKey> PubKey::create() {
	return std::make_shared<PubKey>();
}

std::shared_ptr<vnx::Value> PubKey::clone() const {
	return std::make_shared<PubKey>(*this);
}

void PubKey::read(vnx::TypeInput& _in, const vnx::TypeCode* _type_code, const uint16_t* _code) {
	vnx::read(_in, *this, _type_code, _code);
}

void PubKey::write(vnx::TypeOutput& _out, const vnx::TypeCode* _type_code, const uint16_t* _code) const {
	vnx::write(_out, *this, _type_code, _code);
}

void PubKey::accept(vnx::Visitor& _visitor) const {
	const vnx::TypeCode* _type_code = mmx::contract::vnx_native_type_code_PubKey;
	_visitor.type_begin(*_type_code);
	_visitor.type_field(_type_code->fields[0], 0); vnx::accept(_visitor, version);
	_visitor.type_field(_type_code->fields[1], 1); vnx::accept(_visitor, address);
	_visitor.type_end(*_type_code);
}

void PubKey::write(std::ostream& _out) const {
	_out << "{\"__type\": \"mmx.contract.PubKey\"";
	_out << ", \"version\": "; vnx::write(_out, version);
	_out << ", \"address\": "; vnx::write(_out, address);
	_out << "}";
}

void PubKey::read(std::istream& _in) {
	if(auto _json = vnx::read_json(_in)) {
		from_object(_json->to_object());
	}
}

vnx::Object PubKey::to_object() const {
	vnx::Object _object;
	_object["__type"] = "mmx.contract.PubKey";
	_object["version"] = version;
	_object["address"] = address;
	return _object;
}

void PubKey::from_object(const vnx::Object& _object) {
	for(const auto& _entry : _object.field) {
		if(_entry.first == "address") {
			_entry.second.to(address);
		} else if(_entry.first == "version") {
			_entry.second.to(version);
		}
	}
}

vnx::Variant PubKey::get_field(const std::string& _name) const {
	if(_name == "version") {
		return vnx::Variant(version);
	}
	if(_name == "address") {
		return vnx::Variant(address);
	}
	return vnx::Variant();
}

void PubKey::set_field(const std::string& _name, const vnx::Variant& _value) {
	if(_name == "version") {
		_value.to(version);
	} else if(_name == "address") {
		_value.to(address);
	}
}

/// \private
std::ostream& operator<<(std::ostream& _out, const PubKey& _value) {
	_value.write(_out);
	return _out;
}

/// \private
std::istream& operator>>(std::istream& _in, PubKey& _value) {
	_value.read(_in);
	return _in;
}

const vnx::TypeCode* PubKey::static_get_type_code() {
	const vnx::TypeCode* type_code = vnx::get_type_code(VNX_TYPE_HASH);
	if(!type_code) {
		type_code = vnx::register_type_code(static_create_type_code());
	}
	return type_code;
}

std::shared_ptr<vnx::TypeCode> PubKey::static_create_type_code() {
	auto type_code = std::make_shared<vnx::TypeCode>();
	type_code->name = "mmx.contract.PubKey";
	type_code->type_hash = vnx::Hash64(0x9b3cd508d7f41423ull);
	type_code->code_hash = vnx::Hash64(0xd94be3121579efceull);
	type_code->is_native = true;
	type_code->is_class = true;
	type_code->native_size = sizeof(::mmx::contract::PubKey);
	type_code->parents.resize(1);
	type_code->parents[0] = ::mmx::Contract::static_get_type_code();
	type_code->create_value = []() -> std::shared_ptr<vnx::Value> { return std::make_shared<PubKey>(); };
	type_code->fields.resize(2);
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
	type_code->build();
	return type_code;
}

std::shared_ptr<vnx::Value> PubKey::vnx_call_switch(std::shared_ptr<const vnx::Value> _method) {
	switch(_method->get_type_hash()) {
	}
	return nullptr;
}


} // namespace mmx
} // namespace contract


namespace vnx {

void read(TypeInput& in, ::mmx::contract::PubKey& value, const TypeCode* type_code, const uint16_t* code) {
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
	}
	for(const auto* _field : type_code->ext_fields) {
		switch(_field->native_index) {
			case 1: vnx::read(in, value.address, type_code, _field->code.data()); break;
			default: vnx::skip(in, type_code, _field->code.data());
		}
	}
}

void write(TypeOutput& out, const ::mmx::contract::PubKey& value, const TypeCode* type_code, const uint16_t* code) {
	if(code && code[0] == CODE_OBJECT) {
		vnx::write(out, value.to_object(), nullptr, code);
		return;
	}
	if(!type_code || (code && code[0] == CODE_ANY)) {
		type_code = mmx::contract::vnx_native_type_code_PubKey;
		out.write_type_code(type_code);
		vnx::write_class_header<::mmx::contract::PubKey>(out);
	}
	else if(code && code[0] == CODE_STRUCT) {
		type_code = type_code->depends[code[1]];
	}
	auto* const _buf = out.write(4);
	vnx::write_value(_buf + 0, value.version);
	vnx::write(out, value.address, type_code, type_code->fields[1].code.data());
}

void read(std::istream& in, ::mmx::contract::PubKey& value) {
	value.read(in);
}

void write(std::ostream& out, const ::mmx::contract::PubKey& value) {
	value.write(out);
}

void accept(Visitor& visitor, const ::mmx::contract::PubKey& value) {
	value.accept(visitor);
}

} // vnx
