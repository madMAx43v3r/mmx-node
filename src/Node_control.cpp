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

#include <cmath>
#include <list>
#include <vector>


static bool is_expired(const std::string& file_path, const int64_t max_age_sec = 3600 * 24)
{
	vnx::File file(file_path);
	return !file.exists() || file.last_write_time() + max_age_sec < mmx::get_time_sec();
}


static double get_median(std::list<double> values)
{
	if(values.empty()) {
		throw std::logic_error("cannot get median of an empty list");
	}
	values.sort();
	while(values.size() > 2) {
		values.pop_front();
		values.pop_back();
	}

	double total = 0;
	for(const auto value : values) {
		total += value;
	}
	return total / values.size();
}


namespace mmx {

void Node::update_control()
{
	const auto fetch_func = [this](const std::string& url, const std::string& file_path, const std::string& key, const std::string& options = "") {
		try {
			http_request_file(url, file_path, options);
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

	std::list<double> gold_price_inputs_usd;
	{
		const std::string file_path = storage_path + "goldapi_xau_usd.json";

		if(is_expired(file_path)) {
			const std::string key = "api.gold-api.com";
			if(pending_fetch.insert(key).second) {
				fetch_threads->add_task(std::bind(
						fetch_func, "https://api.gold-api.com/price/XAU/USD", file_path, key));
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

				if(object["symbol"].to_string_value() != "XAU") {
					throw std::logic_error("expected XAU");
				}
				if(object["currency"].to_string_value() != "USD") {
					throw std::logic_error("expected currency USD");
				}
				const auto price = object["price"].to<double>();
				if(!std::isfinite(price) || price <= 0) {
					throw std::logic_error("invalid price: " + std::to_string(price));
				}
				gold_price_inputs_usd.push_back(price);
				log(INFO) << "Got gold-api.com XAU price: " << price << " USD";
			}
			catch(const std::exception& ex) {
				log(WARN) << "Failed to parse " << file_path << ": " << ex.what();
			}
		}
	}

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
				if(!std::isfinite(bid) || !std::isfinite(ask) || bid <= 0 || ask <= 0 || bid / ask > 1.1) {
					throw std::logic_error("invalid bid or ask price: " + std::to_string(bid) + " / " + std::to_string(ask));
				}
				const auto price = (bid + ask) / 2;
				gold_price_inputs_usd.push_back(price);
				log(INFO) << "Got swissquote.com XAU price: " << price << " USD";
			}
			catch(const std::exception& ex) {
				log(WARN) << "Failed to parse " << file_path << ": " << ex.what();
			}
		}
	}

	{
		const std::string file_path = storage_path + "goldprice_dev_xau_usd.json";

		if(is_expired(file_path)) {
			const std::string key = "api.goldprice.dev";
			if(pending_fetch.insert(key).second) {
				fetch_threads->add_task(std::bind(
						fetch_func, "https://api.goldprice.dev/v1/spot/XAU-USD-SPOT", file_path, key));
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

				if(object["symbol"].to_string_value() != "XAU") {
					throw std::logic_error("expected XAU");
				}
				if(object["quote_currency"].to_string_value() != "USD") {
					throw std::logic_error("expected quote currency USD");
				}
				if(object["unit"].to_string_value() != "troy_ounce") {
					throw std::logic_error("expected unit troy_ounce");
				}
				if(object["contract_type"].to_string_value() != "spot") {
					throw std::logic_error("expected spot price");
				}
				if(object["is_stale"].to<bool>()) {
					throw std::logic_error("stale price");
				}
				const auto price = std::stod(object["price"].to_string_value());
				if(!std::isfinite(price) || price <= 0) {
					throw std::logic_error("invalid price: " + std::to_string(price));
				}
				gold_price_inputs_usd.push_back(price);
				log(INFO) << "Got goldprice.dev XAU price: " << price << " USD";
			}
			catch(const std::exception& ex) {
				log(WARN) << "Failed to parse " << file_path << ": " << ex.what();
			}
		}
	}

	if(gold_price_inputs_usd.empty()) {
		if(!try_again) {
			reward_vote = 0;
			log(WARN) << "Failed to query XAU price information!";
		}
		return;
	}
	const auto gold_price_usd = get_median(gold_price_inputs_usd);

	std::list<double> mmx_price_inputs_usd;

