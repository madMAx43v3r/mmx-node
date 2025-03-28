
// AUTO GENERATED by vnxcppcodegen

#include <mmx/package.hxx>
#include <mmx/HarvesterBase.hxx>
#include <vnx/NoSuchMethod.hxx>
#include <mmx/Challenge.hxx>
#include <mmx/FarmInfo.hxx>
#include <mmx/Harvester_add_plot_dir.hxx>
#include <mmx/Harvester_add_plot_dir_return.hxx>
#include <mmx/Harvester_get_farm_info.hxx>
#include <mmx/Harvester_get_farm_info_return.hxx>
#include <mmx/Harvester_get_total_bytes.hxx>
#include <mmx/Harvester_get_total_bytes_return.hxx>
#include <mmx/Harvester_reload.hxx>
#include <mmx/Harvester_reload_return.hxx>
#include <mmx/Harvester_rem_plot_dir.hxx>
#include <mmx/Harvester_rem_plot_dir_return.hxx>
#include <vnx/Module.h>
#include <vnx/ModuleInterface_vnx_get_config.hxx>
#include <vnx/ModuleInterface_vnx_get_config_return.hxx>
#include <vnx/ModuleInterface_vnx_get_config_object.hxx>
#include <vnx/ModuleInterface_vnx_get_config_object_return.hxx>
#include <vnx/ModuleInterface_vnx_get_module_info.hxx>
#include <vnx/ModuleInterface_vnx_get_module_info_return.hxx>
#include <vnx/ModuleInterface_vnx_get_type_code.hxx>
#include <vnx/ModuleInterface_vnx_get_type_code_return.hxx>
#include <vnx/ModuleInterface_vnx_restart.hxx>
#include <vnx/ModuleInterface_vnx_restart_return.hxx>
#include <vnx/ModuleInterface_vnx_self_test.hxx>
#include <vnx/ModuleInterface_vnx_self_test_return.hxx>
#include <vnx/ModuleInterface_vnx_set_config.hxx>
#include <vnx/ModuleInterface_vnx_set_config_return.hxx>
#include <vnx/ModuleInterface_vnx_set_config_object.hxx>
#include <vnx/ModuleInterface_vnx_set_config_object_return.hxx>
#include <vnx/ModuleInterface_vnx_stop.hxx>
#include <vnx/ModuleInterface_vnx_stop_return.hxx>
#include <vnx/TopicPtr.hpp>
#include <vnx/addons/HttpComponent_http_request.hxx>
#include <vnx/addons/HttpComponent_http_request_return.hxx>
#include <vnx/addons/HttpComponent_http_request_chunk.hxx>
#include <vnx/addons/HttpComponent_http_request_chunk_return.hxx>
#include <vnx/addons/HttpData.hxx>
#include <vnx/addons/HttpRequest.hxx>
#include <vnx/addons/HttpResponse.hxx>

#include <vnx/vnx.h>


