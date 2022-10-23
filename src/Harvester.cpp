/*
 * Harvester.cpp
 *
 *  Created on: Dec 11, 2021
 *      Author: mad
 */

#include <mmx/Harvester.h>
#include <mmx/FarmerClient.hxx>
#include <mmx/ProofResponse.hxx>
#include <mmx/ProofOfSpaceOG.hxx>
#include <mmx/ProofOfStake.hxx>
#include <mmx/utils.h>

#include <bls.hpp>
#include <vnx/vnx.h>


namespace mmx {

static bls::PrivateKey derive_local_key(const std::vector<uint8_t>& master_sk)
{
	bls::AugSchemeMPL MPL;
	auto local_sk = bls::PrivateKey::FromBytes(bls::Bytes(master_sk.data(), master_sk.size()));

	for(uint32_t i : {12381, 11337, 3, 0}) {
		local_sk = MPL.DeriveChildSk(local_sk, i);
	}
	return local_sk;
}

Harvester::Harvester(const std::string& _vnx_name)
	:	HarvesterBase(_vnx_name)
{
}

void Harvester::init()
{
	vnx::open_pipe(vnx_name, this, max_queue_ms);

	subscribe(input_challenges, max_queue_ms);
}

void Harvester::main()
{
	params = get_params();
	host_name = vnx::get_host_name();
	{
		vnx::File file(storage_path + "harvester_id.dat");
		if(file.exists()) {
			try {
				file.open("rb");
				vnx::read_generic(file.in, harvester_id);
				file.close();
			} catch(...) {
				// ignore
			}
		}
		if(harvester_id == hash_t()) {
			harvester_id = hash_t::random();
			try {
				file.open("wb");
				vnx::write_generic(file.out, harvester_id);
				file.close();
			} catch(const std::exception& ex) {
				log(WARN) << "Failed to write " << file.get_path() << ": " << ex.what();
			}
		}
	}
	node = std::make_shared<NodeClient>(node_server);
	farmer = std::make_shared<FarmerClient>(farmer_server);

	http = std::make_shared<vnx::addons::HttpInterface<Harvester>>(this, vnx_name);
	add_async_client(http);

	set_timer_millis(10000, std::bind(&Harvester::update, this));

	if(reload_interval > 0) {
		set_timer_millis(int64_t(reload_interval) * 1000, std::bind(&Harvester::reload, this));
	}

	reload();

	Super::main();
}

void Harvester::handle(std::shared_ptr<const Challenge> value)
{
	if(already_checked.count(value->challenge)) {
		return;
	}
	const auto time_begin = vnx::get_wall_time_millis();

	uint32_t best_index = 0;
	uint256_t best_score = uint256_max;
	std::shared_ptr<chiapos::Proof> best_proof;
	std::shared_ptr<chiapos::DiskProver> best_plot;
	vnx::optional<std::pair<addr_t, virtual_plot_t>> best_vplot;

	std::vector<std::shared_ptr<chiapos::DiskProver>> plots;
	std::vector<std::pair<addr_t, virtual_plot_t>> virtual_plots;

	for(const auto& entry : id_map)
	{
		if(check_plot_filter(params, value->challenge, entry.first)) {
			if(auto prover = plot_map[entry.second]) {
				plots.push_back(prover);
			}
		}
	}
	for(const auto& entry : virtual_map)
	{
		if(entry.second.balance) {
			if(check_plot_filter(params, value->challenge, entry.first)) {
				virtual_plots.push_back(entry);
			}
		}
	}
	std::vector<std::vector<uint256_t>> scores(plots.size());

	const auto num_threads_ = std::min<uint32_t>(num_threads, plots.size());
#pragma omp parallel for num_threads(num_threads_)
	for(int i = 0; i < int(plots.size()); ++i)
	{
		const auto& prover = plots[i];
		try {
			const auto qualities = prover->get_qualities(value->challenge.bytes);
			scores[i].resize(qualities.size());

			for(size_t k = 0; k < qualities.size(); ++k) {
				scores[i][k] = calc_proof_score(params, prover->get_ksize(), hash_t::from_bytes(qualities[k]), value->space_diff);
			}
		} catch(const std::exception& ex) {
			log(WARN) << "[" << host_name << "] Failed to fetch qualities: " << ex.what() << " (" << prover->get_file_path() << ")";
		}
	}

	for(size_t i = 0; i < plots.size(); ++i)
	{
		for(size_t k = 0; k < scores[i].size(); ++k)
		{
			const auto& score = scores[i][k];
			if(score < params->score_threshold) {
				if(score < best_score) {
					best_plot = plots[i];
					best_index = k;
				}
			}
			if(score < best_score) {
				best_score = score;
			}
		}
	}

	for(const auto& entry : virtual_plots)
	{
		const auto score = calc_virtual_score(params, value->challenge, entry.first, entry.second.balance, value->space_diff);
		if(score < params->score_threshold) {
			if(score < best_score) {
				best_plot = nullptr;
				best_vplot = entry;
			}
		}
		if(score < best_score) {
			best_score = score;
		}
	}

	if(best_plot) {
		try {
			best_proof = best_plot->get_full_proof(value->challenge.bytes, best_index);
		} catch(const std::exception& ex) {
			log(WARN) << "[" << host_name << "] Failed to fetch proof: " << ex.what() << " (" << best_plot->get_file_path() << ")";
		}
	}
	const auto time_ms = vnx::get_wall_time_millis() - time_begin;

	if(best_proof || best_vplot) {
		auto out = ProofResponse::create();
		out->request = value;
		out->farmer_addr = farmer_addr;
		out->harvester = host_name;
		out->lookup_time_ms = time_ms;

		if(best_proof) {
			const auto local_sk = derive_local_key(best_proof->master_sk);

			auto proof = ProofOfSpaceOG::create();
			proof->ksize = best_proof->k;
			proof->score = best_score;
			proof->plot_id = hash_t::from_bytes(best_proof->id);
			proof->proof_bytes = best_proof->proof;
			proof->local_key = local_sk.GetG1Element();
			proof->farmer_key = bls_pubkey_t::super_t(best_proof->farmer_key);
			proof->pool_key = bls_pubkey_t::super_t(best_proof->pool_key);
			proof->local_sig = bls_signature_t::sign(local_sk, proof->calc_hash());
			out->proof = proof;
		}
		else if(best_vplot) {
			auto proof = ProofOfStake::create();
			proof->score = best_score;
			proof->plot_id = best_vplot->first;
			proof->contract = best_vplot->first;
			proof->farmer_key = best_vplot->second.farmer_key;
			out->proof = proof;
		}

		try {
			out->hash = out->calc_hash();
			// TODO: have node sign it after verify
			out->farmer_sig = farmer->sign_proof(out);
			out->content_hash = out->calc_hash(true);
			publish(out, output_proofs);
		}
		catch(const std::exception& ex) {
			log(WARN) << "Failed to sign proof: " << ex.what();
		}
	}
	already_checked.insert(value->challenge);

	if(!id_map.empty()) {
		log(INFO) << "[" << host_name << "] " << plots.size() << " plots were eligible for height " << value->height
				<< ", best score was " << (best_score != uint256_max ? best_score.str() : "N/A")
				<< ", took " << time_ms / 1e3 << " sec";
	}
}

uint64_t Harvester::get_total_bytes() const
{
	return total_bytes;
}

std::shared_ptr<const FarmInfo> Harvester::get_farm_info() const
{
	auto info = FarmInfo::create();
	info->harvester = host_name;
	info->harvester_id = harvester_id;
	info->plot_dirs = std::vector<std::string>(plot_dirs.begin(), plot_dirs.end());
	info->total_bytes = total_bytes;
	for(const auto& entry : plot_map) {
		if(auto prover = entry.second) {
			info->plot_count[prover->get_ksize()]++;
		}
	}
	for(const auto& entry : virtual_map) {
		info->total_balance += entry.second.balance;
	}
	return info;
}

void Harvester::find_plot_dirs(const std::set<std::string>& dirs, std::set<std::string>& all_dirs) const
{
	std::set<std::string> sub_dirs;
	for(const auto& path : dirs) {
		vnx::Directory dir(path);
		try {
			for(const auto& file : dir.files()) {
				if(file && file->get_extension() == ".plot") {
					all_dirs.insert(path);
					break;
				}
			}
			for(const auto& sub_dir : dir.directories()) {
				const auto name = sub_dir->get_name();
				if(name != "System Volume Information" && name != "$RECYCLE.BIN") {
					sub_dirs.insert(sub_dir->get_path());
				}
			}
		} catch(const std::exception& ex) {
			log(WARN) << "[" << host_name << "] " << ex.what();
		}
	}
	if(!sub_dirs.empty()) {
		find_plot_dirs(sub_dirs, all_dirs);
	}
}

void Harvester::reload()
{
	const auto time_begin = vnx::get_wall_time_millis();

	std::set<std::string> dir_set;
	if(recursive_search) {
		find_plot_dirs(plot_dirs, dir_set);
	} else {
		dir_set = plot_dirs;
	}
	const std::vector<std::string> dir_list(dir_set.begin(), dir_set.end());

	std::set<std::string> missing;
	for(const auto& entry : plot_map) {
		missing.insert(entry.first);
	}
	std::vector<std::pair<std::string, std::shared_ptr<chiapos::DiskProver>>> plots;

	const auto num_threads_ = std::min<uint32_t>(num_threads, dir_list.size());
#pragma omp parallel for num_threads(num_threads_)
	for(int i = 0; i < int(dir_list.size()); ++i) {
		try {
			vnx::Directory dir(dir_list[i]);

			for(const auto& file : dir.files())
			{
				const auto file_name = file->get_path();
#pragma omp critical
				missing.erase(file_name);

				if(!plot_map.count(file_name) && file->get_extension() == ".plot") {
					try {
						auto prover = std::make_shared<chiapos::DiskProver>(file_name);
#pragma omp critical
						plots.emplace_back(file_name, prover);
					}
					catch(const std::exception& ex) {
						log(WARN) << "[" << host_name << "] Failed to load plot '" << file_name << "' due to: " << ex.what();
					} catch(...) {
						log(WARN) << "[" << host_name << "] Failed to load plot '" << file_name << "'";
					}
				}
			}
		} catch(const std::exception& ex) {
			log(WARN) << "[" << host_name << "] " << ex.what();
		}
	}

	// purge missing plots
	for(const auto& file_name : missing) {
		plot_map.erase(file_name);
	}

	if(missing.size()) {
		log(INFO) << "[" << host_name << "] Lost " << missing.size() << " plots";
	}
	if(plots.size() && plot_map.size()) {
		log(INFO) << "[" << host_name << "] Found " << plots.size() << " new plots";
	}

	std::set<bls_pubkey_t> farmer_keys;
	for(const auto& key : farmer->get_farmer_keys()) {
		farmer_keys.insert(key);
	}

	// validate and add new plots
#pragma omp parallel for
	for(int i = 0; i < int(plots.size()); ++i)
	{
		const auto& entry = plots[i];
		const auto& plot = entry.second;
		try {
			const bls_pubkey_t farmer_key = bls_pubkey_t::super_t(plot->get_farmer_key());
			if(!farmer_keys.count(farmer_key)) {
				throw std::logic_error("unknown farmer key: " + farmer_key.to_string());
			}
			const auto plot_id = hash_t::from_bytes(plot->get_plot_id());
			const auto pool_key = plot->get_pool_key();
			const auto local_sk = derive_local_key(plot->get_master_skey());
			const auto local_key = local_sk.GetG1Element();

			const bls_pubkey_t plot_key = local_key + farmer_key.to_bls();
			if(!pool_key.empty()) {
				const uint32_t port = 11337;
				const bls_pubkey_t pool_key_ = bls_pubkey_t::super_t(pool_key);
				if(hash_t(hash_t(pool_key_ + plot_key) + bytes_t<4>(&port, 4)) != plot_id) {
					throw std::logic_error("invalid keys or port");
				}
			} else {
				throw std::logic_error("invalid plot type");
			}
#pragma omp critical
			plot_map.insert(entry);
		}
		catch(const std::exception& ex) {
			log(WARN) << "[" << host_name << "] Invalid plot: " << entry.first << " (" << ex.what() << ")";
		} catch(...) {
			log(WARN) << "[" << host_name << "] Invalid plot: " << entry.first;
		}
	}

	id_map.clear();
	total_bytes = 0;
	for(const auto& entry : plot_map) {
		const auto& prover = entry.second;
		const auto& file_name = entry.first;
		const auto plot_id = hash_t::from_bytes(prover->get_plot_id());

		if(!id_map.emplace(plot_id, file_name).second) {
			log(WARN) << "[" << host_name << "] Duplicate plot: " << entry.first;
		}
		total_bytes += vnx::File(file_name).file_size();
	}

	// gather virtual plots
	virtual_map.clear();
	total_balance = 0;
	if(farm_virtual_plots) {
		for(const auto& farmer_key : farmer_keys) {
			for(const auto& entry : node->get_virtual_plots_for(farmer_key)) {
				if(auto plot = std::dynamic_pointer_cast<const contract::VirtualPlot>(entry.second)) {
					auto& info = virtual_map[entry.first];
					info.farmer_key = farmer_key;
					info.balance = node->get_virtual_plot_balance(entry.first);
					total_balance += info.balance;
				}
			}
		}
	}

	update();

	// check challenges again
	already_checked.clear();

	log(INFO) << "[" << host_name << "] Loaded " << plot_map.size() << " plots, " << virtual_map.size() << " virtual plots, "
			<< total_bytes / pow(1000, 4) << " TB total, " << total_balance / pow(10, params->decimals) << " MMX total, took "
			<< (vnx::get_wall_time_millis() - time_begin) / 1e3 << " sec";
}

void Harvester::add_plot_dir(const std::string& path)
{
	const std::string cpath = config_path + vnx_name + ".json";
	auto object = vnx::read_config_file(cpath);
	{
		auto& var = object["plot_dirs"];
		auto tmp = var.to<std::set<std::string>>();
		tmp.insert(path);
		var = tmp;
		vnx::write_config(vnx_name + ".plot_dirs", tmp);
	}
	vnx::write_config_file(cpath, object);

	plot_dirs.insert(path);
	reload();
}

void Harvester::rem_plot_dir(const std::string& path)
{
	const std::string cpath = config_path + vnx_name + ".json";
	auto object = vnx::read_config_file(cpath);
	{
		auto& var = object["plot_dirs"];
		auto tmp = var.to<std::set<std::string>>();
		tmp.erase(path);
		var = tmp;
		vnx::write_config(vnx_name + ".plot_dirs", tmp);
	}
	vnx::write_config_file(cpath, object);

	plot_dirs.erase(path);
	reload();
}

void Harvester::update()
{
	try {
		farmer_addr = farmer->get_mac_addr();
	}
	catch(const std::exception& ex) {
		log(WARN) << "Failed to contact farmer: " << ex.what();
	}
	publish(get_farm_info(), output_info);
}

void Harvester::http_request_async(	std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
									const vnx::request_id_t& request_id) const
{
	http->http_request(request, sub_path, request_id, vnx_request->session);
}

void Harvester::http_request_chunk_async(	std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
											const int64_t& offset, const int64_t& max_bytes, const vnx::request_id_t& request_id) const
{
	throw std::logic_error("not implemented");
}


} // mmx
