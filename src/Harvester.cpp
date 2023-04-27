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

	threads = std::make_shared<vnx::ThreadPool>(num_threads);
	lookup_timer = add_timer(std::bind(&Harvester::check_queue, this));

	set_timer_millis(10000, std::bind(&Harvester::update, this));

	if(reload_interval > 0) {
		set_timer_millis(int64_t(reload_interval) * 1000, std::bind(&Harvester::reload, this));
	}

	reload();

	Super::main();

	threads->close();
}

void Harvester::send_response(	std::shared_ptr<const Challenge> request, std::shared_ptr<const mmx::chiapos::Proof> chia_proof,
								const virtual_plot_info_t* virtual_plot, const hash_t& plot_id, const uint32_t score, const int64_t time_begin_ms) const
{
	auto out = ProofResponse::create();
	out->request = request;
	out->farmer_addr = farmer_addr;
	out->harvester = host_name;
	out->lookup_time_ms = vnx::get_wall_time_millis() - time_begin_ms;

	vnx::optional<skey_t> local_sk;

	if(chia_proof) {
		const auto local_sk_bls = derive_local_key(chia_proof->master_sk);
		const auto local_key_bls = local_sk_bls.GetG1Element();
		local_sk = skey_t(local_sk_bls);

		auto proof = ProofOfSpaceOG::create();
		proof->ksize = chia_proof->k;
		proof->score = score;
		proof->plot_id = plot_id;
		proof->proof_bytes = chia_proof->proof;
		proof->farmer_key = bls_pubkey_t(chia_proof->farmer_key);
		proof->plot_key = local_key_bls + proof->farmer_key.to_bls();
		proof->pool_key = bls_pubkey_t(chia_proof->pool_key);
		proof->local_key = local_key_bls;
		out->proof = proof;
	}
	else if(virtual_plot) {
		auto proof = ProofOfStake::create();
		proof->score = score;
		proof->plot_id = plot_id;
		proof->contract = plot_id;
		proof->farmer_key = virtual_plot->farmer_key;
		proof->plot_key = virtual_plot->farmer_key;
		out->proof = proof;
	}

	try {
		out->hash = out->calc_hash();
		out->content_hash = out->calc_hash(true);
		// TODO: have node sign it after verify
		out->farmer_sig = farmer->sign_proof(out, local_sk);
		out->content_hash = out->calc_hash(true);
		publish(out, output_proofs);
	}
	catch(const std::exception& ex) {
		log(WARN) << "Failed to sign proof: " << ex.what();
	}
}

void Harvester::check_queue()
{
	const auto now_ms = vnx::get_wall_time_millis();

	for(auto iter = lookup_queue.begin(); iter != lookup_queue.end();)
	{
		const auto& entry = iter->second;
		const auto delay_sec = (now_ms - entry.recv_time_ms) / 1e3;

		if(delay_sec > params->block_time * entry.request->max_delay) {
			log(WARN) << "[" << host_name << "] Missed deadline for height " << entry.request->height << " due to delay of " << delay_sec << " sec";
			iter = lookup_queue.erase(iter);
		} else {
			iter++;
		}
	}
	if(!lookup_queue.empty())
	{
		const auto iter = --lookup_queue.end();
		const auto& entry = iter->second;
		lookup_task(entry.request, entry.recv_time_ms);
		lookup_queue.erase(iter);
	}
}

void Harvester::handle(std::shared_ptr<const Challenge> value)
{
	if(!already_checked.insert(value->challenge).second) {
		return;
	}
	auto& entry = lookup_queue[value->height];
	entry.recv_time_ms = vnx_sample->recv_time / 1000;
	entry.request = value;

	lookup_timer->set_millis(10);
}