	if(mmx_usd_swap_addr != addr_t()) try {
		const auto swap_info = get_swap_info(mmx_usd_swap_addr);
		const auto usd_contract_addr = swap_info.tokens[1];
		const auto usd_contract = get_contract_as<mmx::contract::TokenBase>(usd_contract_addr);
		if(!usd_contract) {
			throw std::runtime_error("could not find USD contract: " + usd_contract_addr.to_string());
		}
		if(!swap_info.balance[0] || !swap_info.balance[1]) {
			throw std::runtime_error("missing swap liquidity");
		}
		const auto price = to_value(swap_info.balance[1], usd_contract->decimals) / to_value(swap_info.balance[0], params->decimals);
		if(!std::isfinite(price) || price <= 0) {
			throw std::runtime_error("invalid swap price: " + std::to_string(price));
		}
		mmx_price_inputs_usd.push_back(price);
	}
	catch(const std::exception& ex) {
		log(WARN) << "Failed to get MMX swap price: " << ex.what();
	}

	{
		const std::string file_path = storage_path + "safetrade_mmx_usdt.json";

		if(is_expired(file_path)) {
			const std::string key = "safetrade.com";
			if(pending_fetch.insert(key).second) {
				const auto tmp = vnx::from_hex_string("2d482022757365722d6167656e743a204d4d582d4e6f64652d313133333722");
				const std::string options((const char*)tmp.data(), tmp.size());
				fetch_threads->add_task(std::bind(
						fetch_func, "https://safe.trade/api/v2/trade/public/currencies/mmx", file_path, key, options));
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

				if(object["status"].to_string_value() != "enabled") {
					throw std::logic_error("bad status: " + object["status"].to_string_value());
				}
				if(object["id"].to_string_value() != "mmx") {
					throw std::logic_error("expected MMX");
				}
				const auto price = std::stod(object["price"].to_string_value());
				if(!std::isfinite(price) || price <= 0) {
					throw std::logic_error("invalid price: " + std::to_string(price));
				}
				mmx_price_inputs_usd.push_back(price);
				log(INFO) << "Got safetrade.com MMX price: " << price << " USD";
			}
			catch(const std::exception& ex) {
				log(WARN) << "Failed to parse " << file_path << ": " << ex.what();
			}
		}
	}

	struct nonkyc_market_t {
		std::string symbol;
		std::string api_symbol;
		std::string file_name;
	};

	static const nonkyc_market_t nonkyc_markets[] = {
		{"MMX/USDT", "MMX_USDT", "nonkyc_mmx_usdt.json"},
		{"MMX/USDC", "MMX_USDC", "nonkyc_mmx_usdc.json"},
	};

	for(const auto& market : nonkyc_markets) {
		const std::string file_path = storage_path + market.file_name;

		if(is_expired(file_path)) {
			const std::string key = "api.nonkyc.io/" + market.api_symbol;
			if(pending_fetch.insert(key).second) {
				fetch_threads->add_task(std::bind(
						fetch_func, "https://api.nonkyc.io/api/v2/market/getbysymbol/" + market.api_symbol, file_path, key, ""));
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

				if(object["symbol"].to_string_value() != market.symbol) {
					throw std::logic_error("expected " + market.symbol);
				}
				if(!object["isActive"].to<bool>() || object["isPaused"].to<bool>()) {
					throw std::logic_error("market is not active");
				}
				const auto price = std::stod(object["lastPrice"].to_string_value());
				if(!std::isfinite(price) || price <= 0) {
					throw std::logic_error("invalid price: " + std::to_string(price));
				}
				mmx_price_inputs_usd.push_back(price);
				log(INFO) << "Got nonkyc.io " << market.symbol << " price: " << price << " USD";
			}
			catch(const std::exception& ex) {
				log(WARN) << "Failed to parse " << file_path << ": " << ex.what();
			}
		}
	}

	if(mmx_price_inputs_usd.empty()) {
		if(!try_again) {
			reward_vote = 0;
			log(INFO) << "Reward voting is disabled due to lack of MMX swap / exchange";
		}
		return;
	}
	const auto mmx_price_usd = get_median(mmx_price_inputs_usd);
	const auto current = gold_price_usd / mmx_price_usd;

	if(current > params->target_mmx_gold_price * 1.01) {
		reward_vote = -1;
	}
	else if(current < params->target_mmx_gold_price / 1.01) {
		reward_vote = 1;
	}
	else {
		reward_vote = 0;
	}
	log(INFO) << u8"\U0001F4B5 MMX price = " << mmx_price_usd << " USD, XAU price = " << gold_price_usd << " USD, MMX per ounce = "
			<< current << " MMX (target " << params->target_mmx_gold_price << "), reward vote = " << reward_vote;
}







} // mmx
