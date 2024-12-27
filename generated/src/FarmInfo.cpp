
// AUTO GENERATED by vnxcppcodegen

#include <mmx/package.hxx>
#include <mmx/FarmInfo.hxx>
#include <mmx/addr_t.hpp>
#include <mmx/hash_t.hpp>
#include <mmx/pooling_info_t.hxx>
#include <mmx/pooling_stats_t.hxx>
#include <vnx/Value.h>

#include <vnx/vnx.h>


namespace mmx {


const vnx::Hash64 FarmInfo::VNX_TYPE_HASH(0xa2701372b9137f0eull);
const vnx::Hash64 FarmInfo::VNX_CODE_HASH(0xd67b28eb2cb51c62ull);

vnx::Hash64 FarmInfo::get_type_hash() const {
	return VNX_TYPE_HASH;
}

std::string FarmInfo::get_type_name() const {
	return "mmx.FarmInfo";
}

const vnx::TypeCode* FarmInfo::get_type_code() const {
	return mmx::vnx_native_type_code_FarmInfo;
}

std::shared_ptr<FarmInfo> FarmInfo::create() {
	return std::make_shared<FarmInfo>();
}

std::shared_ptr<vnx::Value> FarmInfo::clone() const {
	return std::make_shared<FarmInfo>(*this);
}

void FarmInfo::read(vnx::TypeInput& _in, const vnx::TypeCode* _type_code, const uint16_t* _code) {
	vnx::read(_in, *this, _type_code, _code);
}

void FarmInfo::write(vnx::TypeOutput& _out, const vnx::TypeCode* _type_code, const uint16_t* _code) const {
	vnx::write(_out, *this, _type_code, _code);
}

void FarmInfo::accept(vnx::Visitor& _visitor) const {
	const vnx::TypeCode* _type_code = mmx::vnx_native_type_code_FarmInfo;
	_visitor.type_begin(*_type_code);
	_visitor.type_field(_type_code->fields[0], 0); vnx::accept(_visitor, plot_dirs);
	_visitor.type_field(_type_code->fields[1], 1); vnx::accept(_visitor, plot_count);
	_visitor.type_field(_type_code->fields[2], 2); vnx::accept(_visitor, harvester_bytes);
	_visitor.type_field(_type_code->fields[3], 3); vnx::accept(_visitor, pool_info);
	_visitor.type_field(_type_code->fields[4], 4); vnx::accept(_visitor, pool_stats);
	_visitor.type_field(_type_code->fields[5], 5); vnx::accept(_visitor, total_bytes);
	_visitor.type_field(_type_code->fields[6], 6); vnx::accept(_visitor, total_bytes_effective);
	_visitor.type_field(_type_code->fields[7], 7); vnx::accept(_visitor, total_balance);
	_visitor.type_field(_type_code->fields[8], 8); vnx::accept(_visitor, harvester);
	_visitor.type_field(_type_code->fields[9], 9); vnx::accept(_visitor, harvester_id);
	_visitor.type_field(_type_code->fields[10], 10); vnx::accept(_visitor, reward_addr);
	_visitor.type_end(*_type_code);
}

void FarmInfo::write(std::ostream& _out) const {
	_out << "{\"__type\": \"mmx.FarmInfo\"";
	_out << ", \"plot_dirs\": "; vnx::write(_out, plot_dirs);
	_out << ", \"plot_count\": "; vnx::write(_out, plot_count);
	_out << ", \"harvester_bytes\": "; vnx::write(_out, harvester_bytes);
	_out << ", \"pool_info\": "; vnx::write(_out, pool_info);
	_out << ", \"pool_stats\": "; vnx::write(_out, pool_stats);
	_out << ", \"total_bytes\": "; vnx::write(_out, total_bytes);
	_out << ", \"total_bytes_effective\": "; vnx::write(_out, total_bytes_effective);
	_out << ", \"total_balance\": "; vnx::write(_out, total_balance);
	_out << ", \"harvester\": "; vnx::write(_out, harvester);
	_out << ", \"harvester_id\": "; vnx::write(_out, harvester_id);
	_out << ", \"reward_addr\": "; vnx::write(_out, reward_addr);
	_out << "}";
}

void FarmInfo::read(std::istream& _in) {
	if(auto _json = vnx::read_json(_in)) {
		from_object(_json->to_object());
	}
}

vnx::Object FarmInfo::to_object() const {
	vnx::Object _object;
	_object["__type"] = "mmx.FarmInfo";
	_object["plot_dirs"] = plot_dirs;
	_object["plot_count"] = plot_count;
	_object["harvester_bytes"] = harvester_bytes;
	_object["pool_info"] = pool_info;
	_object["pool_stats"] = pool_stats;
	_object["total_bytes"] = total_bytes;
	_object["total_bytes_effective"] = total_bytes_effective;
	_object["total_balance"] = total_balance;
	_object["harvester"] = harvester;
	_object["harvester_id"] = harvester_id;
	_object["reward_addr"] = reward_addr;
	return _object;
}

void FarmInfo::from_object(const vnx::Object& _object) {
	for(const auto& _entry : _object.field) {
		if(_entry.first == "harvester") {
			_entry.second.to(harvester);
		} else if(_entry.first == "harvester_bytes") {
			_entry.second.to(harvester_bytes);
		} else if(_entry.first == "harvester_id") {
			_entry.second.to(harvester_id);
		} else if(_entry.first == "plot_count") {
			_entry.second.to(plot_count);
		} else if(_entry.first == "plot_dirs") {
			_entry.second.to(plot_dirs);
		} else if(_entry.first == "pool_info") {
			_entry.second.to(pool_info);
		} else if(_entry.first == "pool_stats") {
			_entry.second.to(pool_stats);
		} else if(_entry.first == "reward_addr") {
			_entry.second.to(reward_addr);
		} else if(_entry.first == "total_balance") {
			_entry.second.to(total_balance);
		} else if(_entry.first == "total_bytes") {
			_entry.second.to(total_bytes);
		} else if(_entry.first == "total_bytes_effective") {
			_entry.second.to(total_bytes_effective);
		}
	}
}

vnx::Variant FarmInfo::get_field(const std::string& _name) const {
	if(_name == "plot_dirs") {
		return vnx::Variant(plot_dirs);
	}
	if(_name == "plot_count") {
		return vnx::Variant(plot_count);
	}
	if(_name == "harvester_bytes") {
		return vnx::Variant(harvester_bytes);
	}
	if(_name == "pool_info") {
		return vnx::Variant(pool_info);
	}
	if(_name == "pool_stats") {
		return vnx::Variant(pool_stats);
	}
	if(_name == "total_bytes") {
		return vnx::Variant(total_bytes);
	}
	if(_name == "total_bytes_effective") {
		return vnx::Variant(total_bytes_effective);
	}
	if(_name == "total_balance") {
		return vnx::Variant(total_balance);
	}
	if(_name == "harvester") {
		return vnx::Variant(harvester);
	}
	if(_name == "harvester_id") {
		return vnx::Variant(harvester_id);
	}
	if(_name == "reward_addr") {
		return vnx::Variant(reward_addr);
	}
	return vnx::Variant();
}

void FarmInfo::set_field(const std::string& _name, const vnx::Variant& _value) {
	if(_name == "plot_dirs") {
		_value.to(plot_dirs);
	} else if(_name == "plot_count") {
		_value.to(plot_count);
	} else if(_name == "harvester_bytes") {
		_value.to(harvester_bytes);
	} else if(_name == "pool_info") {
		_value.to(pool_info);
	} else if(_name == "pool_stats") {
		_value.to(pool_stats);
	} else if(_name == "total_bytes") {
		_value.to(total_bytes);
	} else if(_name == "total_bytes_effective") {
		_value.to(total_bytes_effective);
	} else if(_name == "total_balance") {
		_value.to(total_balance);
	} else if(_name == "harvester") {
		_value.to(harvester);
	} else if(_name == "harvester_id") {
		_value.to(harvester_id);
	} else if(_name == "reward_addr") {
		_value.to(reward_addr);
	}
}

/// \private
std::ostream& operator<<(std::ostream& _out, const FarmInfo& _value) {
	_value.write(_out);
	return _out;
}

/// \private
std::istream& operator>>(std::istream& _in, FarmInfo& _value) {
	_value.read(_in);
	return _in;
}

const vnx::TypeCode* FarmInfo::static_get_type_code() {
	const vnx::TypeCode* type_code = vnx::get_type_code(VNX_TYPE_HASH);
	if(!type_code) {
		type_code = vnx::register_type_code(static_create_type_code());
	}
	return type_code;
}

std::shared_ptr<vnx::TypeCode> FarmInfo::static_create_type_code() {
	auto type_code = std::make_shared<vnx::TypeCode>();
	type_code->name = "mmx.FarmInfo";
	type_code->type_hash = vnx::Hash64(0xa2701372b9137f0eull);
	type_code->code_hash = vnx::Hash64(0xd67b28eb2cb51c62ull);
	type_code->is_native = true;
	type_code->is_class = true;
	type_code->native_size = sizeof(::mmx::FarmInfo);
	type_code->create_value = []() -> std::shared_ptr<vnx::Value> { return std::make_shared<FarmInfo>(); };
	type_code->depends.resize(2);
	type_code->depends[0] = ::mmx::pooling_info_t::static_get_type_code();
	type_code->depends[1] = ::mmx::pooling_stats_t::static_get_type_code();
	type_code->fields.resize(11);
	{
		auto& field = type_code->fields[0];
		field.is_extended = true;
		field.name = "plot_dirs";
		field.code = {12, 32};
	}
	{
		auto& field = type_code->fields[1];
		field.is_extended = true;
		field.name = "plot_count";
		field.code = {13, 3, 1, 3};
	}
	{
		auto& field = type_code->fields[2];
		field.is_extended = true;
		field.name = "harvester_bytes";
		field.code = {13, 3, 32, 23, 2, 4, 5, 4, 4};
	}
	{
		auto& field = type_code->fields[3];
		field.is_extended = true;
		field.name = "pool_info";
		field.code = {13, 5, 11, 32, 1, 19, 0};
	}
	{
		auto& field = type_code->fields[4];
		field.is_extended = true;
		field.name = "pool_stats";
		field.code = {13, 5, 11, 32, 1, 19, 1};
	}
	{
		auto& field = type_code->fields[5];
		field.data_size = 8;
		field.name = "total_bytes";
		field.code = {4};
	}
	{
		auto& field = type_code->fields[6];
		field.data_size = 8;
		field.name = "total_bytes_effective";
		field.code = {4};
	}
	{
		auto& field = type_code->fields[7];
		field.data_size = 8;
		field.name = "total_balance";
		field.code = {4};
	}
	{
		auto& field = type_code->fields[8];
		field.is_extended = true;
		field.name = "harvester";
		field.code = {33, 32};
	}
	{
		auto& field = type_code->fields[9];
		field.is_extended = true;
		field.name = "harvester_id";
		field.code = {33, 11, 32, 1};
	}
	{
		auto& field = type_code->fields[10];
		field.is_extended = true;
		field.name = "reward_addr";
		field.code = {33, 11, 32, 1};
	}
	type_code->build();
	return type_code;
}

std::shared_ptr<vnx::Value> FarmInfo::vnx_call_switch(std::shared_ptr<const vnx::Value> _method) {
	switch(_method->get_type_hash()) {
	}
	return nullptr;
}


} // namespace mmx


