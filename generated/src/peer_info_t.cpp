
// AUTO GENERATED by vnxcppcodegen

#include <mmx/package.hxx>
#include <mmx/peer_info_t.hxx>
#include <mmx/node_type_e.hxx>

#include <vnx/vnx.h>


namespace mmx {


const vnx::Hash64 peer_info_t::VNX_TYPE_HASH(0xce0ff32e89625afbull);
const vnx::Hash64 peer_info_t::VNX_CODE_HASH(0xd6ab74a4c33a8685ull);

vnx::Hash64 peer_info_t::get_type_hash() const {
	return VNX_TYPE_HASH;
}

std::string peer_info_t::get_type_name() const {
	return "mmx.peer_info_t";
}

const vnx::TypeCode* peer_info_t::get_type_code() const {
	return mmx::vnx_native_type_code_peer_info_t;
}

std::shared_ptr<peer_info_t> peer_info_t::create() {
	return std::make_shared<peer_info_t>();
}

std::shared_ptr<peer_info_t> peer_info_t::clone() const {
	return std::make_shared<peer_info_t>(*this);
}

void peer_info_t::read(vnx::TypeInput& _in, const vnx::TypeCode* _type_code, const uint16_t* _code) {
	vnx::read(_in, *this, _type_code, _code);
}

void peer_info_t::write(vnx::TypeOutput& _out, const vnx::TypeCode* _type_code, const uint16_t* _code) const {
	vnx::write(_out, *this, _type_code, _code);
}

void peer_info_t::accept(vnx::Visitor& _visitor) const {
	const vnx::TypeCode* _type_code = mmx::vnx_native_type_code_peer_info_t;
	_visitor.type_begin(*_type_code);
	_visitor.type_field(_type_code->fields[0], 0); vnx::accept(_visitor, id);
	_visitor.type_field(_type_code->fields[1], 1); vnx::accept(_visitor, address);
	_visitor.type_field(_type_code->fields[2], 2); vnx::accept(_visitor, type);
	_visitor.type_field(_type_code->fields[3], 3); vnx::accept(_visitor, ping_ms);
	_visitor.type_field(_type_code->fields[4], 4); vnx::accept(_visitor, height);
	_visitor.type_field(_type_code->fields[5], 5); vnx::accept(_visitor, version);
	_visitor.type_field(_type_code->fields[6], 6); vnx::accept(_visitor, recv_timeout_ms);
	_visitor.type_field(_type_code->fields[7], 7); vnx::accept(_visitor, connect_time_ms);
	_visitor.type_field(_type_code->fields[8], 8); vnx::accept(_visitor, bytes_send);
	_visitor.type_field(_type_code->fields[9], 9); vnx::accept(_visitor, bytes_recv);
	_visitor.type_field(_type_code->fields[10], 10); vnx::accept(_visitor, pending_cost);
	_visitor.type_field(_type_code->fields[11], 11); vnx::accept(_visitor, compression_ratio);
	_visitor.type_field(_type_code->fields[12], 12); vnx::accept(_visitor, is_synced);
	_visitor.type_field(_type_code->fields[13], 13); vnx::accept(_visitor, is_paused);
	_visitor.type_field(_type_code->fields[14], 14); vnx::accept(_visitor, is_blocked);
	_visitor.type_field(_type_code->fields[15], 15); vnx::accept(_visitor, is_outbound);
	_visitor.type_end(*_type_code);
}

void peer_info_t::write(std::ostream& _out) const {
	_out << "{";
	_out << "\"id\": "; vnx::write(_out, id);
	_out << ", \"address\": "; vnx::write(_out, address);
	_out << ", \"type\": "; vnx::write(_out, type);
	_out << ", \"ping_ms\": "; vnx::write(_out, ping_ms);
	_out << ", \"height\": "; vnx::write(_out, height);
	_out << ", \"version\": "; vnx::write(_out, version);
	_out << ", \"recv_timeout_ms\": "; vnx::write(_out, recv_timeout_ms);
	_out << ", \"connect_time_ms\": "; vnx::write(_out, connect_time_ms);
	_out << ", \"bytes_send\": "; vnx::write(_out, bytes_send);
	_out << ", \"bytes_recv\": "; vnx::write(_out, bytes_recv);
	_out << ", \"pending_cost\": "; vnx::write(_out, pending_cost);
	_out << ", \"compression_ratio\": "; vnx::write(_out, compression_ratio);
	_out << ", \"is_synced\": "; vnx::write(_out, is_synced);
	_out << ", \"is_paused\": "; vnx::write(_out, is_paused);
	_out << ", \"is_blocked\": "; vnx::write(_out, is_blocked);
	_out << ", \"is_outbound\": "; vnx::write(_out, is_outbound);
	_out << "}";
}

void peer_info_t::read(std::istream& _in) {
	if(auto _json = vnx::read_json(_in)) {
		from_object(_json->to_object());
	}
}

vnx::Object peer_info_t::to_object() const {
	vnx::Object _object;
	_object["__type"] = "mmx.peer_info_t";
	_object["id"] = id;
	_object["address"] = address;
	_object["type"] = type;
	_object["ping_ms"] = ping_ms;
	_object["height"] = height;
	_object["version"] = version;
	_object["recv_timeout_ms"] = recv_timeout_ms;
	_object["connect_time_ms"] = connect_time_ms;
	_object["bytes_send"] = bytes_send;
	_object["bytes_recv"] = bytes_recv;
	_object["pending_cost"] = pending_cost;
	_object["compression_ratio"] = compression_ratio;
	_object["is_synced"] = is_synced;
	_object["is_paused"] = is_paused;
	_object["is_blocked"] = is_blocked;
	_object["is_outbound"] = is_outbound;
	return _object;
}

void peer_info_t::from_object(const vnx::Object& _object) {
	for(const auto& _entry : _object.field) {
		if(_entry.first == "address") {
			_entry.second.to(address);
		} else if(_entry.first == "bytes_recv") {
			_entry.second.to(bytes_recv);
		} else if(_entry.first == "bytes_send") {
			_entry.second.to(bytes_send);
		} else if(_entry.first == "compression_ratio") {
			_entry.second.to(compression_ratio);
		} else if(_entry.first == "connect_time_ms") {
			_entry.second.to(connect_time_ms);
		} else if(_entry.first == "height") {
			_entry.second.to(height);
		} else if(_entry.first == "id") {
			_entry.second.to(id);
		} else if(_entry.first == "is_blocked") {
			_entry.second.to(is_blocked);
		} else if(_entry.first == "is_outbound") {
			_entry.second.to(is_outbound);
		} else if(_entry.first == "is_paused") {
			_entry.second.to(is_paused);
		} else if(_entry.first == "is_synced") {
			_entry.second.to(is_synced);
		} else if(_entry.first == "pending_cost") {
			_entry.second.to(pending_cost);
		} else if(_entry.first == "ping_ms") {
			_entry.second.to(ping_ms);
		} else if(_entry.first == "recv_timeout_ms") {
			_entry.second.to(recv_timeout_ms);
		} else if(_entry.first == "type") {
			_entry.second.to(type);
		} else if(_entry.first == "version") {
			_entry.second.to(version);
		}
	}
}

vnx::Variant peer_info_t::get_field(const std::string& _name) const {
	if(_name == "id") {
		return vnx::Variant(id);
	}
	if(_name == "address") {
		return vnx::Variant(address);
	}
	if(_name == "type") {
		return vnx::Variant(type);
	}
	if(_name == "ping_ms") {
		return vnx::Variant(ping_ms);
	}
	if(_name == "height") {
		return vnx::Variant(height);
	}
	if(_name == "version") {
		return vnx::Variant(version);
	}
	if(_name == "recv_timeout_ms") {
		return vnx::Variant(recv_timeout_ms);
	}
	if(_name == "connect_time_ms") {
		return vnx::Variant(connect_time_ms);
	}
	if(_name == "bytes_send") {
		return vnx::Variant(bytes_send);
	}
	if(_name == "bytes_recv") {
		return vnx::Variant(bytes_recv);
	}
	if(_name == "pending_cost") {
		return vnx::Variant(pending_cost);
	}
	if(_name == "compression_ratio") {
		return vnx::Variant(compression_ratio);
	}
	if(_name == "is_synced") {
		return vnx::Variant(is_synced);
	}
	if(_name == "is_paused") {
		return vnx::Variant(is_paused);
	}
	if(_name == "is_blocked") {
		return vnx::Variant(is_blocked);
	}
	if(_name == "is_outbound") {
		return vnx::Variant(is_outbound);
	}
	return vnx::Variant();
}

void peer_info_t::set_field(const std::string& _name, const vnx::Variant& _value) {
	if(_name == "id") {
		_value.to(id);
	} else if(_name == "address") {
		_value.to(address);
	} else if(_name == "type") {
		_value.to(type);
	} else if(_name == "ping_ms") {
		_value.to(ping_ms);
	} else if(_name == "height") {
		_value.to(height);
	} else if(_name == "version") {
		_value.to(version);
	} else if(_name == "recv_timeout_ms") {
		_value.to(recv_timeout_ms);
	} else if(_name == "connect_time_ms") {
		_value.to(connect_time_ms);
	} else if(_name == "bytes_send") {
		_value.to(bytes_send);
	} else if(_name == "bytes_recv") {
		_value.to(bytes_recv);
	} else if(_name == "pending_cost") {
		_value.to(pending_cost);
	} else if(_name == "compression_ratio") {
		_value.to(compression_ratio);
	} else if(_name == "is_synced") {
		_value.to(is_synced);
	} else if(_name == "is_paused") {
		_value.to(is_paused);
	} else if(_name == "is_blocked") {
		_value.to(is_blocked);
	} else if(_name == "is_outbound") {
		_value.to(is_outbound);
	}
}

/// \private
std::ostream& operator<<(std::ostream& _out, const peer_info_t& _value) {
	_value.write(_out);
	return _out;
}

/// \private
std::istream& operator>>(std::istream& _in, peer_info_t& _value) {
	_value.read(_in);
	return _in;
}

const vnx::TypeCode* peer_info_t::static_get_type_code() {
	const vnx::TypeCode* type_code = vnx::get_type_code(VNX_TYPE_HASH);
	if(!type_code) {
		type_code = vnx::register_type_code(static_create_type_code());
	}
	return type_code;
}

std::shared_ptr<vnx::TypeCode> peer_info_t::static_create_type_code() {
	auto type_code = std::make_shared<vnx::TypeCode>();
	type_code->name = "mmx.peer_info_t";
	type_code->type_hash = vnx::Hash64(0xce0ff32e89625afbull);
	type_code->code_hash = vnx::Hash64(0xd6ab74a4c33a8685ull);
	type_code->is_native = true;
	type_code->native_size = sizeof(::mmx::peer_info_t);
	type_code->create_value = []() -> std::shared_ptr<vnx::Value> { return std::make_shared<vnx::Struct<peer_info_t>>(); };
	type_code->depends.resize(1);
	type_code->depends[0] = ::mmx::node_type_e::static_get_type_code();
	type_code->fields.resize(16);
	{
		auto& field = type_code->fields[0];
		field.data_size = 8;
		field.name = "id";
		field.code = {4};
	}
	{
		auto& field = type_code->fields[1];
		field.is_extended = true;
		field.name = "address";
		field.code = {32};
	}
	{
		auto& field = type_code->fields[2];
		field.is_extended = true;
		field.name = "type";
		field.code = {19, 0};
	}
	{
		auto& field = type_code->fields[3];
		field.data_size = 4;
		field.name = "ping_ms";
		field.code = {7};
	}
	{
		auto& field = type_code->fields[4];
		field.data_size = 4;
		field.name = "height";
		field.code = {3};
	}
	{
		auto& field = type_code->fields[5];
		field.data_size = 4;
		field.name = "version";
		field.code = {3};
	}
	{
		auto& field = type_code->fields[6];
		field.data_size = 8;
		field.name = "recv_timeout_ms";
		field.code = {8};
	}
	{
		auto& field = type_code->fields[7];
		field.data_size = 8;
		field.name = "connect_time_ms";
		field.code = {8};
	}
	{
		auto& field = type_code->fields[8];
		field.data_size = 8;
		field.name = "bytes_send";
		field.code = {4};
	}
	{
		auto& field = type_code->fields[9];
		field.data_size = 8;
		field.name = "bytes_recv";
		field.code = {4};
	}
	{
		auto& field = type_code->fields[10];
		field.data_size = 8;
		field.name = "pending_cost";
		field.code = {10};
	}
	{
		auto& field = type_code->fields[11];
		field.data_size = 8;
		field.name = "compression_ratio";
		field.code = {10};
	}
	{
		auto& field = type_code->fields[12];
		field.data_size = 1;
		field.name = "is_synced";
		field.code = {31};
	}
	{
		auto& field = type_code->fields[13];
		field.data_size = 1;
		field.name = "is_paused";
		field.code = {31};
	}
	{
		auto& field = type_code->fields[14];
		field.data_size = 1;
		field.name = "is_blocked";
		field.code = {31};
	}
	{
		auto& field = type_code->fields[15];
		field.data_size = 1;
		field.name = "is_outbound";
		field.code = {31};
	}
	type_code->build();
	return type_code;
}


} // namespace mmx