void Harvester::lookup_task(std::shared_ptr<const Challenge> value, const int64_t recv_time_ms) const
{
	const auto time_begin = vnx::get_wall_time_millis();

	std::mutex mutex;
	uint256_t best_score = uint256_max;
	std::atomic<uint64_t> num_passed {0};

	for(const auto& entry : virtual_map)
	{
		threads->add_task([this, entry, value, time_begin, &best_score, &num_passed, &mutex]()
		{
			if(check_plot_filter(params, value->challenge, entry.first)) {
				try {
					std::lock_guard<std::mutex> lock(mutex);
					const auto balance = node->get_virtual_plot_balance(entry.first, value->diff_block_hash);
					if(balance) {
						const auto score = calc_virtual_score(params, value->challenge, entry.first, balance, value->space_diff);
						if(score < params->score_threshold) {
							send_response(value, nullptr, &entry.second, entry.first, score, time_begin);
						}
						best_score = std::min(best_score, score);
					}
				} catch(const std::exception& ex) {
					log(WARN) << "[" << host_name << "] Failed to check virtual plot: " << ex.what() << " (" << entry.first << ")";
				}
				num_passed++;
			}
		});
	}

	struct {
		hash_t plot_id;
		hash_t challenge;
		uint32_t index = 0;
		uint256_t score = uint256_max;
		std::shared_ptr<chiapos::DiskProver> prover;
	} best_entry;

	for(const auto& entry : id_map)
	{
		threads->add_task([this, entry, value, &best_entry, &best_score, &num_passed, &mutex]()
		{
			if(check_plot_filter(params, value->challenge, entry.first))
			{
				const auto iter = plot_map.find(entry.second);
				if(iter != plot_map.end()) {
					const auto& prover = iter->second;
					try {
						const auto challenge = get_plot_challenge(value->challenge, entry.first);
						const auto qualities = prover->get_qualities(challenge.bytes);

						std::lock_guard<std::mutex> lock(mutex);

						for(size_t k = 0; k < qualities.size(); ++k) {
							const auto score = calc_proof_score(params, prover->get_ksize(), hash_t::from_bytes(qualities[k]), value->space_diff);
							if(score < params->score_threshold) {
								if(score < best_entry.score) {
									best_entry.index = k;
									best_entry.score = score;
									best_entry.plot_id = entry.first;
									best_entry.challenge = challenge;
									best_entry.prover = prover;
								}
							}
							best_score = std::min(best_score, score);
						}
					} catch(const std::exception& ex) {
						log(WARN) << "[" << host_name << "] Failed to fetch qualities: " << ex.what() << " (" << prover->get_file_path() << ")";
					}
				}
				num_passed++;
			}
		});
	}
	threads->sync();

	if(auto prover = best_entry.prover) {
		try {
			const auto proof = prover->get_full_proof(best_entry.challenge.bytes, best_entry.index);
			send_response(value, proof, nullptr, best_entry.plot_id, best_entry.score, time_begin);
		} catch(const std::exception& ex) {
			log(WARN) << "[" << host_name << "] Failed to fetch full proof: " << ex.what() << " (" << prover->get_file_path() << ")";
		}
	}

	const auto now_ms = vnx::get_wall_time_millis();
	const auto time_sec = (now_ms - time_begin) / 1e3;
	const auto delay_sec = (now_ms - recv_time_ms) / 1e3;

	if(delay_sec > params->block_time * value->max_delay) {
		log(WARN) << "[" << host_name << "] Lookup for height " << value->height << " took longer than allowable delay: " << delay_sec << " sec";
	}
	if(!id_map.empty()) {
		log(INFO) << "[" << host_name << "] " << num_passed << " plots were eligible for height " << value->height
				<< ", best score was " << (best_score != uint256_max ? best_score.str() : "N/A")
				<< ", took " << time_sec << " sec, delay " << delay_sec << " sec";
	}
	lookup_timer->set_millis(10);
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
				const auto path = sub_dir->get_path();
				if(!all_dirs.count(path)) {
					const auto name = sub_dir->get_name();
					if(name != "System Volume Information" && name != "$RECYCLE.BIN" && name != "lost+found") {
						sub_dirs.insert(sub_dir->get_path());
					}
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

	std::mutex mutex;
	std::set<std::string> missing;
	for(const auto& entry : plot_map) {
		missing.insert(entry.first);
	}
	std::vector<std::pair<std::string, std::shared_ptr<chiapos::DiskProver>>> plots;

	for(const auto& dir_path : dir_list)
	{
		threads->add_task([this, dir_path, &plots, &missing, &mutex]()
		{
			try {
				vnx::Directory dir(dir_path);

				for(const auto& file : dir.files())
				{
					const auto file_name = file->get_path();
					{
						std::lock_guard<std::mutex> lock(mutex);
						missing.erase(file_name);
					}
					if(!plot_map.count(file_name) && file->get_extension() == ".plot") {
						try {
							auto prover = std::make_shared<chiapos::DiskProver>(file_name);
							{
								std::lock_guard<std::mutex> lock(mutex);
								plots.emplace_back(file_name, prover);
							}
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
		});
	}
	threads->sync();

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
	for(const auto& entry : plots)
	{
		threads->add_task([this, entry, &farmer_keys, &mutex]()
		{
			const auto& plot = entry.second;
			try {
				const bls_pubkey_t farmer_key(plot->get_farmer_key());
				if(!farmer_keys.count(farmer_key)) {
					throw std::logic_error("unknown farmer key: " + farmer_key.to_string());
				}
				if(validate_plots) {
					const auto plot_id = hash_t::from_bytes(plot->get_plot_id());
					const auto local_sk = derive_local_key(plot->get_master_skey());
					const auto pool_key = plot->get_pool_key();

					const bls_pubkey_t plot_key = local_sk.GetG1Element() + farmer_key.to_bls();
					if(!pool_key.empty()) {
						const uint32_t port = 11337;
						if(hash_t(hash_t(bls_pubkey_t(pool_key) + plot_key) + bytes_t<4>(&port, 4)) != plot_id) {
							throw std::logic_error("invalid keys or port");
						}
					} else {
						throw std::logic_error("invalid plot type");
					}
				}
				{
					std::lock_guard<std::mutex> lock(mutex);
					plot_map.insert(entry);
				}
			}
			catch(const std::exception& ex) {
				log(WARN) << "[" << host_name << "] Invalid plot: " << entry.first << " (" << ex.what() << ")";
			} catch(...) {
				log(WARN) << "[" << host_name << "] Invalid plot: " << entry.first;
			}
		});
	}
	threads->sync();

	id_map.clear();
	total_bytes = 0;
	for(const auto& entry : plot_map) {
		const auto& prover = entry.second;
		const auto& file_name = entry.first;
		const auto plot_id = hash_t::from_bytes(prover->get_plot_id());

		if(!id_map.emplace(plot_id, file_name).second) {
			log(WARN) << "[" << host_name << "] Duplicate plot: " << entry.first << " (" << id_map[plot_id] << ")";
		}
		total_bytes += vnx::File(file_name).file_size();
	}

	// gather virtual plots
	virtual_map.clear();
	total_balance = 0;
	if(farm_virtual_plots) {
		for(const auto& farmer_key : farmer_keys) {
			for(const auto& plot : node->get_virtual_plots_for(farmer_key)) {
				virtual_map[plot.address] = plot;
				total_balance += plot.balance;
			}
		}
	}

	update();

	// check challenges again for new plots
	if(plots.size()) {
		already_checked.clear();
	}
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
