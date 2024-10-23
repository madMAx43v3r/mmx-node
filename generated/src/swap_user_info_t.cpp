
// AUTO GENERATED by vnxcppcodegen

#include <mmx/package.hxx>
#include <mmx/swap_user_info_t.hxx>
#include <mmx/uint128.hpp>

#include <vnx/vnx.h>


namespace mmx {


const vnx::Hash64 swap_user_info_t::VNX_TYPE_HASH(0x1b6c720bff2d638cull);
const vnx::Hash64 swap_user_info_t::VNX_CODE_HASH(0x996df91e34c838d2ull);

vnx::Hash64 swap_user_info_t::get_type_hash() const {
	return VNX_TYPE_HASH;
}

std::string swap_user_info_t::get_type_name() const {
	return "mmx.swap_user_info_t";
}

const vnx::TypeCode* swap_user_info_t::get_type_code() const {
	return mmx::vnx_native_type_code_swap_user_info_t;
}

std::shared_ptr<swap_user_info_t> swap_user_info_t::create() {
	return std::make_shared<swap_user_info_t>();
}

std::shared_ptr<swap_user_info_t> swap_user_info_t::clone() const {
	return std::make_shared<swap_user_info_t>(*this);
}

void swap_user_info_t::read(vnx::TypeInput& _in, const vnx::TypeCode* _type_code, const uint16_t* _code) {
	vnx::read(_in, *this, _type_code, _code);
}

void swap_user_info_t::write(vnx::TypeOutput& _out, const vnx::TypeCode* _type_code, const uint16_t* _code) const {
	vnx::write(_out, *this, _type_code, _code);
}

void swap_user_info_t::accept(vnx::Visitor& _visitor) const {
	const vnx::TypeCode* _type_code = mmx::vnx_native_type_code_swap_user_info_t;
	_visitor.type_begin(*_type_code);
	_visitor.type_field(_type_code->fields[0], 0); vnx::accept(_visitor, pool_idx);
	_visitor.type_field(_type_code->fields[1], 1); vnx::accept(_visitor, balance);
	_visitor.type_field(_type_code->fields[2], 2); vnx::accept(_visitor, last_user_total);
	_visitor.type_field(_type_code->fields[3], 3); vnx::accept(_visitor, last_fees_paid);
	_visitor.type_field(_type_code->fields[4], 4); vnx::accept(_visitor, fees_earned);
	_visitor.type_field(_type_code->fields[5], 5); vnx::accept(_visitor, equivalent_liquidity);
	_visitor.type_field(_type_code->fields[6], 6); vnx::accept(_visitor, unlock_height);
	_visitor.type_end(*_type_code);
}

void swap_user_info_t::write(std::ostream& _out) const {
	_out << "{";
	_out << "\"pool_idx\": "; vnx::write(_out, pool_idx);
	_out << ", \"balance\": "; vnx::write(_out, balance);
	_out << ", \"last_user_total\": "; vnx::write(_out, last_user_total);
	_out << ", \"last_fees_paid\": "; vnx::write(_out, last_fees_paid);
	_out << ", \"fees_earned\": "; vnx::write(_out, fees_earned);
	_out << ", \"equivalent_liquidity\": "; vnx::write(_out, equivalent_liquidity);
	_out << ", \"unlock_height\": "; vnx::write(_out, unlock_height);
	_out << "}";
}

void swap_user_info_t::read(std::istream& _in) {
	if(auto _json = vnx::read_json(_in)) {
		from_object(_json->to_object());
	}
}

vnx::Object swap_user_info_t::to_object() const {
	vnx::Object _object;
	_object["__type"] = "mmx.swap_user_info_t";
	_object["pool_idx"] = pool_idx;
	_object["balance"] = balance;
	_object["last_user_total"] = last_user_total;
	_object["last_fees_paid"] = last_fees_paid;
	_object["fees_earned"] = fees_earned;
	_object["equivalent_liquidity"] = equivalent_liquidity;
	_object["unlock_height"] = unlock_height;
	return _object;
}

void swap_user_info_t::from_object(const vnx::Object& _object) {
	for(const auto& _entry : _object.field) {
		if(_entry.first == "balance") {
			_entry.second.to(balance);
		} else if(_entry.first == "equivalent_liquidity") {
			_entry.second.to(equivalent_liquidity);
		} else if(_entry.first == "fees_earned") {
			_entry.second.to(fees_earned);
		} else if(_entry.first == "last_fees_paid") {
			_entry.second.to(last_fees_paid);
		} else if(_entry.first == "last_user_total") {
			_entry.second.to(last_user_total);
		} else if(_entry.first == "pool_idx") {
			_entry.second.to(pool_idx);
		} else if(_entry.first == "unlock_height") {
			_entry.second.to(unlock_height);
		}
	}
}

vnx::Variant swap_user_info_t::get_field(const std::string& _name) const {
	if(_name == "pool_idx") {
		return vnx::Variant(pool_idx);
	}
	if(_name == "balance") {
		return vnx::Variant(balance);
	}
	if(_name == "last_user_total") {
		return vnx::Variant(last_user_total);
	}
	if(_name == "last_fees_paid") {
		return vnx::Variant(last_fees_paid);
	}
	if(_name == "fees_earned") {
		return vnx::Variant(fees_earned);
	}
	if(_name == "equivalent_liquidity") {
		return vnx::Variant(equivalent_liquidity);
	}
	if(_name == "unlock_height") {
		return vnx::Variant(unlock_height);
	}
	return vnx::Variant();
}

void swap_user_info_t::set_field(const std::string& _name, const vnx::Variant& _value) {
	if(_name == "pool_idx") {
		_value.to(pool_idx);
	} else if(_name == "balance") {
		_value.to(balance);
	} else if(_name == "last_user_total") {
		_value.to(last_user_total);
	} else if(_name == "last_fees_paid") {
		_value.to(last_fees_paid);
	} else if(_name == "fees_earned") {
		_value.to(fees_earned);
	} else if(_name == "equivalent_liquidity") {
		_value.to(equivalent_liquidity);
	} else if(_name == "unlock_height") {
		_value.to(unlock_height);
	}
}

/// \private
std::ostream& operator<<(std::ostream& _out, const swap_user_info_t& _value) {
	_value.write(_out);
	return _out;
}

/// \private
std::istream& operator>>(std::istream& _in, swap_user_info_t& _value) {
	_value.read(_in);
	return _in;
}

const vnx::TypeCode* swap_user_info_t::static_get_type_code() {
	const vnx::TypeCode* type_code = vnx::get_type_code(VNX_TYPE_HASH);
	if(!type_code) {
		type_code = vnx::register_type_code(static_create_type_code());
	}
	return type_code;
}

std::shared_ptr<vnx::TypeCode> swap_user_info_t::static_create_type_code() {
	auto type_code = std::make_shared<vnx::TypeCode>();
	type_code->name = "mmx.swap_user_info_t";
	type_code->type_hash = vnx::Hash64(0x1b6c720bff2d638cull);
	type_code->code_hash = vnx::Hash64(0x996df91e34c838d2ull);
	type_code->is_native = true;
	type_code->native_size = sizeof(::mmx::swap_user_info_t);
	type_code->create_value = []() -> std::shared_ptr<vnx::Value> { return std::make_shared<vnx::Struct<swap_user_info_t>>(); };
	type_code->fields.resize(7);
	{
		auto& field = type_code->fields[0];
		field.data_size = 4;
		field.name = "pool_idx";
		field.value = vnx::to_string(-1);
		field.code = {7};
	}
	{
		auto& field = type_code->fields[1];
		field.is_extended = true;
		field.name = "balance";
		field.code = {11, 2, 11, 16, 1};
	}
	{
		auto& field = type_code->fields[2];
		field.is_extended = true;
		field.name = "last_user_total";
		field.code = {11, 2, 11, 16, 1};
	}
	{
		auto& field = type_code->fields[3];
		field.is_extended = true;
		field.name = "last_fees_paid";
		field.code = {11, 2, 11, 16, 1};
	}
	{
		auto& field = type_code->fields[4];
		field.is_extended = true;
		field.name = "fees_earned";
		field.code = {11, 2, 11, 16, 1};
	}
	{
		auto& field = type_code->fields[5];
		field.is_extended = true;
		field.name = "equivalent_liquidity";
		field.code = {11, 2, 11, 16, 1};
	}
	{
		auto& field = type_code->fields[6];
		field.data_size = 4;
		field.name = "unlock_height";
		field.code = {3};
	}
	type_code->build();
	return type_code;
}


} // namespace mmx


