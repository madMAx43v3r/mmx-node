/*
 * Harvester.cpp
 *
 *  Created on: Dec 11, 2021
 *      Author: mad
 */

#include <mmx/Harvester.h>
#include <mmx/utils.h>

#include <vnx/Config.hpp>


namespace mmx {

Harvester::Harvester(const std::string& _vnx_name)
	:	HarvesterBase(_vnx_name)
{
}

void Harvester::main()
{
	{
		auto tmp = ChainParams::create();
		vnx::read_config("chain.params", tmp);
		params = tmp;
	}
	update();

	set_timer_millis(int64_t(reload_interval) * 1000, std::bind(&Harvester::update, this));

	if(self_test) {
		set_timer_millis(1000, std::bind(&Harvester::test_challenge, this));
	}

	Super::main();
}

void Harvester::handle(std::shared_ptr<const Challenge> value)
{
	size_t num_passed = 0;
	uint128_t best_score = -1;
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
	log(INFO) << num_passed << " plots were eligible for height " << value->height
			<< ", best score was " << (best_score != -1 ? best_score.str() : "N/A");
}

uint32_t Harvester::get_plot_count() const
{
	return plot_map.size();
}

uint64_t Harvester::get_total_space() const
{
	return total_bytes;
}

void Harvester::update()
{
	id_map.clear();
	plot_map.clear();
	total_bytes = 0;

	for(const auto& path : plot_dirs) {
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
					id_map[hash_t::from_bytes(prover->get_plot_id())] = file->get_path();
					plot_map[file->get_path()] = prover;
					total_bytes += file->file_size();
				}
				catch(const std::exception& ex) {
					log(WARN) << "Failed to load plot " << file->get_path() << " due to: " << ex.what();
				}
			}
		}
	}
	log(INFO) << "Loaded " << plot_map.size() << " plots, " << (total_bytes / 1024 / 1024) / pow(1024, 2) << " TiB total";
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
