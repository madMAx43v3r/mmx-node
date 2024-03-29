
// AUTO GENERATED by vnxcppcodegen

#include <mmx/package.hxx>
#include <mmx/ProofOfSpace.hxx>
#include <mmx/ProofOfSpace_calc_hash.hxx>
#include <mmx/ProofOfSpace_calc_hash_return.hxx>
#include <mmx/ProofOfSpace_is_valid.hxx>
#include <mmx/ProofOfSpace_is_valid_return.hxx>
#include <mmx/ProofOfSpace_validate.hxx>
#include <mmx/ProofOfSpace_validate_return.hxx>
#include <mmx/bls_pubkey_t.hpp>
#include <mmx/hash_t.hpp>
#include <vnx/Value.h>

#include <vnx/vnx.h>


namespace mmx {


const vnx::Hash64 ProofOfSpace::VNX_TYPE_HASH(0x9269760ad5fd0058ull);
const vnx::Hash64 ProofOfSpace::VNX_CODE_HASH(0xace831450df9a40dull);

vnx::Hash64 ProofOfSpace::get_type_hash() const {
	return VNX_TYPE_HASH;
}

std::string ProofOfSpace::get_type_name() const {
	return "mmx.ProofOfSpace";
}

const vnx::TypeCode* ProofOfSpace::get_type_code() const {
	return mmx::vnx_native_type_code_ProofOfSpace;
}

std::shared_ptr<ProofOfSpace> ProofOfSpace::create() {
	return std::make_shared<ProofOfSpace>();
}

std::shared_ptr<vnx::Value> ProofOfSpace::clone() const {
	return std::make_shared<ProofOfSpace>(*this);
}

void ProofOfSpace::read(vnx::TypeInput& _in, const vnx::TypeCode* _type_code, const uint16_t* _code) {
	vnx::read(_in, *this, _type_code, _code);
}

void ProofOfSpace::write(vnx::TypeOutput& _out, const vnx::TypeCode* _type_code, const uint16_t* _code) const {
	vnx::write(_out, *this, _type_code, _code);
}

void ProofOfSpace::accept(vnx::Visitor& _visitor) const {
	const vnx::TypeCode* _type_code = mmx::vnx_native_type_code_ProofOfSpace;
	_visitor.type_begin(*_type_code);
	_visitor.type_field(_type_code->fields[0], 0); vnx::accept(_visitor, version);
	_visitor.type_field(_type_code->fields[1], 1); vnx::accept(_visitor, score);
	_visitor.type_field(_type_code->fields[2], 2); vnx::accept(_visitor, plot_id);
	_visitor.type_field(_type_code->fields[3], 3); vnx::accept(_visitor, plot_key);
	_visitor.type_field(_type_code->fields[4], 4); vnx::accept(_visitor, farmer_key);
	_visitor.type_end(*_type_code);
}

void ProofOfSpace::write(std::ostream& _out) const {
	_out << "{\"__type\": \"mmx.ProofOfSpace\"";
	_out << ", \"version\": "; vnx::write(_out, version);
	_out << ", \"score\": "; vnx::write(_out, score);
	_out << ", \"plot_id\": "; vnx::write(_out, plot_id);
	_out << ", \"plot_key\": "; vnx::write(_out, plot_key);
	_out << ", \"farmer_key\": "; vnx::write(_out, farmer_key);
	_out << "}";
}

void ProofOfSpace::read(std::istream& _in) {
	if(auto _json = vnx::read_json(_in)) {
		from_object(_json->to_object());
	}
}

vnx::Object ProofOfSpace::to_object() const {
	vnx::Object _object;
	_object["__type"] = "mmx.ProofOfSpace";
	_object["version"] = version;
	_object["score"] = score;
	_object["plot_id"] = plot_id;
	_object["plot_key"] = plot_key;
	_object["farmer_key"] = farmer_key;
	return _object;
}

void ProofOfSpace::from_object(const vnx::Object& _object) {
	for(const auto& _entry : _object.field) {
		if(_entry.first == "farmer_key") {
			_entry.second.to(farmer_key);
		} else if(_entry.first == "plot_id") {
			_entry.second.to(plot_id);
		} else if(_entry.first == "plot_key") {
			_entry.second.to(plot_key);
		} else if(_entry.first == "score") {
			_entry.second.to(score);
		} else if(_entry.first == "version") {
			_entry.second.to(version);
		}
	}
}

vnx::Variant ProofOfSpace::get_field(const std::string& _name) const {
	if(_name == "version") {
		return vnx::Variant(version);
	}
	if(_name == "score") {
		return vnx::Variant(score);
	}
	if(_name == "plot_id") {
		return vnx::Variant(plot_id);
	}
	if(_name == "plot_key") {
		return vnx::Variant(plot_key);
	}
	if(_name == "farmer_key") {
		return vnx::Variant(farmer_key);
	}
	return vnx::Variant();
}

void ProofOfSpace::set_field(const std::string& _name, const vnx::Variant& _value) {
	if(_name == "version") {
		_value.to(version);
	} else if(_name == "score") {
		_value.to(score);
	} else if(_name == "plot_id") {
		_value.to(plot_id);
	} else if(_name == "plot_key") {
		_value.to(plot_key);
	} else if(_name == "farmer_key") {
		_value.to(farmer_key);
	}
}

/// \private
std::ostream& operator<<(std::ostream& _out, const ProofOfSpace& _value) {
	_value.write(_out);
	return _out;
}

/// \private
std::istream& operator>>(std::istream& _in, ProofOfSpace& _value) {
	_value.read(_in);
	return _in;
}

const vnx::TypeCode* ProofOfSpace::static_get_type_code() {
	const vnx::TypeCode* type_code = vnx::get_type_code(VNX_TYPE_HASH);
	if(!type_code) {
		type_code = vnx::register_type_code(static_create_type_code());
	}
	return type_code;
}

std::shared_ptr<vnx::TypeCode> ProofOfSpace::static_create_type_code() {
	auto type_code = std::make_shared<vnx::TypeCode>();
	type_code->name = "mmx.ProofOfSpace";
	type_code->type_hash = vnx::Hash64(0x9269760ad5fd0058ull);
	type_code->code_hash = vnx::Hash64(0xace831450df9a40dull);
	type_code->is_native = true;
	type_code->is_class = true;
	type_code->native_size = sizeof(::mmx::ProofOfSpace);
	type_code->create_value = []() -> std::shared_ptr<vnx::Value> { return std::make_shared<ProofOfSpace>(); };
	type_code->methods.resize(3);
	type_code->methods[0] = ::mmx::ProofOfSpace_calc_hash::static_get_type_code();
	type_code->methods[1] = ::mmx::ProofOfSpace_is_valid::static_get_type_code();
	type_code->methods[2] = ::mmx::ProofOfSpace_validate::static_get_type_code();
	type_code->fields.resize(5);
	{
		auto& field = type_code->fields[0];
		field.data_size = 4;
		field.name = "version";
		field.code = {3};
	}
	{
		auto& field = type_code->fields[1];
		field.data_size = 4;
		field.name = "score";
		field.code = {3};
	}
	{
		auto& field = type_code->fields[2];
		field.is_extended = true;
		field.name = "plot_id";
		field.code = {11, 32, 1};
	}
	{
		auto& field = type_code->fields[3];
		field.is_extended = true;
		field.name = "plot_key";
		field.code = {11, 48, 1};
	}
	{
		auto& field = type_code->fields[4];
		field.is_extended = true;
		field.name = "farmer_key";
		field.code = {11, 48, 1};
	}
	type_code->build();
	return type_code;
}

std::shared_ptr<vnx::Value> ProofOfSpace::vnx_call_switch(std::shared_ptr<const vnx::Value> _method) {
	switch(_method->get_type_hash()) {
		case 0x4056d25a9096f144ull: {
			auto _args = std::static_pointer_cast<const ::mmx::ProofOfSpace_calc_hash>(_method);
			auto _return_value = ::mmx::ProofOfSpace_calc_hash_return::create();
			_return_value->_ret_0 = calc_hash(_args->full_hash);
			return _return_value;
		}
		case 0x143933f39ea710d1ull: {
			auto _args = std::static_pointer_cast<const ::mmx::ProofOfSpace_is_valid>(_method);
			auto _return_value = ::mmx::ProofOfSpace_is_valid_return::create();
			_return_value->_ret_0 = is_valid();
			return _return_value;
		}
		case 0x3586a00594c9af94ull: {
			auto _args = std::static_pointer_cast<const ::mmx::ProofOfSpace_validate>(_method);
			auto _return_value = ::mmx::ProofOfSpace_validate_return::create();
			validate();
			return _return_value;
		}
	}
	return nullptr;
}


} // namespace mmx


