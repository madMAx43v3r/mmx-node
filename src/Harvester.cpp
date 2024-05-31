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
#include <mmx/ProofOfSpaceNFT.hxx>
#include <mmx/ProofOfStake.hxx>
#include <mmx/utils.h>
#include <vnx/vnx.h>


namespace mmx {

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

	threads = std::make_shared<vnx::ThreadPool>(num_threads, num_threads);
	lookup_timer = add_timer(std::bind(&Harvester::check_queue, this));

	set_timer_millis(10000, std::bind(&Harvester::update, this));

	if(reload_interval > 0) {
		set_timer_millis(int64_t(reload_interval) * 1000, std::bind(&Harvester::reload, this));
	}

	reload();

	Super::main();

	threads->close();
}

void Harvester::send_response(	std::shared_ptr<const Challenge> request, std::shared_ptr<const ProofOfSpace> proof,
								const uint32_t score, const int64_t time_begin_ms) const
{
	auto out = ProofResponse::create();
	out->proof = proof;
	out->request = request;
	out->farmer_addr = farmer_addr;
	out->harvester = host_name.substr(0, 256);
	out->lookup_time_ms = vnx::get_wall_time_millis() - time_begin_ms;

	const auto delay_sec = out->lookup_time_ms / 1e3;
	log(INFO) << "[" << host_name << "] Found proof with score " << score << " for height " << request->height << ", delay " << delay_sec << " sec";

	out->hash = out->calc_hash();
	out->content_hash = out->calc_hash(true);
	publish(out, output_proofs);
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
	if(value->max_delay < 2) {
		return;
	}
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
	struct lookup_job_t {
		std::mutex mutex;
		std::condition_variable signal;
		size_t total_plots = 0;
		uint32_t best_score = -1;
		pos::proof_data_t best_proof;
		std::shared_ptr<pos::Prover> best_plot;
		std::atomic<uint64_t> num_left {0};
		std::atomic<uint64_t> num_passed {0};
	};
	const auto job = std::make_shared<lookup_job_t>();
	job->total_plots = id_map.size();
	job->num_left = job->total_plots;

	const auto max_delay_sec = params->block_time * (double(value->max_delay) - 0.5);
	const auto deadline_ms = recv_time_ms + int64_t(max_delay_sec * 1000);

	for(const auto& entry : id_map)
	{
		const auto iter = plot_map.find(entry.second);
		if(iter == plot_map.end()) {
			job->num_left--;
			log(WARN) << "Cannot find plot " << entry.first.to_string();
			continue;
		}
		const auto& plot_id = entry.first;
		const auto& prover = iter->second;

		threads->add_task([this, plot_id, prover, value, job, recv_time_ms, deadline_ms]()
		{
			const bool passed = check_plot_filter(params, value->challenge, plot_id);
			const bool expired = vnx::get_wall_time_millis() > deadline_ms;

			if(passed && !expired) try {
				const auto challenge = get_plot_challenge(value->challenge, plot_id);
				const auto qualities = prover->get_qualities(challenge, params->plot_filter);

				for(const auto& res : qualities)
				{
					if(!res.valid) {
						log(WARN) << "[" << host_name << "] Failed to fetch quality: " << res.error_msg << " (" << prover->get_file_path() << ")";
						continue;
					}
					const auto score = calc_proof_score(params, prover->get_ksize(), res.quality, value->space_diff);
					if(score < params->score_threshold)
					{
						std::lock_guard<std::mutex> lock(job->mutex);

						if(score < job->best_score || !job->best_plot)
						{
							job->best_score = score;
							job->best_plot = prover;
							job->best_proof = res;
						}
					}
				}
			} catch(const std::exception& ex) {
				log(WARN) << "[" << host_name << "] Failed to process plot: " << ex.what() << " (" << prover->get_file_path() << ")";
			}
			if(passed && expired) {
				log(DEBUG) << "Skipping quality lookup for height " << value->height << " due to deadline overshoot";
			}
			{
				std::lock_guard<std::mutex> lock(job->mutex);
				if(passed) {
					job->num_passed++;
				}
				job->num_left--;
			}
			job->signal.notify_all();
		});
	}

	threads->add_task([this, value, job, max_delay_sec, recv_time_ms, deadline_ms]()
	{
		std::unique_lock<std::mutex> lock(job->mutex);
		while(job->num_left) {
			const int64_t timeout_ms = deadline_ms - vnx::get_wall_time_millis();
			if(job->signal.wait_for(lock, std::chrono::milliseconds(timeout_ms)) == std::cv_status::timeout) {
				log(WARN) << "[" << host_name << "] Lookup for height " << value->height << " took longer than allowable delay of " << max_delay_sec << " sec";
				break;
			}
		}
		if(auto prover = job->best_plot)
		{
			auto proof_xs = job->best_proof.proof;
			if(proof_xs.empty()) {
				try {
					const auto data = prover->get_full_proof(value->challenge, job->best_proof.index);
					if(data.valid) {
						proof_xs = data.proof;
					} else {
						throw std::runtime_error(data.error_msg);
					}
				} catch(const std::exception& ex) {
					log(WARN) << "[" << host_name << "] Failed to fetch full proof: " << ex.what() << " (" << prover->get_file_path() << ")";
				}
			}
			if(!proof_xs.empty()) {
				std::shared_ptr<ProofOfSpace> proof;

				const auto header = prover->get_header();
				if(header->contract) {
					auto out = std::make_shared<ProofOfSpaceNFT>();
					out->seed = header->seed;
					out->ksize = header->ksize;
					out->contract = *header->contract;
					out->proof_xs = proof_xs;
					proof = out;
				} else {
					auto out = std::make_shared<ProofOfSpaceOG>();
					out->seed = header->seed;
					out->ksize = header->ksize;
					out->proof_xs = proof_xs;
					proof = out;
				}
				proof->score = job->best_score;
				proof->plot_id = header->plot_id;
				proof->farmer_key = header->farmer_key;

				send_response(value, proof, job->best_score, recv_time_ms);
			}
		}
		if(job->total_plots) {
			const auto delay_sec = (vnx::get_wall_time_millis() - recv_time_ms) / 1e3;
			log(INFO) << "[" << host_name << "] " << job->num_passed << " / " << job->total_plots
					<< " plots were eligible for height " << value->height << ", delay " << delay_sec << " sec";
		}
	});

	for(const auto& entry : virtual_map)
	{
		if(check_plot_filter(params, value->challenge, entry.first)) {
			try {
				const auto balance = node->get_virtual_plot_balance(entry.first, value->diff_block_hash);
				if(balance) {
					const auto score = calc_virtual_score(params, value->challenge, entry.first, balance, value->space_diff);
					if(score < params->score_threshold) {
						auto proof = std::make_shared<ProofOfStake>();
						proof->farmer_key = entry.second.farmer_key;
						proof->plot_id = entry.first;
						proof->score = score;
						send_response(value, proof, score, recv_time_ms);
					}
				}
			} catch(const std::exception& ex) {
				log(WARN) << "[" << host_name << "] Failed to check virtual plot: " << ex.what() << " (" << entry.first << ")";
			}
		}
	}
	const auto delay_sec = (vnx::get_wall_time_millis() - recv_time_ms) / 1e3;
	if(delay_sec > max_delay_sec) {
		log(WARN) << "[" << host_name << "] Virtual plots check for height " << value->height << " took longer than allowable delay: " << delay_sec << " sec";
	}

	lookup_timer->set_millis(0);
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
	info->total_bytes_effective = total_bytes_effective;
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
					if(!dir_blacklist.count(name)) {
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
	std::vector<std::string> plot_files;

	for(const auto& dir_path : dir_list)
	{
		threads->add_task([this, dir_path, &plot_files, &missing, &mutex]()
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
					if(!plot_map.count(file_name) && file->get_extension() == ".plot")
					{
						std::lock_guard<std::mutex> lock(mutex);
						plot_files.push_back(file_name);
					}
				}
			} catch(const std::exception& ex) {
				log(WARN) << "[" << host_name << "] " << ex.what();
			}
		});
	}
	threads->sync();

	std::vector<std::pair<std::string, std::shared_ptr<pos::Prover>>> plots;

	for(const auto& file_path : plot_files)
	{
		threads->add_task([this, file_path, &plots, &mutex]()
		{
			try {
				auto prover = std::make_shared<pos::Prover>(file_path);
				{
					std::lock_guard<std::mutex> lock(mutex);
					plots.emplace_back(file_path, prover);
				}
			} catch(const std::exception& ex) {
				log(WARN) << "[" << host_name << "] Failed to load plot '" << file_path << "' due to: " << ex.what();
			} catch(...) {
				log(WARN) << "[" << host_name << "] Failed to load plot '" << file_path << "'";
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

	std::set<pubkey_t> farmer_keys;
	for(const auto& key : farmer->get_farmer_keys()) {
		farmer_keys.insert(key);
	}

	// validate and add new plots
	for(const auto& entry : plots)
	{
		const auto& plot = entry.second;
		try {
			const auto& farmer_key = plot->get_farmer_key();
			if(!farmer_keys.count(farmer_key)) {
				throw std::logic_error("unknown farmer key: " + farmer_key.to_string());
			}
			plot_map.insert(entry);
		}
		catch(const std::exception& ex) {
			log(WARN) << "[" << host_name << "] Invalid plot: " << entry.first << " (" << ex.what() << ")";
		}
	}

	id_map.clear();
	total_bytes = 0;
	total_bytes_effective = 0;
	for(const auto& entry : plot_map) {
		const auto& prover = entry.second;
		const auto& file_name = entry.first;
		const auto& plot_id = prover->get_plot_id();

		if(!id_map.emplace(plot_id, file_name).second) {
			log(WARN) << "[" << host_name << "] Duplicate plot: " << entry.first << " (already have: " << id_map[plot_id] << ")";
		}
		total_bytes += vnx::File(file_name).file_size();
		total_bytes_effective += get_effective_plot_size(prover->get_ksize());
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
	log(INFO) << "[" << host_name << "] Loaded " << plot_map.size() << " plots, "
			<< total_bytes / pow(1000, 4) << " TB, " << total_bytes_effective / pow(1000, 4) << " TBe, "
			<< virtual_map.size() << " virtual plots, " << total_balance / pow(10, params->decimals) << " MMX total, took "
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