namespace mmx {


const vnx::Hash64 HarvesterBase::VNX_TYPE_HASH(0xc17118896cde1555ull);
const vnx::Hash64 HarvesterBase::VNX_CODE_HASH(0x58f3117ab806b859ull);

HarvesterBase::HarvesterBase(const std::string& _vnx_name)
	:	Module::Module(_vnx_name)
{
	vnx::read_config(vnx_name + ".input_challenges", input_challenges);
	vnx::read_config(vnx_name + ".output_info", output_info);
	vnx::read_config(vnx_name + ".output_proofs", output_proofs);
	vnx::read_config(vnx_name + ".output_lookups", output_lookups);
	vnx::read_config(vnx_name + ".output_partials", output_partials);
	vnx::read_config(vnx_name + ".plot_dirs", plot_dirs);
	vnx::read_config(vnx_name + ".dir_blacklist", dir_blacklist);
	vnx::read_config(vnx_name + ".node_server", node_server);
	vnx::read_config(vnx_name + ".farmer_server", farmer_server);
	vnx::read_config(vnx_name + ".config_path", config_path);
	vnx::read_config(vnx_name + ".storage_path", storage_path);
	vnx::read_config(vnx_name + ".my_name", my_name);
	vnx::read_config(vnx_name + ".max_queue_ms", max_queue_ms);
	vnx::read_config(vnx_name + ".reload_interval", reload_interval);
	vnx::read_config(vnx_name + ".nft_query_interval", nft_query_interval);
	vnx::read_config(vnx_name + ".num_threads", num_threads);
	vnx::read_config(vnx_name + ".max_recursion", max_recursion);
	vnx::read_config(vnx_name + ".recursive_search", recursive_search);
	vnx::read_config(vnx_name + ".farm_virtual_plots", farm_virtual_plots);
}

vnx::Hash64 HarvesterBase::get_type_hash() const {
	return VNX_TYPE_HASH;
}

std::string HarvesterBase::get_type_name() const {
	return "mmx.Harvester";
}

const vnx::TypeCode* HarvesterBase::get_type_code() const {
	return mmx::vnx_native_type_code_HarvesterBase;
}

void HarvesterBase::accept(vnx::Visitor& _visitor) const {
	const vnx::TypeCode* _type_code = mmx::vnx_native_type_code_HarvesterBase;
	_visitor.type_begin(*_type_code);
	_visitor.type_field(_type_code->fields[0], 0); vnx::accept(_visitor, input_challenges);
	_visitor.type_field(_type_code->fields[1], 1); vnx::accept(_visitor, output_info);
	_visitor.type_field(_type_code->fields[2], 2); vnx::accept(_visitor, output_proofs);
	_visitor.type_field(_type_code->fields[3], 3); vnx::accept(_visitor, output_lookups);
	_visitor.type_field(_type_code->fields[4], 4); vnx::accept(_visitor, output_partials);
	_visitor.type_field(_type_code->fields[5], 5); vnx::accept(_visitor, plot_dirs);
	_visitor.type_field(_type_code->fields[6], 6); vnx::accept(_visitor, dir_blacklist);
	_visitor.type_field(_type_code->fields[7], 7); vnx::accept(_visitor, node_server);
	_visitor.type_field(_type_code->fields[8], 8); vnx::accept(_visitor, farmer_server);
	_visitor.type_field(_type_code->fields[9], 9); vnx::accept(_visitor, config_path);
	_visitor.type_field(_type_code->fields[10], 10); vnx::accept(_visitor, storage_path);
	_visitor.type_field(_type_code->fields[11], 11); vnx::accept(_visitor, my_name);
	_visitor.type_field(_type_code->fields[12], 12); vnx::accept(_visitor, max_queue_ms);
	_visitor.type_field(_type_code->fields[13], 13); vnx::accept(_visitor, reload_interval);
	_visitor.type_field(_type_code->fields[14], 14); vnx::accept(_visitor, nft_query_interval);
	_visitor.type_field(_type_code->fields[15], 15); vnx::accept(_visitor, num_threads);
	_visitor.type_field(_type_code->fields[16], 16); vnx::accept(_visitor, max_recursion);
	_visitor.type_field(_type_code->fields[17], 17); vnx::accept(_visitor, recursive_search);
	_visitor.type_field(_type_code->fields[18], 18); vnx::accept(_visitor, farm_virtual_plots);
	_visitor.type_end(*_type_code);
}

void HarvesterBase::write(std::ostream& _out) const {
	_out << "{";
	_out << "\"input_challenges\": "; vnx::write(_out, input_challenges);
	_out << ", \"output_info\": "; vnx::write(_out, output_info);
	_out << ", \"output_proofs\": "; vnx::write(_out, output_proofs);
	_out << ", \"output_lookups\": "; vnx::write(_out, output_lookups);
	_out << ", \"output_partials\": "; vnx::write(_out, output_partials);
	_out << ", \"plot_dirs\": "; vnx::write(_out, plot_dirs);
	_out << ", \"dir_blacklist\": "; vnx::write(_out, dir_blacklist);
	_out << ", \"node_server\": "; vnx::write(_out, node_server);
	_out << ", \"farmer_server\": "; vnx::write(_out, farmer_server);
	_out << ", \"config_path\": "; vnx::write(_out, config_path);
	_out << ", \"storage_path\": "; vnx::write(_out, storage_path);
	_out << ", \"my_name\": "; vnx::write(_out, my_name);
	_out << ", \"max_queue_ms\": "; vnx::write(_out, max_queue_ms);
	_out << ", \"reload_interval\": "; vnx::write(_out, reload_interval);
	_out << ", \"nft_query_interval\": "; vnx::write(_out, nft_query_interval);
	_out << ", \"num_threads\": "; vnx::write(_out, num_threads);
	_out << ", \"max_recursion\": "; vnx::write(_out, max_recursion);
	_out << ", \"recursive_search\": "; vnx::write(_out, recursive_search);
	_out << ", \"farm_virtual_plots\": "; vnx::write(_out, farm_virtual_plots);
	_out << "}";
}

void HarvesterBase::read(std::istream& _in) {
	if(auto _json = vnx::read_json(_in)) {
		from_object(_json->to_object());
	}
}

vnx::Object HarvesterBase::to_object() const {
	vnx::Object _object;
	_object["__type"] = "mmx.Harvester";
	_object["input_challenges"] = input_challenges;
	_object["output_info"] = output_info;
	_object["output_proofs"] = output_proofs;
	_object["output_lookups"] = output_lookups;
	_object["output_partials"] = output_partials;
	_object["plot_dirs"] = plot_dirs;
	_object["dir_blacklist"] = dir_blacklist;
	_object["node_server"] = node_server;
	_object["farmer_server"] = farmer_server;
	_object["config_path"] = config_path;
	_object["storage_path"] = storage_path;
	_object["my_name"] = my_name;
	_object["max_queue_ms"] = max_queue_ms;
	_object["reload_interval"] = reload_interval;
	_object["nft_query_interval"] = nft_query_interval;
	_object["num_threads"] = num_threads;
	_object["max_recursion"] = max_recursion;
	_object["recursive_search"] = recursive_search;
	_object["farm_virtual_plots"] = farm_virtual_plots;
	return _object;
}

void HarvesterBase::from_object(const vnx::Object& _object) {
	for(const auto& _entry : _object.field) {
		if(_entry.first == "config_path") {
			_entry.second.to(config_path);
		} else if(_entry.first == "dir_blacklist") {
			_entry.second.to(dir_blacklist);
		} else if(_entry.first == "farm_virtual_plots") {
			_entry.second.to(farm_virtual_plots);
		} else if(_entry.first == "farmer_server") {
			_entry.second.to(farmer_server);
		} else if(_entry.first == "input_challenges") {
			_entry.second.to(input_challenges);
		} else if(_entry.first == "max_queue_ms") {
			_entry.second.to(max_queue_ms);
		} else if(_entry.first == "max_recursion") {
			_entry.second.to(max_recursion);
		} else if(_entry.first == "my_name") {
			_entry.second.to(my_name);
		} else if(_entry.first == "nft_query_interval") {
			_entry.second.to(nft_query_interval);
		} else if(_entry.first == "node_server") {
			_entry.second.to(node_server);
		} else if(_entry.first == "num_threads") {
			_entry.second.to(num_threads);
		} else if(_entry.first == "output_info") {
			_entry.second.to(output_info);
		} else if(_entry.first == "output_lookups") {
			_entry.second.to(output_lookups);
		} else if(_entry.first == "output_partials") {
			_entry.second.to(output_partials);
		} else if(_entry.first == "output_proofs") {
			_entry.second.to(output_proofs);
		} else if(_entry.first == "plot_dirs") {
			_entry.second.to(plot_dirs);
		} else if(_entry.first == "recursive_search") {
			_entry.second.to(recursive_search);
		} else if(_entry.first == "reload_interval") {
			_entry.second.to(reload_interval);
		} else if(_entry.first == "storage_path") {
			_entry.second.to(storage_path);
		}
	}
}

vnx::Variant HarvesterBase::get_field(const std::string& _name) const {
	if(_name == "input_challenges") {
		return vnx::Variant(input_challenges);
	}
	if(_name == "output_info") {
		return vnx::Variant(output_info);
	}
	if(_name == "output_proofs") {
		return vnx::Variant(output_proofs);
	}
	if(_name == "output_lookups") {
		return vnx::Variant(output_lookups);
	}
	if(_name == "output_partials") {
		return vnx::Variant(output_partials);
	}
	if(_name == "plot_dirs") {
		return vnx::Variant(plot_dirs);
	}
	if(_name == "dir_blacklist") {
		return vnx::Variant(dir_blacklist);
	}
	if(_name == "node_server") {
		return vnx::Variant(node_server);
	}
	if(_name == "farmer_server") {
		return vnx::Variant(farmer_server);
	}
	if(_name == "config_path") {
		return vnx::Variant(config_path);
	}
	if(_name == "storage_path") {
		return vnx::Variant(storage_path);
	}
	if(_name == "my_name") {
		return vnx::Variant(my_name);
	}
	if(_name == "max_queue_ms") {
		return vnx::Variant(max_queue_ms);
	}
	if(_name == "reload_interval") {
		return vnx::Variant(reload_interval);
	}
	if(_name == "nft_query_interval") {
		return vnx::Variant(nft_query_interval);
	}
	if(_name == "num_threads") {
		return vnx::Variant(num_threads);
	}
	if(_name == "max_recursion") {
		return vnx::Variant(max_recursion);
	}
	if(_name == "recursive_search") {
		return vnx::Variant(recursive_search);
	}
	if(_name == "farm_virtual_plots") {
		return vnx::Variant(farm_virtual_plots);
	}
	return vnx::Variant();
}

void HarvesterBase::set_field(const std::string& _name, const vnx::Variant& _value) {
	if(_name == "input_challenges") {
		_value.to(input_challenges);
	} else if(_name == "output_info") {
		_value.to(output_info);
	} else if(_name == "output_proofs") {
		_value.to(output_proofs);
	} else if(_name == "output_lookups") {
		_value.to(output_lookups);
	} else if(_name == "output_partials") {
		_value.to(output_partials);
	} else if(_name == "plot_dirs") {
		_value.to(plot_dirs);
	} else if(_name == "dir_blacklist") {
		_value.to(dir_blacklist);
	} else if(_name == "node_server") {
		_value.to(node_server);
	} else if(_name == "farmer_server") {
		_value.to(farmer_server);
	} else if(_name == "config_path") {
		_value.to(config_path);
	} else if(_name == "storage_path") {
		_value.to(storage_path);
	} else if(_name == "my_name") {
		_value.to(my_name);
	} else if(_name == "max_queue_ms") {
		_value.to(max_queue_ms);
	} else if(_name == "reload_interval") {
		_value.to(reload_interval);
	} else if(_name == "nft_query_interval") {
		_value.to(nft_query_interval);
	} else if(_name == "num_threads") {
		_value.to(num_threads);
	} else if(_name == "max_recursion") {
		_value.to(max_recursion);
	} else if(_name == "recursive_search") {
		_value.to(recursive_search);
	} else if(_name == "farm_virtual_plots") {
		_value.to(farm_virtual_plots);
	}
}

/// \private
std::ostream& operator<<(std::ostream& _out, const HarvesterBase& _value) {
	_value.write(_out);
	return _out;
}

/// \private
std::istream& operator>>(std::istream& _in, HarvesterBase& _value) {
	_value.read(_in);
	return _in;
}

const vnx::TypeCode* HarvesterBase::static_get_type_code() {
	const vnx::TypeCode* type_code = vnx::get_type_code(VNX_TYPE_HASH);
	if(!type_code) {
		type_code = vnx::register_type_code(static_create_type_code());
	}
	return type_code;
}

std::shared_ptr<vnx::TypeCode> HarvesterBase::static_create_type_code() {
	auto type_code = std::make_shared<vnx::TypeCode>();
	type_code->name = "mmx.Harvester";
	type_code->type_hash = vnx::Hash64(0xc17118896cde1555ull);
	type_code->code_hash = vnx::Hash64(0x58f3117ab806b859ull);
	type_code->is_native = true;
	type_code->native_size = sizeof(::mmx::HarvesterBase);
	type_code->methods.resize(16);
	type_code->methods[0] = ::mmx::Harvester_add_plot_dir::static_get_type_code();
	type_code->methods[1] = ::mmx::Harvester_get_farm_info::static_get_type_code();
	type_code->methods[2] = ::mmx::Harvester_get_total_bytes::static_get_type_code();
	type_code->methods[3] = ::mmx::Harvester_reload::static_get_type_code();
	type_code->methods[4] = ::mmx::Harvester_rem_plot_dir::static_get_type_code();
	type_code->methods[5] = ::vnx::ModuleInterface_vnx_get_config::static_get_type_code();
	type_code->methods[6] = ::vnx::ModuleInterface_vnx_get_config_object::static_get_type_code();
	type_code->methods[7] = ::vnx::ModuleInterface_vnx_get_module_info::static_get_type_code();
	type_code->methods[8] = ::vnx::ModuleInterface_vnx_get_type_code::static_get_type_code();
	type_code->methods[9] = ::vnx::ModuleInterface_vnx_restart::static_get_type_code();
	type_code->methods[10] = ::vnx::ModuleInterface_vnx_self_test::static_get_type_code();
	type_code->methods[11] = ::vnx::ModuleInterface_vnx_set_config::static_get_type_code();
	type_code->methods[12] = ::vnx::ModuleInterface_vnx_set_config_object::static_get_type_code();
	type_code->methods[13] = ::vnx::ModuleInterface_vnx_stop::static_get_type_code();
	type_code->methods[14] = ::vnx::addons::HttpComponent_http_request::static_get_type_code();
	type_code->methods[15] = ::vnx::addons::HttpComponent_http_request_chunk::static_get_type_code();
	type_code->fields.resize(19);
	{
		auto& field = type_code->fields[0];
		field.is_extended = true;
		field.name = "input_challenges";
		field.value = vnx::to_string("harvester.challenges");
		field.code = {12, 5};
	}
	{
		auto& field = type_code->fields[1];
		field.is_extended = true;
		field.name = "output_info";
		field.value = vnx::to_string("harvester.info");
		field.code = {12, 5};
	}
	{
		auto& field = type_code->fields[2];
		field.is_extended = true;
		field.name = "output_proofs";
		field.value = vnx::to_string("harvester.proof");
		field.code = {12, 5};
	}
	{
		auto& field = type_code->fields[3];
		field.is_extended = true;
		field.name = "output_lookups";
		field.value = vnx::to_string("harvester.lookups");
		field.code = {12, 5};
	}
	{
		auto& field = type_code->fields[4];
		field.is_extended = true;
		field.name = "output_partials";
		field.value = vnx::to_string("harvester.partials");
		field.code = {12, 5};
	}
	{
		auto& field = type_code->fields[5];
		field.is_extended = true;
		field.name = "plot_dirs";
		field.code = {12, 32};
	}
	{
		auto& field = type_code->fields[6];
		field.is_extended = true;
		field.name = "dir_blacklist";
		field.code = {12, 32};
	}
	{
		auto& field = type_code->fields[7];
		field.is_extended = true;
		field.name = "node_server";
		field.value = vnx::to_string("Node");
		field.code = {32};
	}
	{
		auto& field = type_code->fields[8];
		field.is_extended = true;
		field.name = "farmer_server";
		field.value = vnx::to_string("Farmer");
		field.code = {32};
	}
	{
		auto& field = type_code->fields[9];
		field.is_extended = true;
		field.name = "config_path";
		field.code = {32};
	}
	{
		auto& field = type_code->fields[10];
		field.is_extended = true;
		field.name = "storage_path";
		field.code = {32};
	}
	{
		auto& field = type_code->fields[11];
		field.is_extended = true;
		field.name = "my_name";
		field.code = {32};
	}
	{
		auto& field = type_code->fields[12];
		field.data_size = 4;
		field.name = "max_queue_ms";
		field.value = vnx::to_string(10000);
		field.code = {7};
	}
	{
		auto& field = type_code->fields[13];
		field.data_size = 4;
		field.name = "reload_interval";
		field.value = vnx::to_string(3600);
		field.code = {7};
	}
	{
		auto& field = type_code->fields[14];
		field.data_size = 4;
		field.name = "nft_query_interval";
		field.value = vnx::to_string(60);
		field.code = {7};
	}
	{
		auto& field = type_code->fields[15];
		field.data_size = 4;
		field.name = "num_threads";
		field.value = vnx::to_string(32);
		field.code = {3};
	}
	{
		auto& field = type_code->fields[16];
		field.data_size = 4;
		field.name = "max_recursion";
		field.value = vnx::to_string(4);
		field.code = {3};
	}
	{
		auto& field = type_code->fields[17];
		field.data_size = 1;
		field.name = "recursive_search";
		field.value = vnx::to_string(true);
		field.code = {31};
	}
	{
		auto& field = type_code->fields[18];
		field.data_size = 1;
		field.name = "farm_virtual_plots";
		field.value = vnx::to_string(true);
		field.code = {31};
	}
	type_code->build();
	return type_code;
}

void HarvesterBase::vnx_handle_switch(std::shared_ptr<const vnx::Value> _value) {
	const auto* _type_code = _value->get_type_code();
	while(_type_code) {
		switch(_type_code->type_hash) {
			case 0x4bf49f8022405249ull:
				handle(std::static_pointer_cast<const ::mmx::Challenge>(_value));
				return;
			default:
				_type_code = _type_code->super;
		}
	}
	handle(std::static_pointer_cast<const vnx::Value>(_value));
}

std::shared_ptr<vnx::Value> HarvesterBase::vnx_call_switch(std::shared_ptr<const vnx::Value> _method, const vnx::request_id_t& _request_id) {
	switch(_method->get_type_hash()) {
		case 0x61714d1c7ecaffddull: {
			auto _args = std::static_pointer_cast<const ::mmx::Harvester_add_plot_dir>(_method);
			auto _return_value = ::mmx::Harvester_add_plot_dir_return::create();
			add_plot_dir(_args->path);
			return _return_value;
		}
		case 0x129f91b9ade2891full: {
			auto _args = std::static_pointer_cast<const ::mmx::Harvester_get_farm_info>(_method);
			auto _return_value = ::mmx::Harvester_get_farm_info_return::create();
			_return_value->_ret_0 = get_farm_info();
			return _return_value;
		}
		case 0x36f2104b41d9a25cull: {
			auto _args = std::static_pointer_cast<const ::mmx::Harvester_get_total_bytes>(_method);
			auto _return_value = ::mmx::Harvester_get_total_bytes_return::create();
			_return_value->_ret_0 = get_total_bytes();
			return _return_value;
		}
		case 0xc67a4577de7e85caull: {
			auto _args = std::static_pointer_cast<const ::mmx::Harvester_reload>(_method);
			auto _return_value = ::mmx::Harvester_reload_return::create();
			reload();
			return _return_value;
		}
		case 0x57674e56f3ab6076ull: {
			auto _args = std::static_pointer_cast<const ::mmx::Harvester_rem_plot_dir>(_method);
			auto _return_value = ::mmx::Harvester_rem_plot_dir_return::create();
			rem_plot_dir(_args->path);
			return _return_value;
		}
		case 0xbbc7f1a01044d294ull: {
			auto _args = std::static_pointer_cast<const ::vnx::ModuleInterface_vnx_get_config>(_method);
			auto _return_value = ::vnx::ModuleInterface_vnx_get_config_return::create();
			_return_value->_ret_0 = vnx_get_config(_args->name);
			return _return_value;
		}
		case 0x17f58f68bf83abc0ull: {
			auto _args = std::static_pointer_cast<const ::vnx::ModuleInterface_vnx_get_config_object>(_method);
			auto _return_value = ::vnx::ModuleInterface_vnx_get_config_object_return::create();
			_return_value->_ret_0 = vnx_get_config_object();
			return _return_value;
		}
		case 0xf6d82bdf66d034a1ull: {
			auto _args = std::static_pointer_cast<const ::vnx::ModuleInterface_vnx_get_module_info>(_method);
			auto _return_value = ::vnx::ModuleInterface_vnx_get_module_info_return::create();
			_return_value->_ret_0 = vnx_get_module_info();
			return _return_value;
		}
		case 0x305ec4d628960e5dull: {
			auto _args = std::static_pointer_cast<const ::vnx::ModuleInterface_vnx_get_type_code>(_method);
			auto _return_value = ::vnx::ModuleInterface_vnx_get_type_code_return::create();
			_return_value->_ret_0 = vnx_get_type_code();
			return _return_value;
		}
		case 0x9e95dc280cecca1bull: {
			auto _args = std::static_pointer_cast<const ::vnx::ModuleInterface_vnx_restart>(_method);
			auto _return_value = ::vnx::ModuleInterface_vnx_restart_return::create();
			vnx_restart();
			return _return_value;
		}
		case 0x6ce3775b41a42697ull: {
			auto _args = std::static_pointer_cast<const ::vnx::ModuleInterface_vnx_self_test>(_method);
			auto _return_value = ::vnx::ModuleInterface_vnx_self_test_return::create();
			_return_value->_ret_0 = vnx_self_test();
			return _return_value;
		}
		case 0x362aac91373958b7ull: {
			auto _args = std::static_pointer_cast<const ::vnx::ModuleInterface_vnx_set_config>(_method);
			auto _return_value = ::vnx::ModuleInterface_vnx_set_config_return::create();
			vnx_set_config(_args->name, _args->value);
			return _return_value;
		}
		case 0xca30f814f17f322full: {
			auto _args = std::static_pointer_cast<const ::vnx::ModuleInterface_vnx_set_config_object>(_method);
			auto _return_value = ::vnx::ModuleInterface_vnx_set_config_object_return::create();
			vnx_set_config_object(_args->config);
			return _return_value;
		}
		case 0x7ab49ce3d1bfc0d2ull: {
			auto _args = std::static_pointer_cast<const ::vnx::ModuleInterface_vnx_stop>(_method);
			auto _return_value = ::vnx::ModuleInterface_vnx_stop_return::create();
			vnx_stop();
			return _return_value;
		}
		case 0xe0b6c38f619bad92ull: {
			auto _args = std::static_pointer_cast<const ::vnx::addons::HttpComponent_http_request>(_method);
			http_request_async(_args->request, _args->sub_path, _request_id);
			return nullptr;
		}
		case 0x97e79d08440406d5ull: {
			auto _args = std::static_pointer_cast<const ::vnx::addons::HttpComponent_http_request_chunk>(_method);
			http_request_chunk_async(_args->request, _args->sub_path, _args->offset, _args->max_bytes, _request_id);
			return nullptr;
		}
	}
	auto _ex = vnx::NoSuchMethod::create();
	_ex->method = _method->get_type_name();
	return _ex;
}

void HarvesterBase::http_request_async_return(const vnx::request_id_t& _request_id, const std::shared_ptr<const ::vnx::addons::HttpResponse>& _ret_0) const {
	auto _return_value = ::vnx::addons::HttpComponent_http_request_return::create();
	_return_value->_ret_0 = _ret_0;
	vnx_async_return(_request_id, _return_value);
}

void HarvesterBase::http_request_chunk_async_return(const vnx::request_id_t& _request_id, const std::shared_ptr<const ::vnx::addons::HttpData>& _ret_0) const {
	auto _return_value = ::vnx::addons::HttpComponent_http_request_chunk_return::create();
	_return_value->_ret_0 = _ret_0;
	vnx_async_return(_request_id, _return_value);
}


} // namespace mmx


