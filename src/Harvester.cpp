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
#include <mmx/LookupInfo.hxx>
#include <mmx/Partial.hxx>
#include <mmx/utils.h>
#include <mmx/pos/verify.h>
#include <vnx/vnx.h>


namespace mmx {

Harvester::Harvester(const std::string& _vnx_name)
	:	HarvesterBase(_vnx_name)
{
	if(my_name.empty()) {
		my_name = vnx::get_host_name();
	}
	params = get_params();
	harvester_id = hash_t::random();
}

void Harvester::init()
{
	vnx::open_pipe(vnx_name, this, max_queue_ms);

	subscribe(input_challenges, max_queue_ms);
}

void Harvester::main()
{
	farmer = std::make_shared<FarmerClient>(farmer_server);
	farmer_async = std::make_shared<FarmerAsyncClient>(farmer_server);
	farmer_async->vnx_set_non_blocking(true);
	node_async = std::make_shared<NodeAsyncClient>(node_server);
	node_async->vnx_set_non_blocking(true);

	http = std::make_shared<vnx::addons::HttpInterface<Harvester>>(this, vnx_name);
	add_async_client(http);
	add_async_client(node_async);
	add_async_client(farmer_async);

	threads = std::make_shared<vnx::ThreadPool>(num_threads, num_threads);
	lookup_timer = add_timer(std::bind(&Harvester::check_queue, this));

	set_timer_millis(10000, std::bind(&Harvester::update, this));
	set_timer_millis(int64_t(nft_query_interval) * 1000, std::bind(&Harvester::update_nfts, this));

	if(reload_interval > 0) {
		set_timer_millis(int64_t(reload_interval) * 1000, std::bind(&Harvester::reload, this));
	}

	reload();

	Super::main();

	threads->close();
}

void Harvester::send_response(	std::shared_ptr<const Challenge> request, std::shared_ptr<const ProofOfSpace> proof,
								const int64_t time_begin_ms) const
{
	// Note: NEEDS TO BE THREAD SAFE
	auto out = ProofResponse::create();
	out->proof = proof;
	out->vdf_height = request->vdf_height;
	out->farmer_addr = farmer_addr;
	out->harvester = my_name.substr(0, 1024);
	out->lookup_time_ms = get_time_ms() - time_begin_ms;
	out->hash = out->calc_hash();
	out->content_hash = out->calc_content_hash();

	const auto delay_sec = out->lookup_time_ms / 1e3;
	log(INFO) << "[" << my_name << "] Found proof with score " << proof->score << " for height "
			<< request->vdf_height << ", delay " << delay_sec << " sec";

	publish(out, output_proofs);
}

void Harvester::check_queue()
{
	const auto now_ms = get_time_ms();

	for(auto iter = lookup_queue.begin(); iter != lookup_queue.end();)
	{
		const auto& entry = iter->second;
		const auto delay_ms = now_ms - entry.recv_time_ms;

		if(delay_ms > params->challenge_delay * params->block_interval_ms) {
			log(WARN) << "[" << my_name << "] Missed deadline for height " << entry.request->vdf_height;
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
	if(!is_ready) {
		return;
	}
	if(!already_checked.insert(value->challenge).second) {
		return;
	}
	auto& entry = lookup_queue[value->vdf_height];

	const auto prev = entry.request;
	if(!prev || prev->challenge != value->challenge) {
		entry.request = value;
		entry.recv_time_ms = vnx_sample->recv_time / 1000;

		// trigger first lookup if no new challenge received for 10 ms
		lookup_timer->set_millis(10);
	}
}

std::vector<uint32_t> Harvester::fetch_full_proof(
		std::shared_ptr<pos::Prover> prover, const hash_t& challenge, const uint64_t index) const
{
	// Note: NEEDS TO BE THREAD SAFE
	try {
		const auto time_begin = get_time_ms();
		const auto data = prover->get_full_proof(challenge, index);
		if(data.valid) {
			const auto elapsed = (get_time_ms() - time_begin) / 1e3;
			log(elapsed > 20 ? WARN : DEBUG) << "[" << my_name << "] Fetching full proof took " << elapsed << " sec (" << prover->get_file_path() << ")";
			return data.proof;
		} else {
			throw std::runtime_error(data.error_msg);
		}
	} catch(const std::exception& ex) {
		log(WARN) << "[" << my_name << "] Failed to fetch full proof: " << ex.what() << " (" << prover->get_file_path() << ")";
		throw;
	}
}

void Harvester::lookup_task(std::shared_ptr<const Challenge> value, const int64_t recv_time_ms) const
{
	struct pool_conf_t {
		addr_t owner;
		uint64_t difficulty = 0;
		std::string server_url;
	};
	struct lookup_job_t {
		std::mutex mutex;
		std::condition_variable signal;
		size_t total_plots = 0;
		int64_t slow_time_ms = 0;
		int64_t time_begin = 0;
		std::string slow_plot;
		std::unordered_map<addr_t, pool_conf_t> pool_config;
		std::atomic<uint64_t> num_left {0};
		std::atomic<uint64_t> num_passed {0};
		std::atomic<uint32_t> num_proofs {0};
	};
	const auto job = std::make_shared<lookup_job_t>();
	job->total_plots = id_map.size();
	job->num_left = job->total_plots;
	job->time_begin = get_time_ms();

	for(const auto& entry : plot_nfts) {
		const auto& info = entry.second;
		if(info.is_locked && info.server_url) {
			const auto server_url = *info.server_url;
			const auto iter = partial_diff.find(info.address);
			if(iter != partial_diff.end() && iter->second > 0) {
				job->pool_config[info.address] = {info.owner, iter->second, server_url};
			} else {
				log(WARN) << "Waiting on partial difficulty for pool: " + server_url;
			}
		}
	}

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

		threads->add_task([this, plot_id, prover, value, job, recv_time_ms]()
		{
			const auto header = prover->get_header();
			const auto time_begin = get_time_ms();
			const bool hard_fork = value->vdf_height >= params->hardfork1_height;
			const bool passed_filter = check_plot_filter(params, value->challenge, plot_id);

			if(passed_filter) try
			{
				const pool_conf_t* pool_config = nullptr;
				if(auto contract = header->contract) {
					auto iter = job->pool_config.find(*contract);
					if(iter != job->pool_config.end()) {
						pool_config = &iter->second;
					}
				}
				const auto challenge = get_plot_challenge(value->challenge, plot_id);
				const auto qualities = prover->get_qualities(challenge, params->plot_filter);

				for(const auto& res : qualities) {
					try {
						if(!res.valid) {
							log(WARN) << "[" << my_name << "] Failed to fetch quality: " << res.error_msg << " (" << prover->get_file_path() << ")";
							continue;
						}
						if(hard_fork && !pos::check_post_filter(challenge, res.meta, params->post_filter)) {
							continue;	// failed post filter
						}
						hash_t quality;
						if(!hard_fork) {
							quality = pos::calc_quality(challenge, res.meta);
						}

						std::vector<uint32_t> proof_xs;
						if(res.proof.size()) {
							proof_xs = res.proof;	// SSD plot
						} else if(hard_fork) {
							proof_xs = fetch_full_proof(prover, challenge, res.index);	// HDD plot
						}

						if(hard_fork) {
							quality = calc_proof_hash(challenge, proof_xs);
						}
						const auto is_solo_proof =
								check_proof_threshold(params, header->ksize, quality, value->difficulty, hard_fork);
						const auto is_partial_proof = pool_config ?
								check_proof_threshold(params, header->ksize, quality, pool_config->difficulty, hard_fork) : false;

						uint16_t score = -1;
						if(is_solo_proof || is_partial_proof)
						{
							hash_t hash;
							if(hard_fork) {
								hash = quality;
							} else {
								if(proof_xs.empty()) {
									proof_xs = fetch_full_proof(prover, challenge, res.index);	// HDD plot
								}
								hash = calc_proof_hash(value->challenge, proof_xs);
							}
							score = get_proof_score(hash);
						}

						if(is_partial_proof) {
							auto proof = std::make_shared<ProofOfSpaceNFT>();
							proof->seed = header->seed;
							proof->ksize = header->ksize;
							proof->plot_id = header->plot_id;
							proof->challenge = value->challenge;
							proof->difficulty = pool_config->difficulty;
							proof->farmer_key = header->farmer_key;
							proof->contract = *header->contract;
							proof->score = score;
							proof->proof_xs = proof_xs;

							auto out = Partial::create();
							out->vdf_height = value->vdf_height;
							out->contract = *header->contract;
							out->account = pool_config->owner;
							out->pool_url = pool_config->server_url;
							out->proof = proof;
							out->harvester = my_name;
							out->lookup_time_ms = get_time_ms() - time_begin;

							publish(out, output_partials);
						}

						if(is_solo_proof) {
							std::shared_ptr<ProofOfSpace> proof;

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
							proof->score = score;
							proof->plot_id = header->plot_id;
							proof->challenge = value->challenge;
							proof->difficulty = value->difficulty;
							proof->farmer_key = header->farmer_key;

							send_response(value, proof, recv_time_ms);
						}
					} catch(const std::exception& ex) {
						log(WARN) << "[" << my_name << "] Failed to process quality "
								<< res.index << ": " << ex.what() << " (" << prover->get_file_path() << ")";
					}
				}
			} catch(const std::exception& ex) {
				log(WARN) << "[" << my_name << "] Failed to process plot: " << ex.what() << " (" << prover->get_file_path() << ")";
			}

			const auto time_lookup = get_time_ms() - time_begin;
			{
				std::lock_guard<std::mutex> lock(job->mutex);
				if(passed_filter) {
					if(time_lookup > job->slow_time_ms) {
						job->slow_time_ms = time_lookup;
						job->slow_plot = prover->get_file_path();
					}
					job->num_passed++;
				}
				job->num_left--;
			}
			job->signal.notify_all();
		});
	}

	threads->add_task([this, value, job, recv_time_ms]()
	{
		std::unique_lock<std::mutex> lock(job->mutex);
		while(job->num_left) {
			job->signal.wait(lock);
		}
		const auto time_end = get_time_ms();
		{
			auto out = LookupInfo::create();
			out->id = harvester_id;
			out->name = my_name;
			out->vdf_height = value->vdf_height;
			out->num_total = job->total_plots;
			out->num_passed = job->num_passed;
			out->total_time_ms = time_end - job->time_begin;
			out->total_delay_ms = time_end - recv_time_ms;
			if(job->num_passed) {
				out->slow_time_ms = job->slow_time_ms;
				out->slow_plot = job->slow_plot;
			}
			publish(out, output_lookups);
		}
		if(job->total_plots) {
			const auto slow_time = job->slow_time_ms / 1e3;
			if(job->num_passed) {
				log(slow_time > 20 ? WARN : DEBUG) << "[" << my_name << "] Slowest plot took " << slow_time << " sec (" << job->slow_plot << ")";
			}
			const auto delay_sec = (time_end - recv_time_ms) / 1e3;
			log(INFO) << "[" << my_name << "] " << job->num_passed << " of " << job->total_plots
					<< " plots were eligible for height " << value->vdf_height
					<< ", max lookup " << slow_time << " sec, delay " << delay_sec << " sec";
		}
	});

	// trigger next lookup right away
	lookup_timer->set_millis(0);
}

uint64_t Harvester::get_total_bytes() const
{
	return total_bytes;
}

std::shared_ptr<const FarmInfo> Harvester::get_farm_info() const
{
	auto out = FarmInfo::create();
	out->harvester = my_name;
	out->harvester_id = harvester_id;
	out->plot_dirs = std::vector<std::string>(plot_dirs.begin(), plot_dirs.end());
	out->total_bytes = total_bytes;
	out->total_bytes_effective = total_bytes_effective;
	for(const auto& entry : plot_map) {
		if(const auto& prover = entry.second) {
			out->plot_count[prover->get_ksize()]++;
		}
	}
	for(const auto& entry : plot_nfts) {
		const auto& nft = entry.second;
		auto& info = out->pool_info[nft.address];
		info.contract = nft.address;
		info.name = nft.name;
		info.is_plot_nft = true;
		if(nft.is_locked) {
			info.server_url = nft.server_url;
			info.pool_target = nft.target;
		}
		{
			auto iter = partial_diff.find(nft.address);
			if(iter != partial_diff.end()) {
				info.partial_diff = iter->second;
			}
		}
		{
			auto iter = plot_contract_set.find(nft.address);
			if(iter != plot_contract_set.end()) {
				info.plot_count = iter->second;
			}
		}
	}
	for(const auto& entry : plot_contract_set) {
		if(!plot_nfts.count(entry.first)) {
			auto& info = out->pool_info[entry.first];
			info.contract = entry.first;
			info.plot_count = entry.second;
		}
	}
	return out;
}

void Harvester::find_plot_dirs(const std::set<std::string>& dirs, std::set<std::string>& all_dirs, const size_t depth) const
{
	if(depth > max_recursion) {
		return;
	}
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
			log(WARN) << "[" << my_name << "] " << ex.what();
		}
	}
	if(!sub_dirs.empty()) {
		find_plot_dirs(sub_dirs, all_dirs, depth + 1);
	}
}

