
// AUTO GENERATED by vnxcppcodegen

#include <mmx/package.hxx>
#include <mmx/Harvester_rem_plot_dir.hxx>
#include <mmx/Harvester_rem_plot_dir_return.hxx>
#include <vnx/Value.h>

#include <vnx/vnx.h>


namespace mmx {


const vnx::Hash64 Harvester_rem_plot_dir::VNX_TYPE_HASH(0x57674e56f3ab6076ull);
const vnx::Hash64 Harvester_rem_plot_dir::VNX_CODE_HASH(0xa08c1cef8ea0dc95ull);

vnx::Hash64 Harvester_rem_plot_dir::get_type_hash() const {
	return VNX_TYPE_HASH;
}

std::string Harvester_rem_plot_dir::get_type_name() const {
	return "mmx.Harvester.rem_plot_dir";
}

const vnx::TypeCode* Harvester_rem_plot_dir::get_type_code() const {
	return mmx::vnx_native_type_code_Harvester_rem_plot_dir;
}

std::shared_ptr<Harvester_rem_plot_dir> Harvester_rem_plot_dir::create() {
	return std::make_shared<Harvester_rem_plot_dir>();
}

std::shared_ptr<vnx::Value> Harvester_rem_plot_dir::clone() const {
	return std::make_shared<Harvester_rem_plot_dir>(*this);
}

void Harvester_rem_plot_dir::read(vnx::TypeInput& _in, const vnx::TypeCode* _type_code, const uint16_t* _code) {
	vnx::read(_in, *this, _type_code, _code);
}

void Harvester_rem_plot_dir::write(vnx::TypeOutput& _out, const vnx::TypeCode* _type_code, const uint16_t* _code) const {
	vnx::write(_out, *this, _type_code, _code);
}

void Harvester_rem_plot_dir::accept(vnx::Visitor& _visitor) const {
	const vnx::TypeCode* _type_code = mmx::vnx_native_type_code_Harvester_rem_plot_dir;
	_visitor.type_begin(*_type_code);
	_visitor.type_field(_type_code->fields[0], 0); vnx::accept(_visitor, path);
	_visitor.type_end(*_type_code);
}

void Harvester_rem_plot_dir::write(std::ostream& _out) const {
	_out << "{\"__type\": \"mmx.Harvester.rem_plot_dir\"";
	_out << ", \"path\": "; vnx::write(_out, path);
	_out << "}";
}

void Harvester_rem_plot_dir::read(std::istream& _in) {
	if(auto _json = vnx::read_json(_in)) {
		from_object(_json->to_object());
	}
}

vnx::Object Harvester_rem_plot_dir::to_object() const {
	vnx::Object _object;
	_object["__type"] = "mmx.Harvester.rem_plot_dir";
	_object["path"] = path;
	return _object;
}

void Harvester_rem_plot_dir::from_object(const vnx::Object& _object) {
	for(const auto& _entry : _object.field) {
		if(_entry.first == "path") {
			_entry.second.to(path);
		}
	}
}

vnx::Variant Harvester_rem_plot_dir::get_field(const std::string& _name) const {
	if(_name == "path") {
		return vnx::Variant(path);
	}
	return vnx::Variant();
}

void Harvester_rem_plot_dir::set_field(const std::string& _name, const vnx::Variant& _value) {
	if(_name == "path") {
		_value.to(path);
	}
}

/// \private
std::ostream& operator<<(std::ostream& _out, const Harvester_rem_plot_dir& _value) {
	_value.write(_out);
	return _out;
}

/// \private
std::istream& operator>>(std::istream& _in, Harvester_rem_plot_dir& _value) {
	_value.read(_in);
	return _in;
}

const vnx::TypeCode* Harvester_rem_plot_dir::static_get_type_code() {
	const vnx::TypeCode* type_code = vnx::get_type_code(VNX_TYPE_HASH);
	if(!type_code) {
		type_code = vnx::register_type_code(static_create_type_code());
	}
	return type_code;
}

std::shared_ptr<vnx::TypeCode> Harvester_rem_plot_dir::static_create_type_code() {
	auto type_code = std::make_shared<vnx::TypeCode>();
	type_code->name = "mmx.Harvester.rem_plot_dir";
	type_code->type_hash = vnx::Hash64(0x57674e56f3ab6076ull);
	type_code->code_hash = vnx::Hash64(0xa08c1cef8ea0dc95ull);
	type_code->is_native = true;
	type_code->is_class = true;
	type_code->is_method = true;
	type_code->native_size = sizeof(::mmx::Harvester_rem_plot_dir);
	type_code->create_value = []() -> std::shared_ptr<vnx::Value> { return std::make_shared<Harvester_rem_plot_dir>(); };
	type_code->return_type = ::mmx::Harvester_rem_plot_dir_return::static_get_type_code();
	type_code->fields.resize(1);
	{
		auto& field = type_code->fields[0];
		field.is_extended = true;
		field.name = "path";
		field.code = {32};
	}
	type_code->build();
	return type_code;
}


} // namespace mmx


namespace vnx {

void read(TypeInput& in, ::mmx::Harvester_rem_plot_dir& value, const TypeCode* type_code, const uint16_t* code) {
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
			case 0: vnx::read(in, value.path, type_code, _field->code.data()); break;
			default: vnx::skip(in, type_code, _field->code.data());
		}
	}
}

void write(TypeOutput& out, const ::mmx::Harvester_rem_plot_dir& value, const TypeCode* type_code, const uint16_t* code) {
	if(code && code[0] == CODE_OBJECT) {
		vnx::write(out, value.to_object(), nullptr, code);
		return;
	}
	if(!type_code || (code && code[0] == CODE_ANY)) {
		type_code = mmx::vnx_native_type_code_Harvester_rem_plot_dir;
		out.write_type_code(type_code);
		vnx::write_class_header<::mmx::Harvester_rem_plot_dir>(out);
	}
	else if(code && code[0] == CODE_STRUCT) {
		type_code = type_code->depends[code[1]];
	}
	vnx::write(out, value.path, type_code, type_code->fields[0].code.data());
}

void read(std::istream& in, ::mmx::Harvester_rem_plot_dir& value) {
	value.read(in);
}

void write(std::ostream& out, const ::mmx::Harvester_rem_plot_dir& value) {
	value.write(out);
}

void accept(Visitor& visitor, const ::mmx::Harvester_rem_plot_dir& value) {
	value.accept(visitor);
}

} // vnx