namespace vnx {

void read(TypeInput& in, ::mmx::HarvesterBase& value, const TypeCode* type_code, const uint16_t* code) {
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
		if(const auto* const _field = type_code->field_map[12]) {
			vnx::read_value(_buf + _field->offset, value.max_queue_ms, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[13]) {
			vnx::read_value(_buf + _field->offset, value.reload_interval, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[14]) {
			vnx::read_value(_buf + _field->offset, value.nft_query_interval, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[15]) {
			vnx::read_value(_buf + _field->offset, value.num_threads, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[16]) {
			vnx::read_value(_buf + _field->offset, value.max_recursion, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[17]) {
			vnx::read_value(_buf + _field->offset, value.recursive_search, _field->code.data());
		}
		if(const auto* const _field = type_code->field_map[18]) {
			vnx::read_value(_buf + _field->offset, value.farm_virtual_plots, _field->code.data());
		}
	}
	for(const auto* _field : type_code->ext_fields) {
		switch(_field->native_index) {
			case 0: vnx::read(in, value.input_challenges, type_code, _field->code.data()); break;
			case 1: vnx::read(in, value.output_info, type_code, _field->code.data()); break;
			case 2: vnx::read(in, value.output_proofs, type_code, _field->code.data()); break;
			case 3: vnx::read(in, value.output_lookups, type_code, _field->code.data()); break;
			case 4: vnx::read(in, value.output_partials, type_code, _field->code.data()); break;
			case 5: vnx::read(in, value.plot_dirs, type_code, _field->code.data()); break;
			case 6: vnx::read(in, value.dir_blacklist, type_code, _field->code.data()); break;
			case 7: vnx::read(in, value.node_server, type_code, _field->code.data()); break;
			case 8: vnx::read(in, value.farmer_server, type_code, _field->code.data()); break;
			case 9: vnx::read(in, value.config_path, type_code, _field->code.data()); break;
			case 10: vnx::read(in, value.storage_path, type_code, _field->code.data()); break;
			case 11: vnx::read(in, value.my_name, type_code, _field->code.data()); break;
			default: vnx::skip(in, type_code, _field->code.data());
		}
	}
}

void write(TypeOutput& out, const ::mmx::HarvesterBase& value, const TypeCode* type_code, const uint16_t* code) {
	if(code && code[0] == CODE_OBJECT) {
		vnx::write(out, value.to_object(), nullptr, code);
		return;
	}
	if(!type_code || (code && code[0] == CODE_ANY)) {
		type_code = mmx::vnx_native_type_code_HarvesterBase;
		out.write_type_code(type_code);
		vnx::write_class_header<::mmx::HarvesterBase>(out);
	}
	else if(code && code[0] == CODE_STRUCT) {
		type_code = type_code->depends[code[1]];
	}
	auto* const _buf = out.write(22);
	vnx::write_value(_buf + 0, value.max_queue_ms);
	vnx::write_value(_buf + 4, value.reload_interval);
	vnx::write_value(_buf + 8, value.nft_query_interval);
	vnx::write_value(_buf + 12, value.num_threads);
	vnx::write_value(_buf + 16, value.max_recursion);
	vnx::write_value(_buf + 20, value.recursive_search);
	vnx::write_value(_buf + 21, value.farm_virtual_plots);
	vnx::write(out, value.input_challenges, type_code, type_code->fields[0].code.data());
	vnx::write(out, value.output_info, type_code, type_code->fields[1].code.data());
	vnx::write(out, value.output_proofs, type_code, type_code->fields[2].code.data());
	vnx::write(out, value.output_lookups, type_code, type_code->fields[3].code.data());
	vnx::write(out, value.output_partials, type_code, type_code->fields[4].code.data());
	vnx::write(out, value.plot_dirs, type_code, type_code->fields[5].code.data());
	vnx::write(out, value.dir_blacklist, type_code, type_code->fields[6].code.data());
	vnx::write(out, value.node_server, type_code, type_code->fields[7].code.data());
	vnx::write(out, value.farmer_server, type_code, type_code->fields[8].code.data());
	vnx::write(out, value.config_path, type_code, type_code->fields[9].code.data());
	vnx::write(out, value.storage_path, type_code, type_code->fields[10].code.data());
	vnx::write(out, value.my_name, type_code, type_code->fields[11].code.data());
}

void read(std::istream& in, ::mmx::HarvesterBase& value) {
	value.read(in);
}

void write(std::ostream& out, const ::mmx::HarvesterBase& value) {
	value.write(out);
}

void accept(Visitor& visitor, const ::mmx::HarvesterBase& value) {
	value.accept(visitor);
}

} // vnx
