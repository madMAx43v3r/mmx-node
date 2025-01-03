
// AUTO GENERATED by vnxcppcodegen

#include <mmx/package.hxx>
#include <mmx/ProofOfStake.hxx>
#include <mmx/ProofOfSpace.hxx>
#include <mmx/hash_t.hpp>
#include <mmx/signature_t.hpp>

#include <vnx/vnx.h>


namespace mmx {


const vnx::Hash64 ProofOfStake::VNX_TYPE_HASH(0xf5f1629c4ada2ccfull);
const vnx::Hash64 ProofOfStake::VNX_CODE_HASH(0xe88089f99ab0f91cull);

vnx::Hash64 ProofOfStake::get_type_hash() const {
	return VNX_TYPE_HASH;
}

std::string ProofOfStake::get_type_name() const {
	return "mmx.ProofOfStake";
}

const vnx::TypeCode* ProofOfStake::get_type_code() const {
	return mmx::vnx_native_type_code_ProofOfStake;
}

std::shared_ptr<ProofOfStake> ProofOfStake::create() {
	return std::make_shared<ProofOfStake>();
}

std::shared_ptr<vnx::Value> ProofOfStake::clone() const {
	return std::make_shared<ProofOfStake>(*this);
}

void ProofOfStake::read(vnx::TypeInput& _in, const vnx::TypeCode* _type_code, const uint16_t* _code) {
	vnx::read(_in, *this, _type_code, _code);
}

void ProofOfStake::write(vnx::TypeOutput& _out, const vnx::TypeCode* _type_code, const uint16_t* _code) const {
	vnx::write(_out, *this, _type_code, _code);
}

void ProofOfStake::accept(vnx::Visitor& _visitor) const {
	const vnx::TypeCode* _type_code = mmx::vnx_native_type_code_ProofOfStake;
	_visitor.type_begin(*_type_code);
	_visitor.type_field(_type_code->fields[0], 0); vnx::accept(_visitor, score);
	_visitor.type_field(_type_code->fields[1], 1); vnx::accept(_visitor, plot_id);
	_visitor.type_field(_type_code->fields[2], 2); vnx::accept(_visitor, challenge);
	_visitor.type_field(_type_code->fields[3], 3); vnx::accept(_visitor, difficulty);
	_visitor.type_field(_type_code->fields[4], 4); vnx::accept(_visitor, farmer_key);
	_visitor.type_field(_type_code->fields[5], 5); vnx::accept(_visitor, proof_sig);
	_visitor.type_end(*_type_code);
}

void ProofOfStake::write(std::ostream& _out) const {
	_out << "{\"__type\": \"mmx.ProofOfStake\"";
	_out << ", \"score\": "; vnx::write(_out, score);
	_out << ", \"plot_id\": "; vnx::write(_out, plot_id);
	_out << ", \"challenge\": "; vnx::write(_out, challenge);
	_out << ", \"difficulty\": "; vnx::write(_out, difficulty);
	_out << ", \"farmer_key\": "; vnx::write(_out, farmer_key);
	_out << ", \"proof_sig\": "; vnx::write(_out, proof_sig);
	_out << "}";
}

void ProofOfStake::read(std::istream& _in) {
	if(auto _json = vnx::read_json(_in)) {
		from_object(_json->to_object());
	}
}

vnx::Object ProofOfStake::to_object() const {
	vnx::Object _object;
	_object["__type"] = "mmx.ProofOfStake";
	_object["score"] = score;
	_object["plot_id"] = plot_id;
	_object["challenge"] = challenge;
	_object["difficulty"] = difficulty;
	_object["farmer_key"] = farmer_key;
	_object["proof_sig"] = proof_sig;
	return _object;
}

void ProofOfStake::from_object(const vnx::Object& _object) {
	for(const auto& _entry : _object.field) {
		if(_entry.first == "challenge") {
			_entry.second.to(challenge);
		} else if(_entry.first == "difficulty") {
			_entry.second.to(difficulty);
		} else if(_entry.first == "farmer_key") {
			_entry.second.to(farmer_key);
		} else if(_entry.first == "plot_id") {
			_entry.second.to(plot_id);
		} else if(_entry.first == "proof_sig") {
			_entry.second.to(proof_sig);
		} else if(_entry.first == "score") {
			_entry.second.to(score);
		}
	}
}

vnx::Variant ProofOfStake::get_field(const std::string& _name) const {
	if(_name == "score") {
		return vnx::Variant(score);
	}
	if(_name == "plot_id") {
		return vnx::Variant(plot_id);
	}
	if(_name == "challenge") {
		return vnx::Variant(challenge);
	}
	if(_name == "difficulty") {
		return vnx::Variant(difficulty);
	}
	if(_name == "farmer_key") {
		return vnx::Variant(farmer_key);
	}
	if(_name == "proof_sig") {
		return vnx::Variant(proof_sig);
	}
	return vnx::Variant();
}

void ProofOfStake::set_field(const std::string& _name, const vnx::Variant& _value) {
	if(_name == "score") {
		_value.to(score);
	} else if(_name == "plot_id") {
		_value.to(plot_id);
	} else if(_name == "challenge") {
		_value.to(challenge);
	} else if(_name == "difficulty") {
		_value.to(difficulty);
	} else if(_name == "farmer_key") {
		_value.to(farmer_key);
	} else if(_name == "proof_sig") {
		_value.to(proof_sig);
	}
}

/// \private
std::ostream& operator<<(std::ostream& _out, const ProofOfStake& _value) {
	_value.write(_out);
	return _out;
}

/// \private
std::istream& operator>>(std::istream& _in, ProofOfStake& _value) {
	_value.read(_in);
	return _in;
}

const vnx::TypeCode* ProofOfStake::static_get_type_code() {
	const vnx::TypeCode* type_code = vnx::get_type_code(VNX_TYPE_HASH);
	if(!type_code) {
		type_code = vnx::register_type_code(static_create_type_code());
	}
	return type_code;
}

std::shared_ptr<vnx::TypeCode> ProofOfStake::static_create_type_code() {
	auto type_code = std::make_shared<vnx::TypeCode>();
	type_code->name = "mmx.ProofOfStake";
	type_code->type_hash = vnx::Hash64(0xf5f1629c4ada2ccfull);
	type_code->code_hash = vnx::Hash64(0xe88089f99ab0f91cull);
	type_code->is_native = true;
	type_code->is_class = true;
	type_code->native_size = sizeof(::mmx::ProofOfStake);
	type_code->parents.resize(1);
	type_code->parents[0] = ::mmx::ProofOfSpace::static_get_type_code();
	type_code->create_value = []() -> std::shared_ptr<vnx::Value> { return std::make_shared<ProofOfStake>(); };
	type_code->fields.resize(6);
	{
		auto& field = type_code->fields[0];
		field.data_size = 2;
		field.name = "score";
		field.code = {2};
	}
	{
		auto& field = type_code->fields[1];
		field.is_extended = true;
		field.name = "plot_id";
		field.code = {11, 32, 1};
	}
	{
		auto& field = type_code->fields[2];
		field.is_extended = true;
		field.name = "challenge";
		field.code = {11, 32, 1};
	}
	{
		auto& field = type_code->fields[3];
		field.data_size = 8;
		field.name = "difficulty";
		field.code = {4};
	}
	{
		auto& field = type_code->fields[4];
		field.is_extended = true;
		field.name = "farmer_key";
		field.code = {11, 33, 1};
	}
	{
		auto& field = type_code->fields[5];
		field.is_extended = true;
		field.name = "proof_sig";
		field.code = {11, 64, 1};
	}
	type_code->build();
	return type_code;
}

std::shared_ptr<vnx::Value> ProofOfStake::vnx_call_switch(std::shared_ptr<const vnx::Value> _method) {
	switch(_method->get_type_hash()) {
	}
	return nullptr;
}


} // namespace mmx