namespace vnx {

void read(TypeInput& in, ::mmx::FarmInfo& value, const TypeCode* type_code, const uint16_t* code) {
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
		if(const auto* const _field = type_code->field_map[5]) {
			vnx::read_value(_buf + _field->offset, value.total_bytes, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[6]) {
			vnx::read_value(_buf + _field->offset, value.total_bytes_effective, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[7]) {
			vnx::read_value(_buf + _field->offset, value.total_balance, _field->code.data());
		}
	}
	for(const auto* _field : type_code->ext_fields) {
		switch(_field->native_index) {
			case 0: vnx::read(in, value.plot_dirs, type_code, _field->code.data()); break;
			case 1: vnx::read(in, value.plot_count, type_code, _field->code.data()); break;
			case 2: vnx::read(in, value.harvester_bytes, type_code, _field->code.data()); break;
			case 3: vnx::read(in, value.pool_info, type_code, _field->code.data()); break;
			case 4: vnx::read(in, value.pool_stats, type_code, _field->code.data()); break;
			case 8: vnx::read(in, value.harvester, type_code, _field->code.data()); break;
			case 9: vnx::read(in, value.harvester_id, type_code, _field->code.data()); break;
			case 10: vnx::read(in, value.reward_addr, type_code, _field->code.data()); break;
			default: vnx::skip(in, type_code, _field->code.data());
		}
	}
}

void write(TypeOutput& out, const ::mmx::FarmInfo& value, const TypeCode* type_code, const uint16_t* code) {
	if(code && code[0] == CODE_OBJECT) {
		vnx::write(out, value.to_object(), nullptr, code);
		return;
	}
	if(!type_code || (code && code[0] == CODE_ANY)) {
		type_code = mmx::vnx_native_type_code_FarmInfo;
		out.write_type_code(type_code);
		vnx::write_class_header<::mmx::FarmInfo>(out);
	}
	else if(code && code[0] == CODE_STRUCT) {
		type_code = type_code->depends[code[1]];
	}
	auto* const _buf = out.write(24);
	vnx::write_value(_buf + 0, value.total_bytes);
	vnx::write_value(_buf + 8, value.total_bytes_effective);
	vnx::write_value(_buf + 16, value.total_balance);
	vnx::write(out, value.plot_dirs, type_code, type_code->fields[0].code.data());
	vnx::write(out, value.plot_count, type_code, type_code->fields[1].code.data());
	vnx::write(out, value.harvester_bytes, type_code, type_code->fields[2].code.data());
	vnx::write(out, value.pool_info, type_code, type_code->fields[3].code.data());
	vnx::write(out, value.pool_stats, type_code, type_code->fields[4].code.data());
	vnx::write(out, value.harvester, type_code, type_code->fields[8].code.data());
	vnx::write(out, value.harvester_id, type_code, type_code->fields[9].code.data());
	vnx::write(out, value.reward_addr, type_code, type_code->fields[10].code.data());
}

void read(std::istream& in, ::mmx::FarmInfo& value) {
	value.read(in);
}

void write(std::ostream& out, const ::mmx::FarmInfo& value) {
	value.write(out);
}

void accept(Visitor& visitor, const ::mmx::FarmInfo& value) {
	value.accept(visitor);
}

} // vnx
