
// AUTO GENERATED by vnxcppcodegen

#include <mmx/package.hxx>
#include <mmx/TransactionBase.hxx>
#include <mmx/ChainParams.hxx>
#include <mmx/hash_t.hpp>
#include <vnx/Value.h>

#include <vnx/vnx.h>


namespace mmx {


const vnx::Hash64 TransactionBase::VNX_TYPE_HASH(0x6697ffbf3611887dull);
const vnx::Hash64 TransactionBase::VNX_CODE_HASH(0x2693a4e1bdd90f6eull);

vnx::Hash64 TransactionBase::get_type_hash() const {
	return VNX_TYPE_HASH;
}

std::string TransactionBase::get_type_name() const {
	return "mmx.TransactionBase";
}

const vnx::TypeCode* TransactionBase::get_type_code() const {
	return mmx::vnx_native_type_code_TransactionBase;
}

std::shared_ptr<TransactionBase> TransactionBase::create() {
	return std::make_shared<TransactionBase>();
}

std::shared_ptr<vnx::Value> TransactionBase::clone() const {
	return std::make_shared<TransactionBase>(*this);
}

void TransactionBase::read(vnx::TypeInput& _in, const vnx::TypeCode* _type_code, const uint16_t* _code) {
	vnx::read(_in, *this, _type_code, _code);
}

void TransactionBase::write(vnx::TypeOutput& _out, const vnx::TypeCode* _type_code, const uint16_t* _code) const {
	vnx::write(_out, *this, _type_code, _code);
}

void TransactionBase::accept(vnx::Visitor& _visitor) const {
	const vnx::TypeCode* _type_code = mmx::vnx_native_type_code_TransactionBase;
	_visitor.type_begin(*_type_code);
	_visitor.type_field(_type_code->fields[0], 0); vnx::accept(_visitor, id);
	_visitor.type_end(*_type_code);
}

void TransactionBase::write(std::ostream& _out) const {
	_out << "{\"__type\": \"mmx.TransactionBase\"";
	_out << ", \"id\": "; vnx::write(_out, id);
	_out << "}";
}

void TransactionBase::read(std::istream& _in) {
	if(auto _json = vnx::read_json(_in)) {
		from_object(_json->to_object());
	}
}

vnx::Object TransactionBase::to_object() const {
	vnx::Object _object;
	_object["__type"] = "mmx.TransactionBase";
	_object["id"] = id;
	return _object;
}

void TransactionBase::from_object(const vnx::Object& _object) {
	for(const auto& _entry : _object.field) {
		if(_entry.first == "id") {
			_entry.second.to(id);
		}
	}
}

vnx::Variant TransactionBase::get_field(const std::string& _name) const {
	if(_name == "id") {
		return vnx::Variant(id);
	}
	return vnx::Variant();
}

void TransactionBase::set_field(const std::string& _name, const vnx::Variant& _value) {
	if(_name == "id") {
		_value.to(id);
	}
}

/// \private
std::ostream& operator<<(std::ostream& _out, const TransactionBase& _value) {
	_value.write(_out);
	return _out;
}

/// \private
std::istream& operator>>(std::istream& _in, TransactionBase& _value) {
	_value.read(_in);
	return _in;
}

const vnx::TypeCode* TransactionBase::static_get_type_code() {
	const vnx::TypeCode* type_code = vnx::get_type_code(VNX_TYPE_HASH);
	if(!type_code) {
		type_code = vnx::register_type_code(static_create_type_code());
	}
	return type_code;
}

std::shared_ptr<vnx::TypeCode> TransactionBase::static_create_type_code() {
	auto type_code = std::make_shared<vnx::TypeCode>();
	type_code->name = "mmx.TransactionBase";
	type_code->type_hash = vnx::Hash64(0x6697ffbf3611887dull);
	type_code->code_hash = vnx::Hash64(0x2693a4e1bdd90f6eull);
	type_code->is_native = true;
	type_code->is_class = true;
	type_code->native_size = sizeof(::mmx::TransactionBase);
	type_code->create_value = []() -> std::shared_ptr<vnx::Value> { return std::make_shared<TransactionBase>(); };
	type_code->fields.resize(1);
	{
		auto& field = type_code->fields[0];
		field.is_extended = true;
		field.name = "id";
		field.code = {11, 32, 1};
	}
	type_code->build();
	return type_code;
}

std::shared_ptr<vnx::Value> TransactionBase::vnx_call_switch(std::shared_ptr<const vnx::Value> _method) {
	switch(_method->get_type_hash()) {
	}
	return nullptr;
}


} // namespace mmx


namespace vnx {

void read(TypeInput& in, ::mmx::TransactionBase& value, const TypeCode* type_code, const uint16_t* code) {
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
			case 0: vnx::read(in, value.id, type_code, _field->code.data()); break;
			default: vnx::skip(in, type_code, _field->code.data());
		}
	}
}

void write(TypeOutput& out, const ::mmx::TransactionBase& value, const TypeCode* type_code, const uint16_t* code) {
	if(code && code[0] == CODE_OBJECT) {
		vnx::write(out, value.to_object(), nullptr, code);
		return;
	}
	if(!type_code || (code && code[0] == CODE_ANY)) {
		type_code = mmx::vnx_native_type_code_TransactionBase;
		out.write_type_code(type_code);
		vnx::write_class_header<::mmx::TransactionBase>(out);
	}
	else if(code && code[0] == CODE_STRUCT) {
		type_code = type_code->depends[code[1]];
	}
	vnx::write(out, value.id, type_code, type_code->fields[0].code.data());
}

void read(std::istream& in, ::mmx::TransactionBase& value) {
	value.read(in);
}

void write(std::ostream& out, const ::mmx::TransactionBase& value) {
	value.write(out);
}

void accept(Visitor& visitor, const ::mmx::TransactionBase& value) {
	value.accept(visitor);
}

} // vnx