namespace vnx {

void read(TypeInput& in, ::mmx::swap_user_info_t& value, const TypeCode* type_code, const uint16_t* code) {
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
			vnx::read_value(_buf + _field->offset, value.pool_idx, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[6]) {
			vnx::read_value(_buf + _field->offset, value.unlock_height, _field->code.data());
		}
	}
	for(const auto* _field : type_code->ext_fields) {
		switch(_field->native_index) {
			case 1: vnx::read(in, value.balance, type_code, _field->code.data()); break;
			case 2: vnx::read(in, value.last_user_total, type_code, _field->code.data()); break;
			case 3: vnx::read(in, value.last_fees_paid, type_code, _field->code.data()); break;
			case 4: vnx::read(in, value.fees_earned, type_code, _field->code.data()); break;
			case 5: vnx::read(in, value.equivalent_liquidity, type_code, _field->code.data()); break;
			default: vnx::skip(in, type_code, _field->code.data());
		}
	}
}

void write(TypeOutput& out, const ::mmx::swap_user_info_t& value, const TypeCode* type_code, const uint16_t* code) {
	if(code && code[0] == CODE_OBJECT) {
		vnx::write(out, value.to_object(), nullptr, code);
		return;
	}
	if(!type_code || (code && code[0] == CODE_ANY)) {
		type_code = mmx::vnx_native_type_code_swap_user_info_t;
		out.write_type_code(type_code);
		vnx::write_class_header<::mmx::swap_user_info_t>(out);
	}
	else if(code && code[0] == CODE_STRUCT) {
		type_code = type_code->depends[code[1]];
	}
	auto* const _buf = out.write(8);
	vnx::write_value(_buf + 0, value.pool_idx);
	vnx::write_value(_buf + 4, value.unlock_height);
	vnx::write(out, value.balance, type_code, type_code->fields[1].code.data());
	vnx::write(out, value.last_user_total, type_code, type_code->fields[2].code.data());
	vnx::write(out, value.last_fees_paid, type_code, type_code->fields[3].code.data());
	vnx::write(out, value.fees_earned, type_code, type_code->fields[4].code.data());
	vnx::write(out, value.equivalent_liquidity, type_code, type_code->fields[5].code.data());
}

void read(std::istream& in, ::mmx::swap_user_info_t& value) {
	value.read(in);
}

void write(std::ostream& out, const ::mmx::swap_user_info_t& value) {
	value.write(out);
}

void accept(Visitor& visitor, const ::mmx::swap_user_info_t& value) {
	value.accept(visitor);
}

bool is_equivalent<::mmx::swap_user_info_t>::operator()(const uint16_t* code, const TypeCode* type_code) {
	if(code[0] != CODE_STRUCT || !type_code) {
		return false;
	}
	type_code = type_code->depends[code[1]];
	return type_code->type_hash == ::mmx::swap_user_info_t::VNX_TYPE_HASH && type_code->is_equivalent;
}

} // vnx
