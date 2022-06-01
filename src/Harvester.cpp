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
	if(!num_threads) {
		num_threads = 16;
	}
	params = get_params();

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
	const auto time_begin = vnx::get_time_micros();

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
			log(WARN) << "Failed to fetch qualities: " << ex.what() << " (" << prover->get_file_path() << ")";
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
			log(WARN) << "Failed to fetch proof: " << ex.what() << " (" << best_plot->get_file_path() << ")";
		}
	}
	if(best_proof || best_vplot) {
		auto out = ProofResponse::create();
		out->request = value;
		out->farmer_addr = farmer_addr;

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
			// TODO: sign response hash
			// TODO: have node sign it after verify
			out->farmer_sig = farmer->sign_proof(out->proof);

			if(auto proof = std::dynamic_pointer_cast<const ProofOfStake>(out->proof)) {
				auto copy = vnx::clone(proof);
				copy->farmer_sig = out->farmer_sig;
				out->proof = copy;
			}
			publish(out, output_proofs);
		}
		catch(const std::exception& ex) {
			log(WARN) << "Failed to sign proof: " << ex.what();
		}
	}
	already_checked.insert(value->challenge);

	if(!id_map.empty()) {
		log(INFO) << plots.size() << " plots were eligible for height " << value->height
				<< ", best score was " << (best_score != uint256_max ? best_score.str() : "N/A")
				<< ", took " << (vnx::get_time_micros() - time_begin) / 1e6 << " sec";
	}
}

uint64_t Harvester::get_total_bytes() const
{
	return total_bytes;
}

std::shared_ptr<const FarmInfo> Harvester::get_farm_info() const
{
	auto info = FarmInfo::create();
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
			dir.open();
			for(const auto& file : dir.files()) {
				if(file && file->get_extension() == ".plot") {
					all_dirs.insert(path);
					break;
				}
			}
			for(const auto& sub_dir : dir.directories()) {
				sub_dirs.insert(sub_dir->get_path());
			}
		} catch(const std::exception& ex) {
			log(WARN) << ex.what();
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

#pragma omp parallel for num_threads(num_threads)
	for(int i = 0; i < int(dir_list.size()); ++i)
	{
		vnx::Directory dir(dir_list[i]);
		try {
			dir.open();
			for(const auto& file : dir.files())
			{
				const auto file_name = file->get_path();
#pragma omp critical
				missing.erase(file_name);

				if(plot_map.count(file_name)) {
					continue;
				}
				if(file->get_extension() == ".plot") {
					try {
						auto prover = std::make_shared<chiapos::DiskProver>(file_name);
#pragma omp critical
						plots.emplace_back(file_name, prover);
					}
					catch(const std::exception& ex) {
						log(WARN) << "Failed to load plot '" << file_name << "' due to: " << ex.what();
					}
				}
			}
		} catch(const std::exception& ex) {
			log(WARN) << ex.what();
			continue;
		}
	}

	// purge missing plots
	for(const auto& file_name : missing) {
		plot_map.erase(file_name);
	}

	if(missing.size()) {
		log(INFO) << "Lost " << missing.size() << " plots";
	}
	if(plots.size() && plot_map.size()) {
		log(INFO) << "Found " << plots.size() << " new plots";
	}

	// add new plots
	plot_map.insert(plots.begin(), plots.end());

	id_map.clear();
	total_bytes = 0;
	for(const auto& entry : plot_map) {
		const auto& prover = entry.second;
		const auto& file_name = entry.first;
		const auto plot_id = hash_t::from_bytes(prover->get_plot_id());
		id_map[plot_id] = file_name;
		total_bytes += vnx::File(file_name).file_size();
	}

	// gather virtual plots
	virtual_map.clear();
	total_balance = 0;
	for(const auto& farmer_key : farmer->get_farmer_keys()) {
		for(const auto& entry : node->get_virtual_plots_for(farmer_key)) {
			if(auto plot = std::dynamic_pointer_cast<const contract::VirtualPlot>(entry.second)) {
				auto& info = virtual_map[entry.first];
				info.farmer_key = farmer_key;
				info.balance = node->get_virtual_plot_balance(entry.first);
				total_balance += info.balance;
			}
		}
	}

	update();

	// check challenges again
	already_checked.clear();

	log(INFO) << "Loaded " << plot_map.size() << " plots, " << virtual_map.size() << " virtual plots, "
			<< total_bytes / pow(1000, 4) << " TB total, " << total_balance / pow(10, params->decimals) << " MMX total, took "
			<< (vnx::get_wall_time_millis() - time_begin) / 1e3 << " sec";
}

void Harvester::add_plot_dir(const std::string& path)
{
	plot_dirs.insert(path);
}

void Harvester::rem_plot_dir(const std::string& path)
{
	plot_dirs.erase(path);
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