void Harvester::reload()
{
	const auto time_begin = get_time_ms();

	std::set<pubkey_t> farmer_keys;
	for(const auto& key : farmer->get_farmer_keys()) {
		farmer_keys.insert(key);
	}

	std::set<std::string> dir_set;
	if(recursive_search) {
		find_plot_dirs(plot_dirs, dir_set, 0);
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
					if(!plot_map.count(file_name) && file->get_extension() == ".plot" && file->get_name().substr(0, 9) == "plot-mmx-")
					{
						std::lock_guard<std::mutex> lock(mutex);
						plot_files.push_back(file_name);
					}
				}
			} catch(const std::exception& ex) {
				log(WARN) << "[" << my_name << "] " << ex.what();
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
				const auto prover = std::make_shared<pos::Prover>(file_path);
				const auto ksize = uint32_t(prover->get_ksize());
				if(ksize < params->min_ksize || ksize > params->max_ksize) {
					throw std::logic_error("invalid ksize: " + std::to_string(ksize));
				}
				{
					std::lock_guard<std::mutex> lock(mutex);
					plots.emplace_back(file_path, prover);
				}
			} catch(const std::exception& ex) {
				log(WARN) << "[" << my_name << "] Failed to load plot '" << file_path << "' due to: " << ex.what();
			} catch(...) {
				log(WARN) << "[" << my_name << "] Failed to load plot '" << file_path << "'";
			}
		});
	}
	threads->sync();

	// purge missing plots
	for(const auto& file_name : missing) {
		plot_map.erase(file_name);
	}

	if(missing.size()) {
		log(INFO) << "[" << my_name << "] Lost " << missing.size() << " plots";
	}
	if(plots.size() && plot_map.size()) {
		log(INFO) << "[" << my_name << "] Found " << plots.size() << " new plots";
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
			log(WARN) << "[" << my_name << "] Invalid plot: " << entry.first << " (" << ex.what() << ")";
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
			log(WARN) << "[" << my_name << "] Duplicate plot: " << entry.first << " (already have: " << id_map[plot_id] << ")";
		}
		total_bytes += vnx::File(file_name).file_size();
		total_bytes_effective += get_effective_plot_size(prover->get_ksize());
	}

	// gather plot NFTs
	plot_contract_set.clear();
	for(const auto& entry : plot_map) {
		if(const auto& addr = entry.second->get_contract()) {
			plot_contract_set[*addr]++;
		}
	}

	update();
	update_nfts();

	// check challenges again for new plots
	if(plots.size()) {
		is_ready = false;
		already_checked.clear();
	}
	if(!is_ready) {
		set_timeout_millis(3000, [this]() {
			is_ready = true;
		});
	}
	log(INFO) << "[" << my_name << "] Loaded " << plot_map.size() << " plots, "
			<< total_bytes / pow(1000, 4) << " TB, " << total_bytes_effective / pow(1000, 4) << " TBe"
			<< ", took " << (get_time_ms() - time_begin) / 1e3 << " sec";
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
	farmer_async->get_mac_addr(
		[this](const vnx::Hash64& mac) {
			farmer_addr = mac;
		},
		[this](const std::exception& ex) {
			log(WARN) << "Failed to contact farmer: " << ex.what();
		});

	std::vector<addr_t> pool_nfts;
	for(const auto& entry : plot_nfts) {
		if(entry.second.server_url) {
			pool_nfts.push_back(entry.first);
		}
	}
	farmer_async->get_partial_diffs(pool_nfts,
		[this](const std::map<addr_t, uint64_t>& new_diff) {
			for(const auto& entry : new_diff) {
				if(entry.second != partial_diff[entry.first]) {
					log(INFO) << "New partial difficulty: " << entry.second << " (" << entry.first << ")";
				}
			}
			partial_diff = new_diff;
		});

	publish(get_farm_info(), output_info);
}

void Harvester::update_nfts()
{
	std::set<addr_t> missing;
	for(const auto& entry : plot_nfts) {
		missing.insert(entry.first);
	}
	auto job = std::make_shared<size_t>(plot_contract_set.size());

	for(const auto& entry : plot_contract_set) {
		const auto& address = entry.first;
		node_async->get_plot_nft_info(address,
			[this, job, address](const vnx::optional<plot_nft_info_t>& info) {
				if(info) {
					if(!plot_nfts.count(address)) {
						log(INFO) << "Found PlotNFT " << (info->name.empty() ? info->address.to_string() : "'" + info->name + "'")
								<< ": is_locked = " << vnx::to_string(info->is_locked)
								<< ", server_url = " << vnx::to_string(info->server_url);
					}
					plot_nfts[address] = *info;
				}
				if(--(*job) == 0) {
					update();
					set_timeout_millis(2000, std::bind(&Harvester::update, this));
				}
			},
			[this, address](const std::exception& ex) {
				log(WARN) << "Failed to query PlotNFT " << address.to_string() << " due to: " << ex.what();
			});
		missing.erase(address);
	}
	for(const auto& address : missing) {
		plot_nfts.erase(address);
	}
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