namespace vnx {

void read(TypeInput& in, ::mmx::ProofOfStake& value, const TypeCode* type_code, const uint16_t* code) {
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
			vnx::read_value(_buf + _field->offset, value.score, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[3]) {
			vnx::read_value(_buf + _field->offset, value.difficulty, _field->code.data());
		}
	}
	for(const auto* _field : type_code->ext_fields) {
		switch(_field->native_index) {
			case 1: vnx::read(in, value.plot_id, type_code, _field->code.data()); break;
			case 2: vnx::read(in, value.challenge, type_code, _field->code.data()); break;
			case 4: vnx::read(in, value.farmer_key, type_code, _field->code.data()); break;
			case 5: vnx::read(in, value.proof_sig, type_code, _field->code.data()); break;
			default: vnx::skip(in, type_code, _field->code.data());
		}
	}
}

void write(TypeOutput& out, const ::mmx::ProofOfStake& value, const TypeCode* type_code, const uint16_t* code) {
	if(code && code[0] == CODE_OBJECT) {
		vnx::write(out, value.to_object(), nullptr, code);
		return;
	}
	if(!type_code || (code && code[0] == CODE_ANY)) {
		type_code = mmx::vnx_native_type_code_ProofOfStake;
		out.write_type_code(type_code);
		vnx::write_class_header<::mmx::ProofOfStake>(out);
	}
	else if(code && code[0] == CODE_STRUCT) {
		type_code = type_code->depends[code[1]];
	}
	auto* const _buf = out.write(10);
	vnx::write_value(_buf + 0, value.score);
	vnx::write_value(_buf + 2, value.difficulty);
	vnx::write(out, value.plot_id, type_code, type_code->fields[1].code.data());
	vnx::write(out, value.challenge, type_code, type_code->fields[2].code.data());
	vnx::write(out, value.farmer_key, type_code, type_code->fields[4].code.data());
	vnx::write(out, value.proof_sig, type_code, type_code->fields[5].code.data());
}

void read(std::istream& in, ::mmx::ProofOfStake& value) {
	value.read(in);
}

void write(std::ostream& out, const ::mmx::ProofOfStake& value) {
	value.write(out);
}

void accept(Visitor& visitor, const ::mmx::ProofOfStake& value) {
	value.accept(visitor);
}

} // vnx