namespace vnx {

void read(TypeInput& in, ::mmx::peer_info_t& value, const TypeCode* type_code, const uint16_t* code) {
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
			vnx::read_value(_buf + _field->offset, value.id, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[3]) {
			vnx::read_value(_buf + _field->offset, value.ping_ms, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[4]) {
			vnx::read_value(_buf + _field->offset, value.height, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[5]) {
			vnx::read_value(_buf + _field->offset, value.version, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[6]) {
			vnx::read_value(_buf + _field->offset, value.recv_timeout_ms, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[7]) {
			vnx::read_value(_buf + _field->offset, value.connect_time_ms, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[8]) {
			vnx::read_value(_buf + _field->offset, value.bytes_send, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[9]) {
			vnx::read_value(_buf + _field->offset, value.bytes_recv, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[10]) {
			vnx::read_value(_buf + _field->offset, value.pending_cost, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[11]) {
			vnx::read_value(_buf + _field->offset, value.compression_ratio, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[12]) {
			vnx::read_value(_buf + _field->offset, value.is_synced, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[13]) {
			vnx::read_value(_buf + _field->offset, value.is_paused, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[14]) {
			vnx::read_value(_buf + _field->offset, value.is_blocked, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[15]) {
			vnx::read_value(_buf + _field->offset, value.is_outbound, _field->code.data());
		}
	}
	for(const auto* _field : type_code->ext_fields) {
		switch(_field->native_index) {
			case 1: vnx::read(in, value.address, type_code, _field->code.data()); break;
			case 2: vnx::read(in, value.type, type_code, _field->code.data()); break;
			default: vnx::skip(in, type_code, _field->code.data());
		}
	}
}

void write(TypeOutput& out, const ::mmx::peer_info_t& value, const TypeCode* type_code, const uint16_t* code) {
	if(code && code[0] == CODE_OBJECT) {
		vnx::write(out, value.to_object(), nullptr, code);
		return;
	}
	if(!type_code || (code && code[0] == CODE_ANY)) {
		type_code = mmx::vnx_native_type_code_peer_info_t;
		out.write_type_code(type_code);
		vnx::write_class_header<::mmx::peer_info_t>(out);
	}
	else if(code && code[0] == CODE_STRUCT) {
		type_code = type_code->depends[code[1]];
	}
	auto* const _buf = out.write(72);
	vnx::write_value(_buf + 0, value.id);
	vnx::write_value(_buf + 8, value.ping_ms);
	vnx::write_value(_buf + 12, value.height);
	vnx::write_value(_buf + 16, value.version);
	vnx::write_value(_buf + 20, value.recv_timeout_ms);
	vnx::write_value(_buf + 28, value.connect_time_ms);
	vnx::write_value(_buf + 36, value.bytes_send);
	vnx::write_value(_buf + 44, value.bytes_recv);
	vnx::write_value(_buf + 52, value.pending_cost);
	vnx::write_value(_buf + 60, value.compression_ratio);
	vnx::write_value(_buf + 68, value.is_synced);
	vnx::write_value(_buf + 69, value.is_paused);
	vnx::write_value(_buf + 70, value.is_blocked);
	vnx::write_value(_buf + 71, value.is_outbound);
	vnx::write(out, value.address, type_code, type_code->fields[1].code.data());
	vnx::write(out, value.type, type_code, type_code->fields[2].code.data());
}

void read(std::istream& in, ::mmx::peer_info_t& value) {
	value.read(in);
}

void write(std::ostream& out, const ::mmx::peer_info_t& value) {
	value.write(out);
}

void accept(Visitor& visitor, const ::mmx::peer_info_t& value) {
	value.accept(visitor);
}

bool is_equivalent<::mmx::peer_info_t>::operator()(const uint16_t* code, const TypeCode* type_code) {
	if(code[0] != CODE_STRUCT || !type_code) {
		return false;
	}
	type_code = type_code->depends[code[1]];
	return type_code->type_hash == ::mmx::peer_info_t::VNX_TYPE_HASH && type_code->is_equivalent;
}

} // vnx
