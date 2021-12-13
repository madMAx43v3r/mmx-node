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

void Harvester::main()
{
	params = get_params();

	subscribe(input_challenges, 1000);

	set_timer_millis(1000, std::bind(&Harvester::update, this));
	set_timer_millis(int64_t(reload_interval) * 1000, std::bind(&Harvester::reload, this));

	if(self_test) {
		set_timer_millis(1000, std::bind(&Harvester::test_challenge, this));
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
	size_t num_passed = 0;
	uint128_t best_score = ~uint128_0;
	std::shared_ptr<chiapos::Proof> best_proof;

	for(const auto& entry : id_map)
	{
		if(check_plot_filter(params, value->challenge, entry.first)) {
			if(auto prover = plot_map[entry.second])
			{
				const auto qualities = prover->get_qualities(value->challenge.bytes);
				for(size_t i = 0; i < qualities.size(); ++i)
				{
					const auto score = calc_proof_score(
							params, prover->get_ksize(), hash_t::from_bytes(qualities[i]), value->space_diff);
					if(score < params->score_threshold) {
						if(score < best_score) {
							if(auto proof = prover->get_full_proof(value->challenge.bytes, i)) {
								best_proof = proof;
								log(INFO) << "Found proof with score " << score << " for height " << value->height;
							}
						}
					}
					if(score < best_score) {
						best_score = score;
					}
				}
			}
			num_passed++;
		}
	}
	if(best_proof) {
		auto out = ProofResponse::create();
		out->height = value->height;
		out->score = best_score;
		out->challenge = value->challenge;
		out->farmer_addr = farmer_addr;

		bls::AugSchemeMPL MPL;
		auto local_sk = bls::PrivateKey::FromBytes(bls::Bytes(best_proof->master_sk.data(), best_proof->master_sk.size()));

		for(uint32_t i : {12381, 11337, 3, 0}) {
			local_sk = MPL.DeriveChildSk(local_sk, i);
		}
		auto proof = ProofOfSpace::create();
		proof->ksize = best_proof->k;
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

	log(INFO) << num_passed << " plots were eligible for height " << value->height
			<< ", best score was " << (best_score != ~uint128_0 ? best_score.str() : "N/A");
}

uint32_t Harvester::get_plot_count() const
{
	return plot_map.size();
}

uint64_t Harvester::get_total_space() const
{
	return total_bytes;
}

void Harvester::reload()
{
	std::vector<std::pair<std::shared_ptr<vnx::File>, std::shared_ptr<chiapos::DiskProver>>> plots;

#pragma omp parallel for
	for(const auto& path : plot_dirs)
	{
		vnx::Directory dir(path);
		try {
			dir.open();
		} catch(const std::exception& ex) {
			log(WARN) << "Failed to open directory " << path << " due to: " << ex.what();
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
					log(WARN) << "Failed to load plot " << file->get_path() << " due to: " << ex.what();
				}
			}
		}
	}
	id_map.clear();
	plot_map.clear();
	total_bytes = 0;

	for(const auto& entry : plots)
	{
		const auto& file = entry.first;
		const auto& prover = entry.second;
		id_map[hash_t::from_bytes(prover->get_plot_id())] = file->get_path();
		plot_map[file->get_path()] = prover;
		total_bytes += file->file_size();
	}
	log(INFO) << "Loaded " << plot_map.size() << " plots, " << (total_bytes / 1024 / 1024) / pow(1024, 2) << " TiB total";
}

void Harvester::update()
{
	FarmerClient farmer(farmer_server);
	farmer_addr = farmer.get_mac_addr();
}

void Harvester::test_challenge()
{
	auto value = Challenge::create();
	const auto nonce = ::rand();
	value->challenge = hash_t(&nonce, sizeof(nonce));
	value->space_diff = params->initial_space_diff;
	handle(value);
}


} // mmx
