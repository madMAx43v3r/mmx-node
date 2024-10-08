/*
 * Farmer.cpp
 *
 *  Created on: Dec 12, 2021
 *      Author: mad
 */

#include <mmx/Farmer.h>
#include <mmx/Transaction.hxx>
#include <mmx/ProofOfSpaceOG.hxx>
#include <mmx/WebRender.h>
#include <mmx/utils.h>


namespace mmx {

Farmer::Farmer(const std::string& _vnx_name)
	:	FarmerBase(_vnx_name)
{
}

void Farmer::init()
{
	pipe = vnx::open_pipe(vnx_name, this, 1000);
	pipe->pause();

	vnx::open_pipe(vnx_get_id(), this, 1000);

	subscribe(input_info, 1000);
	subscribe(input_proofs, 10000);
	subscribe(input_partials, 10000);
}

void Farmer::main()
{
	if(reward_addr) {
		if(*reward_addr == addr_t()) {
			reward_addr = nullptr;
		} else {
			log(INFO) << "Reward address: " << reward_addr->to_string();
		}
	}
	params = get_params();

	http_client = new vnx::addons::HttpClient(vnx_name + ".HttpClient");
	http_client->output_response = "farmer.http_response";
	http_client.start();

	wallet = std::make_shared<WalletAsyncClient>(wallet_server);
	http_async = std::make_shared<vnx::addons::HttpClientAsyncClient>(http_client.get_id());

	wallet->vnx_set_non_blocking(true);
	http_async->vnx_set_non_blocking(true);

	add_async_client(wallet);
	add_async_client(http_async);

	set_timer_millis(60 * 1000, std::bind(&Farmer::update, this));

	set_timer_millis(int64_t(difficulty_interval) * 1000, std::bind(&Farmer::update_difficulty, this));

	update();

	Super::main();
}

vnx::Hash64 Farmer::get_mac_addr() const
{
	return vnx_get_id();
}

uint64_t Farmer::get_partial_diff(const addr_t& plot_nft) const
{
	auto iter = nft_stats.find(plot_nft);
	if(iter != nft_stats.end()) {
		const auto& stats = iter->second;
		if(stats.partial_diff > 0) {
			return stats.partial_diff;
		}
	}
	return 1;
}

std::map<addr_t, uint64_t> Farmer::get_partial_diffs(const std::vector<addr_t>& plot_nfts) const
{
	std::map<addr_t, uint64_t> out;
	for(const auto& addr : plot_nfts) {
		out[addr] = get_partial_diff(addr);
	}
	return out;
}

std::vector<pubkey_t> Farmer::get_farmer_keys() const
{
	std::vector<pubkey_t> out;
	for(const auto& entry : key_map) {
		out.push_back(entry.first);
	}
	return out;
}

std::shared_ptr<const FarmInfo> Farmer::get_farm_info() const
{
	auto info = FarmInfo::create();
	for(const auto& entry : info_map) {
		if(auto value = std::dynamic_pointer_cast<const FarmInfo>(entry.second->value)) {
			if(value->harvester) {
				auto& entry = info->harvester_bytes[*value->harvester];
				entry.first += value->total_bytes;
				entry.second += value->total_bytes_effective;
			}
			for(const auto& entry : value->plot_count) {
				info->plot_count[entry.first] += entry.second;
			}
			for(const auto& dir : value->plot_dirs) {
				info->plot_dirs.push_back((value->harvester ? *value->harvester + ":" : "") + dir);
			}
			for(const auto& entry : value->pool_info) {
				auto& dst = info->pool_info[entry.first];
				const auto prev_count = dst.plot_count;
				dst = entry.second;
				dst.plot_count += prev_count;
			}
			info->total_bytes += value->total_bytes;
			info->total_bytes_effective += value->total_bytes_effective;
			info->total_balance += value->total_balance;
		}
	}
	for(auto& entry : info->pool_info) {
		entry.second.partial_diff = get_partial_diff(entry.first);
	}
	info->pool_stats = nft_stats;
	return info;
}

void Farmer::update()
{
	vnx::open_flow(vnx::get_pipe(node_server), vnx::get_pipe(vnx_get_id()));

	wallet->get_all_farmer_keys(
		[this](const std::vector<std::pair<skey_t, pubkey_t>>& list) {
			for(const auto& keys : list) {
				if(key_map.emplace(keys.second, keys.first).second) {
					log(INFO) << "Got Farmer Key: " << keys.second;
				}
			}
			pipe->resume();
		},
		[this](const vnx::exception& ex) {
			log(WARN) << "Failed to get keys from wallet: " << ex.what();
		});

	if(!reward_addr) {
		wallet->get_all_accounts(
			[this](const std::vector<account_info_t>& accounts) {
				if(!reward_addr) {
					for(const auto& entry : accounts) {
						if(entry.address) {
							reward_addr = entry.address;
							break;
						}
					}
					if(reward_addr) {
						log(INFO) << "Reward address: " << reward_addr->to_string();
					} else {
						log(WARN) << "Failed to get reward address from wallet: no wallet available";
					}
				}
			},
			[this](const vnx::exception& ex) {
				log(WARN) << "Failed to get reward address from wallet: " << ex.what();
			});
	}

	const auto now = vnx::get_sync_time_micros();
	for(auto iter = info_map.begin(); iter != info_map.end();) {
		if((now - iter->second->recv_time) / 1000000 > harvester_timeout) {
			iter = info_map.erase(iter);
		} else {
			iter++;
		}
	}
}

void Farmer::update_difficulty()
{
	for(const auto& entry : nft_stats) {
		query_difficulty(entry.first, entry.second.server_url);
	}
}

void Farmer::query_difficulty(const addr_t& contract, const std::string& url)
{
	if(url.empty()) {
		return;
	}
	http_async->get_json(url + "/difficulty", {},
		[this, url, contract](const vnx::Variant& value) {
			const auto res = value.to_object();
			if(auto field = res["difficulty"]) {
				const auto diff = field.to<uint64_t>();
				if(diff > 0) {
					auto& stats = nft_stats[contract];
					if(stats.partial_diff <= 0) {
						log(INFO) << "Got partial difficulty: " << diff << " (" << url << ")";
					}
					stats.partial_diff = diff;
				} else {
					log(WARN) << "Got invalid partial difficulty: " << diff << " (" << url << ")";
				}
			} else {
				log(WARN) << "Failed to query partial difficulty from " << url << ": " << value.to_string();
			}
		},
		[this, url](const std::exception& ex) {
			log(WARN) << "Failed to query partial difficulty from " << url << " due to: " << ex.what();
		});
}

void Farmer::handle(std::shared_ptr<const FarmInfo> value)
{
	if(auto sample = vnx_sample) {
		if(value->harvester_id) {
			info_map[*value->harvester_id] = sample;
		} else if(value->harvester) {
			info_map[hash_t(*value->harvester)] = sample;
		}
	}
	for(const auto& entry : value->pool_info) {
		const auto& info = entry.second;
		if(auto url = info.server_url) {
			if(url->size()) {
				auto& stats = nft_stats[info.contract];
				if(stats.server_url != (*url)) {
					stats.server_url = *url;
					query_difficulty(info.contract, *url);
				}
			}
		}
	}
}

void Farmer::handle(std::shared_ptr<const ProofResponse> value) try
{
	if(!value->is_valid()) {
		throw std::logic_error("invalid proof");
	}
	const skey_t farmer_sk = get_skey(value->proof->farmer_key);

	auto out = vnx::clone(value);
	out->farmer_sig = signature_t::sign(farmer_sk, value->hash);
	out->content_hash = out->calc_hash(true);
	publish(out, output_proofs);
}
catch(const std::exception& ex) {
	log(WARN) << "Failed to sign proof from harvester '" << value->harvester << "' due to: " << ex.what();
}

void Farmer::handle(std::shared_ptr<const Partial> value) try
{
	if(!value->proof) {
		return;
	}
	auto out = vnx::clone(value);
	if(reward_addr) {
		out->account = *reward_addr;
	} else {
		log(WARN) << "Using plot NFT owner as fallback payout address: " << out->account.to_string();
	}
	out->hash = out->calc_hash();

	const auto farmer_sk = get_skey(value->proof->farmer_key);
	out->farmer_sig = signature_t::sign(farmer_sk, out->hash);

	const auto payload = vnx::to_pretty_string(web_render(out));

	http_async->post_json(out->pool_url + "/partial", payload, {},
		[this, out](std::shared_ptr<const vnx::addons::HttpResponse> response) {
			auto& stats = nft_stats[out->contract];
			stats.last_partial = vnx::get_wall_time_seconds();
			bool is_valid = false;
			bool have_error = false;
			if(response->is_json()) {
				const auto value = response->parse_json();
				if(value.is_object()) {
					const auto res = value.to_object();
					is_valid = res["valid"].to<bool>();
					if(is_valid) {
						const auto points = res["points"].to<int64_t>();
						const auto response_ms = res["response_time"].to<int64_t>();
						stats.valid_points += (points > 0 ? points : out->difficulty);
						stats.total_partials++;
						stats.total_response_time += response_ms;
						log(INFO) << "Partial accepted: points = " << points
								<< ", response = " << response_ms / 1e3
								<< " sec [" << out->harvester << "] (" << out->pool_url << ")";
					} else {
						const auto code = res["error_code"].to<pooling_error_e>();
						if(code != pooling_error_e::NONE) {
							have_error = true;
							const auto message = res["error_message"].to_string_value();
							stats.error_count[code]++;
							log(WARN) << "Partial was rejected due to: "
									<< code.to_string_value() << ": " << (message.empty() ? "???" : message)
									<< " [" << out->harvester << "] (" << out->pool_url << ")";
						}
					}
				}
			}
			if(!is_valid) {
				if(!have_error) {
					stats.error_count[pooling_error_e::SERVER_ERROR]++;
					log(WARN) << "Partial failed due to: unknown error [" << out->harvester << "] (" << out->pool_url << ")";
				}
				stats.failed_points += out->difficulty;
			}
		},
		[this, out](const std::exception& ex) {
			auto& stats = nft_stats[out->contract];
			stats.failed_points += out->difficulty;
			stats.error_count[pooling_error_e::SERVER_ERROR]++;
			log(WARN) << "Failed to send partial to " << out->pool_url << " due to: " << ex.what();
		});

	publish(out, output_partials);
}
catch(const std::exception& ex) {
	log(WARN) << "Failed to process partial from harvester '" << value->harvester << "' due to: " << ex.what();
}

skey_t Farmer::get_skey(const pubkey_t& pubkey) const
{
	auto iter = key_map.find(pubkey);
	if(iter == key_map.end()) {
		throw std::logic_error("unknown farmer key: " + pubkey.to_string());
	}
	return iter->second;
}

std::shared_ptr<const BlockHeader>
Farmer::sign_block(std::shared_ptr<const BlockHeader> block) const
{
	if(!block) {
		throw std::logic_error("!block");
	}
	if(!block->proof) {
		throw std::logic_error("!proof");
	}
	const auto farmer_sk = get_skey(block->proof->farmer_key);

	auto out = vnx::clone(block);
	out->nonce = vnx::rand64();

	if(!out->reward_addr || std::dynamic_pointer_cast<const ProofOfSpaceOG>(block->proof)) {
		out->reward_addr = reward_addr;
	} else {
		out->reward_account = reward_addr;
	}
	out->hash = out->calc_hash().first;
	out->farmer_sig = signature_t::sign(farmer_sk, out->hash);
	out->content_hash = out->calc_hash().second;
	return out;
}


} // mmx
