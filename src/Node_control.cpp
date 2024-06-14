/*
 * Node_control.cpp
 *
 *  Created on: May 6, 2024
 *      Author: mad
 */

#include <mmx/Node.h>
#include <mmx/http_request.h>
#include <mmx/contract/TokenBase.hxx>

#include <vnx/vnx.h>
#include <url.h>


static bool is_expired(const std::string& file_path, const int64_t max_age_sec = 3600 * 24)
{
	vnx::File file(file_path);
	return !file.exists() || file.last_write_time() + max_age_sec < vnx::get_wall_time_seconds();
}


namespace mmx {

void Node::update_control()
{
	const auto fetch_func = [this](const std::string& url, const std::string& file_path, const std::string& key) {
		try {
			http_request_file(url, file_path);
			if(vnx::do_run()) {
				add_task(std::bind(&Node::update_control, this));
			}
		}
		catch(const std::exception& ex) {
			log(WARN) << "Failed to fetch " << Url::Url(url).setQuery({}).str();
		}
		if(vnx::do_run()) {
			std::lock_guard<std::mutex> lock(fetch_mutex);
			pending_fetch.erase(key);
		}
	};

	std::lock_guard<std::mutex> lock(fetch_mutex);

	bool try_again = false;

	vnx::optional<double> metalsdev_price;
	{
		const std::string file_path = storage_path + "metalsdev_xau_usd.json";

		if(is_expired(file_path)) {
			const std::string key = "api.metals.dev";
			if(pending_fetch.insert(key).second) {
				std::string param;
				if(metalsdev_api_key.empty()) {
					const auto tmp = vnx::from_hex_string("6170695f6b65793d554a4f4f584b43364756575739484e4a494b594e3537394e4a494b594e");
					param = std::string((const char*)tmp.data(), tmp.size());
				} else {
					param = "api_key=" + metalsdev_api_key;
				}
				fetch_threads->add_task(std::bind(
						fetch_func, "https://api.metals.dev/v1/metal/spot?" + param + "&metal=gold&currency=USD", file_path, key));
			}
			try_again = true;
		} else {
			try {
				std::ifstream stream(file_path);
				const auto json = vnx::read_json(stream, true);
				if(!json) {
					throw std::logic_error("empty file");
				}
				const auto object = json->to_object();

				if(object["status"].to_string_value() != "success") {
					throw std::logic_error("bad status: " + object["status"].to_string_value());
				}
				if(object["metal"].to_string_value() != "gold") {
					throw std::logic_error("expected gold");
				}
				if(object["unit"].to_string_value() != "toz") {
					throw std::logic_error("expected unit toz");
				}
				if(object["currency"].to_string_value() != "USD") {
					throw std::logic_error("expected currency USD");
				}
				const auto rate = object["rate"].to_object();
				const auto price = rate["price"].to<double>();
				if(price <= 0) {
					throw std::logic_error("invalid price: " + std::to_string(price));
				}
				metalsdev_price = price;
				log(INFO) << "Got metals.dev XAU price: " << price << " USD";
			}
			catch(const std::exception& ex) {
				log(WARN) << "Failed to parse " << file_path << ": " << ex.what();
			}
		}
	}

	vnx::optional<double> swissquote_price;
	{
		const std::string file_path = storage_path + "swissquote_xau_usd.json";

		if(is_expired(file_path)) {
			const std::string key = "forex-data-feed.swissquote.com";
			if(pending_fetch.insert(key).second) {
				fetch_threads->add_task(std::bind(
						fetch_func, "https://forex-data-feed.swissquote.com/public-quotes/bboquotes/instrument/XAU/USD", file_path, key));
			}
			try_again = true;
		} else {
			try {
				std::ifstream stream(file_path);
				const auto json = vnx::read_json(stream, true);
				if(!json) {
					throw std::logic_error("empty file");
				}
				const auto array = std::dynamic_pointer_cast<vnx::JSON_Array>(json);
				if(!array) {
					throw std::logic_error("top level not an array");
				}
				const auto list = array->get_values();
				if(list.empty()) {
					throw std::logic_error("empty list");
				}
				const auto MT5 = list[0]->to_object();
				const auto profiles = MT5["spreadProfilePrices"].to<std::vector<vnx::Variant>>();
				if(profiles.empty()) {
					throw std::logic_error("found no MT5 profiles");
				}
				const auto prime = profiles[0].to_object();

				const auto bid = prime["bid"].to<double>();
				const auto ask = prime["ask"].to<double>();
				if(bid <= 0 || ask <= 0 || bid / ask > 1.1) {
					throw std::logic_error("invalid bid or ask price: " + std::to_string(bid) + " / " + std::to_string(ask));
				}
				const auto price = (bid + ask) / 2;
				swissquote_price = price;
				log(INFO) << "Got swissquote.com XAU price: " << price << " USD";
			}
			catch(const std::exception& ex) {
				log(WARN) << "Failed to parse " << file_path << ": " << ex.what();
			}
		}
	}

	double gold_price = 0;
	if(metalsdev_price) {
		gold_price = *metalsdev_price;
	} else if(swissquote_price) {
		gold_price = *swissquote_price;
	} else {
		if(!try_again) {
			reward_vote = 0;
			log(WARN) << "Failed to query XAU price information!";
		}
		return;
	}

	if(mmx_usd_swap_addr == addr_t()) {
		log(INFO) << "Reward voting is disabled due to lack of MMX swap / exchange";
		return;
	}

	try {
		const auto swap_info = get_swap_info(mmx_usd_swap_addr);
		const auto usd_contract_addr = swap_info.tokens[1];
		const auto usd_contract = get_contract_as<mmx::contract::TokenBase>(usd_contract_addr);
		if(!usd_contract) {
			throw std::runtime_error("could not find USD contract: " + usd_contract_addr.to_string());
		}
		if(!swap_info.balance[0] || !swap_info.balance[1]) {
			throw std::runtime_error("missing swap liquidity");
		}
		const auto mmx_usd_price = to_value(swap_info.balance[1], usd_contract->decimals) / to_value(swap_info.balance[0], params->decimals);
		const auto current = gold_price / mmx_usd_price;

		if(current > params->target_mmx_gold_price * 1.01) {
			reward_vote = -1;
		}
		else if(current < params->target_mmx_gold_price / 1.01) {
			reward_vote = 1;
		}
		else {
			reward_vote = 0;
		}
		log(INFO) << u8"\U0001F4B5 MMX price = " << mmx_usd_price << " USD, XAU price = " << gold_price << " USD, MMX per ounce = "
				<< current << " MMX (target " << params->target_mmx_gold_price << "), reward vote = " << reward_vote;
	}
	catch(const std::exception& ex) {
		log(WARN) << "Failed to calculate reward vote: " << ex.what();
	}
}







} // mmx