namespace vnx {

void read(TypeInput& in, ::mmx::ProofOfSpace& value, const TypeCode* type_code, const uint16_t* code) {
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
			vnx::read_value(_buf + _field->offset, value.version, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[1]) {
			vnx::read_value(_buf + _field->offset, value.score, _field->code.data());
		}
	}
	for(const auto* _field : type_code->ext_fields) {
		switch(_field->native_index) {
			case 2: vnx::read(in, value.plot_id, type_code, _field->code.data()); break;
			case 3: vnx::read(in, value.plot_key, type_code, _field->code.data()); break;
			case 4: vnx::read(in, value.farmer_key, type_code, _field->code.data()); break;
			default: vnx::skip(in, type_code, _field->code.data());
		}
	}
}

void write(TypeOutput& out, const ::mmx::ProofOfSpace& value, const TypeCode* type_code, const uint16_t* code) {
	if(code && code[0] == CODE_OBJECT) {
		vnx::write(out, value.to_object(), nullptr, code);
		return;
	}
	if(!type_code || (code && code[0] == CODE_ANY)) {
		type_code = mmx::vnx_native_type_code_ProofOfSpace;
		out.write_type_code(type_code);
		vnx::write_class_header<::mmx::ProofOfSpace>(out);
	}
	else if(code && code[0] == CODE_STRUCT) {
		type_code = type_code->depends[code[1]];
	}
	char* const _buf = out.write(8);
	vnx::write_value(_buf + 0, value.version);
	vnx::write_value(_buf + 4, value.score);
	vnx::write(out, value.plot_id, type_code, type_code->fields[2].code.data());
	vnx::write(out, value.plot_key, type_code, type_code->fields[3].code.data());
	vnx::write(out, value.farmer_key, type_code, type_code->fields[4].code.data());
}

void read(std::istream& in, ::mmx::ProofOfSpace& value) {
	value.read(in);
}

void write(std::ostream& out, const ::mmx::ProofOfSpace& value) {
	value.write(out);
}

void accept(Visitor& visitor, const ::mmx::ProofOfSpace& value) {
	value.accept(visitor);
}

} // vnx
