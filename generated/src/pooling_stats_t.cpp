
// AUTO GENERATED by vnxcppcodegen

#include <mmx/package.hxx>
#include <mmx/pooling_stats_t.hxx>
#include <mmx/pooling_error_e.hxx>

#include <vnx/vnx.h>


namespace mmx {


const vnx::Hash64 pooling_stats_t::VNX_TYPE_HASH(0xb2441a254359df11ull);
const vnx::Hash64 pooling_stats_t::VNX_CODE_HASH(0xc4b1a2cb298c781cull);

vnx::Hash64 pooling_stats_t::get_type_hash() const {
	return VNX_TYPE_HASH;
}

std::string pooling_stats_t::get_type_name() const {
	return "mmx.pooling_stats_t";
}

const vnx::TypeCode* pooling_stats_t::get_type_code() const {
	return mmx::vnx_native_type_code_pooling_stats_t;
}

std::shared_ptr<pooling_stats_t> pooling_stats_t::create() {
	return std::make_shared<pooling_stats_t>();
}

std::shared_ptr<pooling_stats_t> pooling_stats_t::clone() const {
	return std::make_shared<pooling_stats_t>(*this);
}

void pooling_stats_t::read(vnx::TypeInput& _in, const vnx::TypeCode* _type_code, const uint16_t* _code) {
	vnx::read(_in, *this, _type_code, _code);
}

void pooling_stats_t::write(vnx::TypeOutput& _out, const vnx::TypeCode* _type_code, const uint16_t* _code) const {
	vnx::write(_out, *this, _type_code, _code);
}

void pooling_stats_t::accept(vnx::Visitor& _visitor) const {
	const vnx::TypeCode* _type_code = mmx::vnx_native_type_code_pooling_stats_t;
	_visitor.type_begin(*_type_code);
	_visitor.type_field(_type_code->fields[0], 0); vnx::accept(_visitor, server_url);
	_visitor.type_field(_type_code->fields[1], 1); vnx::accept(_visitor, partial_diff);
	_visitor.type_field(_type_code->fields[2], 2); vnx::accept(_visitor, valid_points);
	_visitor.type_field(_type_code->fields[3], 3); vnx::accept(_visitor, failed_points);
	_visitor.type_field(_type_code->fields[4], 4); vnx::accept(_visitor, error_count);
	_visitor.type_end(*_type_code);
}

void pooling_stats_t::write(std::ostream& _out) const {
	_out << "{";
	_out << "\"server_url\": "; vnx::write(_out, server_url);
	_out << ", \"partial_diff\": "; vnx::write(_out, partial_diff);
	_out << ", \"valid_points\": "; vnx::write(_out, valid_points);
	_out << ", \"failed_points\": "; vnx::write(_out, failed_points);
	_out << ", \"error_count\": "; vnx::write(_out, error_count);
	_out << "}";
}

void pooling_stats_t::read(std::istream& _in) {
	if(auto _json = vnx::read_json(_in)) {
		from_object(_json->to_object());
	}
}

vnx::Object pooling_stats_t::to_object() const {
	vnx::Object _object;
	_object["__type"] = "mmx.pooling_stats_t";
	_object["server_url"] = server_url;
	_object["partial_diff"] = partial_diff;
	_object["valid_points"] = valid_points;
	_object["failed_points"] = failed_points;
	_object["error_count"] = error_count;
	return _object;
}

void pooling_stats_t::from_object(const vnx::Object& _object) {
	for(const auto& _entry : _object.field) {
		if(_entry.first == "error_count") {
			_entry.second.to(error_count);
		} else if(_entry.first == "failed_points") {
			_entry.second.to(failed_points);
		} else if(_entry.first == "partial_diff") {
			_entry.second.to(partial_diff);
		} else if(_entry.first == "server_url") {
			_entry.second.to(server_url);
		} else if(_entry.first == "valid_points") {
			_entry.second.to(valid_points);
		}
	}
}

vnx::Variant pooling_stats_t::get_field(const std::string& _name) const {
	if(_name == "server_url") {
		return vnx::Variant(server_url);
	}
	if(_name == "partial_diff") {
		return vnx::Variant(partial_diff);
	}
	if(_name == "valid_points") {
		return vnx::Variant(valid_points);
	}
	if(_name == "failed_points") {
		return vnx::Variant(failed_points);
	}
	if(_name == "error_count") {
		return vnx::Variant(error_count);
	}
	return vnx::Variant();
}

void pooling_stats_t::set_field(const std::string& _name, const vnx::Variant& _value) {
	if(_name == "server_url") {
		_value.to(server_url);
	} else if(_name == "partial_diff") {
		_value.to(partial_diff);
	} else if(_name == "valid_points") {
		_value.to(valid_points);
	} else if(_name == "failed_points") {
		_value.to(failed_points);
	} else if(_name == "error_count") {
		_value.to(error_count);
	}
}

/// \private
std::ostream& operator<<(std::ostream& _out, const pooling_stats_t& _value) {
	_value.write(_out);
	return _out;
}

/// \private
std::istream& operator>>(std::istream& _in, pooling_stats_t& _value) {
	_value.read(_in);
	return _in;
}

const vnx::TypeCode* pooling_stats_t::static_get_type_code() {
	const vnx::TypeCode* type_code = vnx::get_type_code(VNX_TYPE_HASH);
	if(!type_code) {
		type_code = vnx::register_type_code(static_create_type_code());
	}
	return type_code;
}

std::shared_ptr<vnx::TypeCode> pooling_stats_t::static_create_type_code() {
	auto type_code = std::make_shared<vnx::TypeCode>();
	type_code->name = "mmx.pooling_stats_t";
	type_code->type_hash = vnx::Hash64(0xb2441a254359df11ull);
	type_code->code_hash = vnx::Hash64(0xc4b1a2cb298c781cull);
	type_code->is_native = true;
	type_code->native_size = sizeof(::mmx::pooling_stats_t);
	type_code->create_value = []() -> std::shared_ptr<vnx::Value> { return std::make_shared<vnx::Struct<pooling_stats_t>>(); };
	type_code->depends.resize(1);
	type_code->depends[0] = ::mmx::pooling_error_e::static_get_type_code();
	type_code->fields.resize(5);
	{
		auto& field = type_code->fields[0];
		field.is_extended = true;
		field.name = "server_url";
		field.code = {32};
	}
	{
		auto& field = type_code->fields[1];
		field.data_size = 8;
		field.name = "partial_diff";
		field.code = {4};
	}
	{
		auto& field = type_code->fields[2];
		field.data_size = 8;
		field.name = "valid_points";
		field.code = {4};
	}
	{
		auto& field = type_code->fields[3];
		field.data_size = 8;
		field.name = "failed_points";
		field.code = {4};
	}
	{
		auto& field = type_code->fields[4];
		field.is_extended = true;
		field.name = "error_count";
		field.code = {13, 4, 19, 0, 3};
	}
	type_code->build();
	return type_code;
}


} // namespace mmx


