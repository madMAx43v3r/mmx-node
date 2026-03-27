/*
 * mmx_node.cpp
 *
 *  Created on: Dec 10, 2021
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/Wallet.h>
#include <mmx/TimeLord.h>
#include <mmx/Farmer.h>
#include <mmx/Harvester.h>
#include <mmx/ProofServer.h>
#include <mmx/Router.h>
#include <mmx/WebAPI.h>
#include <mmx/Qt_GUI.h>
#include <mmx/WalletClient.hxx>
#include <mmx/secp256k1.hpp>
#include <mmx/utils.h>

#include <sha256_ni.h>
#include <sha256_arm.h>
#include <sha256_avx2.h>

#include <vnx/vnx.h>
#include <vnx/Server.h>
#include <vnx/Terminal.h>
#include <vnx/TcpEndpoint.hxx>
#include <vnx/addons/FileServer.h>
#include <vnx/addons/HttpServer.h>
#include <vnx/addons/HttpBalancer.h>

#ifdef WITH_CUDA
#include <mmx/pos/cuda_recompute.h>
#endif


int main(int argc, char** argv)
{
	mmx::secp256k1_init();

	std::string mmx_home;
	std::string mmx_network;
	if(auto path = ::getenv("MMX_HOME")) {
		mmx_home = path;
		std::cerr << "MMX_HOME = " << mmx_home << std::endl;
	}
	if(auto path = ::getenv("MMX_NETWORK")) {
		mmx_network = path;
		std::cerr << "MMX_NETWORK = " << mmx_network << std::endl;
	}
	vnx::Directory(mmx_network).create();

	vnx::write_config("mmx_node.log_file_path", mmx_network + "logs/");

	std::map<std::string, std::string> options;
	options["t"] = "timelord";

	vnx::init("mmx_node", argc, argv, options);
	{
		std::string version;
		std::string commit;
		vnx::read_config("build.version", version);
		vnx::read_config("build.commit", commit);
		vnx::log_info() << "Build version: " << version;
		vnx::log_info() << "Build commit: " << commit;
	}

	if(!vnx::is_config_protected("passwd")) {
		throw std::logic_error("missing config protection");
	}
	const auto params = mmx::get_params();
	const auto api_token = mmx::hash_t::random().to_string();

	bool with_gui = false;
	bool with_farmer = true;
	bool with_wallet = true;
	bool with_timelord = true;
	bool with_harvester = true;
	bool allow_remote = false;
	uint32_t wapi_threads = 1;
	std::string endpoint = "0.0.0.0";		// requires allow_remote

	vnx::read_config("gui", with_gui);
	vnx::read_config("wallet", with_wallet);
	vnx::read_config("farmer", with_farmer);
	vnx::read_config("timelord", with_timelord);
	vnx::read_config("harvester", with_harvester);
	vnx::read_config("allow_remote", allow_remote);
	vnx::read_config("wapi_threads", wapi_threads);
	vnx::read_config("endpoint", endpoint);

	if(!allow_remote) {
		endpoint = "localhost";
	}
	wapi_threads = std::max(std::min(wapi_threads, 256u), 1u);

	vnx::log_info() << "AVX2 support:   " << (avx2_available() ? "yes" : "no");
	vnx::log_info() << "SHA-NI support: " << (sha256_ni_available() ? "yes" : "no");
#ifdef __aarch64__
	vnx::log_info() << "ARM-SHA2 support: " << (sha256_arm_available() ? "yes" : "no");
#endif // __aarch64__

#ifdef WITH_CUDA
	vnx::log_info() << "CUDA available: yes";
	mmx::pos::cuda_recompute_init();
#else
	vnx::log_info() << "CUDA available: no";
#endif

	vnx::log_info() << "Remote service access is: " << (allow_remote ? "enabled on " + endpoint : "disabled");

	if(!with_wallet) {
		with_farmer = false;
	}
	if(!with_farmer) {
		with_harvester = false;
	}
	vnx::write_config("farmer", with_farmer);
	vnx::write_config("harvester", with_harvester);

	mmx::sync_type_codes(mmx_network + "type_codes");

	{
		vnx::Handle<vnx::Terminal> module = new vnx::Terminal("Terminal");
		module.start_detached();
	}
	std::vector<vnx::Hash64> wapi_instances;

	for(uint32_t i = 0; i < wapi_threads; ++i)
	{
		vnx::Handle<mmx::WebAPI> module = new mmx::WebAPI("WebAPI" + std::string(wapi_threads > 1 ? "_" + std::to_string(i) : ""));
		vnx::read_config("WebAPI.config_path", module->config_path);
		vnx::read_config("WebAPI.cache_max_age", module->cache_max_age);
		module->config_path = mmx_home + module->config_path;
		wapi_instances.push_back(module->vnx_get_id());
		module.start_detached();
	}
	if(wapi_instances.size() > 1) {
		vnx::Handle<vnx::addons::HttpBalancer> module = new vnx::addons::HttpBalancer("WebAPI");
		module->backend = wapi_instances;
		module.start_detached();
	}
	{
		// for remote farmer / timelord
		vnx::Handle<vnx::Server> module = new vnx::Server("Server0", vnx::Endpoint::from_url(endpoint + ":11330"));
		module->use_authentication = true;
		module->default_access = "REMOTE";
		module.start_detached();
	}
	if(with_wallet) {
		{
			vnx::Handle<mmx::Wallet> module = new mmx::Wallet("Wallet");
			module->config_path = mmx_home + module->config_path;
			module->storage_path = mmx_home + module->storage_path;
			module->database_path = mmx_network + module->database_path;
			module.start_detached();
		}
		{
			vnx::Handle<vnx::Server> module = new vnx::Server("Server5", vnx::Endpoint::from_url("localhost:11335"));
			module->use_authentication = true;
			module->default_access = "USER";
			module.start_detached();
		}
	}
	{
		vnx::Handle<vnx::addons::FileServer> module = new vnx::addons::FileServer("FileServer_1");
		module->www_root = "ui/dist/gui/";
		module->directory_files.push_back("index.html");
		module.start_detached();
	}
	int http_port = 0;
	std::string api_token_header;
	{
		vnx::Handle<vnx::addons::HttpServer> module = new vnx::addons::HttpServer("HttpServer");
		module->default_access = "NETWORK";
		module->components["/server/"] = "HttpServer";
		module->components["/wapi/"] = "WebAPI";
		module->components["/api/node/"] = "Node";
		module->components["/api/wallet/"] = "Wallet";
		module->components["/api/farmer/"] = "Farmer";
		module->components["/api/router/"] = "Router";
		module->components["/api/harvester/"] = "Harvester";
		module->components["/gui/"] = "FileServer_1";
		if(with_gui) {
			http_port = module->port;
			api_token_header = module->token_header_name;
			module->token_map[api_token] = "ADMIN";
		}
		module.start_detached();
	}
	if(with_timelord) {
		{
			vnx::Handle<mmx::TimeLord> module = new mmx::TimeLord("TimeLord");
			module->storage_path = mmx_network + module->storage_path;
			module.start_detached();
		}
	}
	if(with_farmer) {
		{
			vnx::Handle<mmx::Farmer> module = new mmx::Farmer("Farmer");
			module.start_detached();
		}
		{
			vnx::Handle<mmx::ProofServer> module = new mmx::ProofServer("ProofServer");
			module.start_detached();
		}
		{
			// for remote harvesters
			vnx::Handle<vnx::Server> module = new vnx::Server("Server3", vnx::Endpoint::from_url(endpoint + ":11333"));
			module->use_authentication = true;
			module->default_access = "REMOTE";
			module.start_detached();
		}
	}
	if(with_harvester) {
		vnx::Handle<mmx::Harvester> module = new mmx::Harvester("Harvester");
		module->config_path = mmx_home + module->config_path;
		module->storage_path = mmx_network + module->storage_path;
		module.start_detached();
	}
	{
		vnx::Handle<mmx::Node> module = new mmx::Node("Node");
		module->storage_path = mmx_network + module->storage_path;
		module->database_path = mmx_network + module->database_path;
		module.start_detached();
	}

	if(with_gui) {
		const auto host = "localhost:" + std::to_string(http_port);
#ifdef WITH_QT
		qt_gui_exec(argv, host, api_token, api_token_header);
#else
		vnx::log_warn() << "No GUI available";
		vnx::wait();
#endif
	} else {
		vnx::wait();
	}

	vnx::close();

#ifdef WITH_CUDA
	mmx::pos::cuda_recompute_shutdown();
#endif

	mmx::secp256k1_free();

	return 0;
}


