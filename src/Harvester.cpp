/*
 * Harvester.cpp
 *
 *  Created on: Dec 11, 2021
 *      Author: mad
 */

#include <mmx/Harvester.h>
#include <mmx/FarmerClient.hxx>
#include <mmx/ProofResponse.hxx>
#include <mmx/utils.h>

#include <bls.hpp>


namespace mmx {

Harvester::Harvester(const std::string& _vnx_name)
	:	HarvesterBase(_vnx_name)
{
}

void Harvester::init()
{
	vnx::open_pipe(vnx_name, this, max_queue_ms);
}

void Harvester::main()
{
	if(!num_threads) {
		num_threads = std::max<size_t>(plot_dirs.size(), 1);
	}
	params = get_params();

	subscribe(input_challenges, max_queue_ms);

	set_timer_millis(10000, std::bind(&Harvester::update, this));

	if(reload_interval > 0) {
		set_timer_millis(int64_t(reload_interval) * 1000, std::bind(&Harvester::reload, this));
	}

	update();
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
	uint128_t best_score = ~uint128_0;
	std::shared_ptr<chiapos::Proof> best_proof;
	std::shared_ptr<chiapos::DiskProver> best_plot;
	std::vector<std::shared_ptr<chiapos::DiskProver>> plots;

	for(const auto& entry : id_map)
	{
		if(check_plot_filter(params, value->challenge, entry.first)) {
			if(auto prover = plot_map[entry.second]) {
				plots.push_back(prover);
			}
		}
	}
	std::vector<std::vector<uint128_t>> scores(plots.size());

#pragma omp parallel for num_threads(num_threads)
	for(size_t i = 0; i < plots.size(); ++i)
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
	if(best_plot) {
		try {
			best_proof = best_plot->get_full_proof(value->challenge.bytes, best_index);
		} catch(const std::exception& ex) {
			log(WARN) << "Failed to fetch proof: " << ex.what() << " (" << best_plot->get_file_path() << ")";
		}
	}
	if(best_proof) {
		auto out = ProofResponse::create();
		out->request = value;
		out->score = best_score;
		out->farmer_addr = farmer_addr;

		bls::AugSchemeMPL MPL;
		auto local_sk = bls::PrivateKey::FromBytes(bls::Bytes(best_proof->master_sk.data(), best_proof->master_sk.size()));

		for(uint32_t i : {12381, 11337, 3, 0}) {
			local_sk = MPL.DeriveChildSk(local_sk, i);
		}
		auto proof = ProofOfSpace::create();
		proof->ksize = best_proof->k;
		proof->score = best_score;
		proof->plot_id = hash_t::from_bytes(best_proof->id);
		proof->proof_bytes = best_proof->proof;
		proof->local_key = local_sk.GetG1Element();
		proof->farmer_key = bls_pubkey_t::super_t(best_proof->farmer_key);
		proof->pool_key = bls_pubkey_t::super_t(best_proof->pool_key);
		proof->local_sig = bls_signature_t::sign(local_sk, proof->calc_hash());

		out->proof = proof;
		publish(out, output_proofs);
	}
	already_checked.insert(value->challenge);

	if(!id_map.empty()) {
		log(INFO) << plots.size() << " plots were eligible for height " << value->height
				<< ", best score was " << (best_score != ~uint128_0 ? best_score.str() : "N/A")
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
	info->plot_dirs = plot_dirs;
	info->total_bytes = total_bytes;
	for(const auto& entry : plot_map) {
		if(auto prover = entry.second) {
			info->plot_count[prover->get_ksize()]++;
		}
	}
	return info;
}

void Harvester::reload()
{
	std::vector<std::pair<std::shared_ptr<vnx::File>, std::shared_ptr<chiapos::DiskProver>>> plots;

#pragma omp parallel for num_threads(num_threads)
	for(size_t i = 0; i < plot_dirs.size(); ++i)
	{
		vnx::Directory dir(plot_dirs[i]);
		try {
			dir.open();
		} catch(const std::exception& ex) {
			log(WARN) << ex.what();
			continue;
		}
		for(const auto& file : dir.files()) {
			if(file && file->get_extension() == ".plot") {
				try {
					auto prover = std::make_shared<chiapos::DiskProver>(file->get_path());
#pragma omp critical
					plots.emplace_back(file, prover);
				}
				catch(const std::exception& ex) {
					log(WARN) << "Failed to load plot '" << file->get_path() << "' due to: " << ex.what();
				}
			}
		}
	}
	total_bytes = 0;
	id_map.clear();
	plot_map.clear();
	already_checked.clear();

	for(const auto& entry : plots)
	{
		const auto& file = entry.first;
		const auto& prover = entry.second;
		const auto file_name = file->get_path();
		const auto plot_id = hash_t::from_bytes(prover->get_plot_id());
		id_map[plot_id] = file_name;
		plot_map[file_name] = prover;
		total_bytes += file->file_size();
	}
	log(INFO) << "Loaded " << plot_map.size() << " plots, " << total_bytes / pow(1024, 4) << " TiB total";
}

void Harvester::update()
{
	try {
		FarmerClient farmer(farmer_server);
		farmer_addr = farmer.get_mac_addr();
	}
	catch(const std::exception& ex) {
		log(WARN) << "Failed to contact farmer: " << ex.what();
	}
}


} // mmx