namespace vnx {

void read(TypeInput& in, ::mmx::pooling_stats_t& value, const TypeCode* type_code, const uint16_t* code) {
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
		if(const auto* const _field = type_code->field_map[1]) {
			vnx::read_value(_buf + _field->offset, value.partial_diff, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[2]) {
			vnx::read_value(_buf + _field->offset, value.valid_points, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[3]) {
			vnx::read_value(_buf + _field->offset, value.failed_points, _field->code.data());
		}
	}
	for(const auto* _field : type_code->ext_fields) {
		switch(_field->native_index) {
			case 0: vnx::read(in, value.server_url, type_code, _field->code.data()); break;
			case 4: vnx::read(in, value.error_count, type_code, _field->code.data()); break;
			default: vnx::skip(in, type_code, _field->code.data());
		}
	}
}

void write(TypeOutput& out, const ::mmx::pooling_stats_t& value, const TypeCode* type_code, const uint16_t* code) {
	if(code && code[0] == CODE_OBJECT) {
		vnx::write(out, value.to_object(), nullptr, code);
		return;
	}
	if(!type_code || (code && code[0] == CODE_ANY)) {
		type_code = mmx::vnx_native_type_code_pooling_stats_t;
		out.write_type_code(type_code);
		vnx::write_class_header<::mmx::pooling_stats_t>(out);
	}
	else if(code && code[0] == CODE_STRUCT) {
		type_code = type_code->depends[code[1]];
	}
	auto* const _buf = out.write(24);
	vnx::write_value(_buf + 0, value.partial_diff);
	vnx::write_value(_buf + 8, value.valid_points);
	vnx::write_value(_buf + 16, value.failed_points);
	vnx::write(out, value.server_url, type_code, type_code->fields[0].code.data());
	vnx::write(out, value.error_count, type_code, type_code->fields[4].code.data());
}

void read(std::istream& in, ::mmx::pooling_stats_t& value) {
	value.read(in);
}

void write(std::ostream& out, const ::mmx::pooling_stats_t& value) {
	value.write(out);
}

void accept(Visitor& visitor, const ::mmx::pooling_stats_t& value) {
	value.accept(visitor);
}

bool is_equivalent<::mmx::pooling_stats_t>::operator()(const uint16_t* code, const TypeCode* type_code) {
	if(code[0] != CODE_STRUCT || !type_code) {
		return false;
	}
	type_code = type_code->depends[code[1]];
	return type_code->type_hash == ::mmx::pooling_stats_t::VNX_TYPE_HASH && type_code->is_equivalent;
}

} // vnx
