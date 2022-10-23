/*
 * balance_cache_t.h
 *
 *  Created on: May 8, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_BALANCE_CACHE_T_H_
#define INCLUDE_MMX_BALANCE_CACHE_T_H_

#include <mmx/addr_t.hpp>
#include <mmx/uint128.hpp>

#include <map>


namespace mmx {

class balance_cache_t {
public:
	balance_cache_t(const balance_cache_t&) = default;

	balance_cache_t(balance_cache_t* parent) : parent(parent) {}

	balance_cache_t(const std::map<std::pair<addr_t, addr_t>, uint128>* source) : source(source) {}

	balance_cache_t& operator=(const balance_cache_t&) = default;

	uint128* find(const addr_t& address, const addr_t& contract)
	{
		const auto key = std::make_pair(address, contract);
		auto iter = balance.find(key);
		if(iter == balance.end()) {
			if(source) {
				auto iter2 = source->find(key);
				if(iter2 != source->end()) {
					iter = balance.emplace(key, iter2->second).first;
				} else {
					return nullptr;
				}
			}
			if(parent) {
				if(auto value = parent->find(address, contract)) {
					iter = balance.emplace(key, *value).first;
				} else {
					return nullptr;
				}
			}
		}
		return &iter->second;
	}

	uint128& get(const addr_t& address, const addr_t& contract)
	{
		if(auto value = find(address, contract)) {
			return *value;
		}
		return balance[std::make_pair(address, contract)] = 0;
	}

	void apply(const balance_cache_t& cache)
	{
		for(const auto& entry : cache.balance) {
			balance[entry.first] = entry.second;
		}
	}

	std::map<std::pair<addr_t, addr_t>, uint128> balance;

private:
	balance_cache_t* const parent = nullptr;
	const std::map<std::pair<addr_t, addr_t>, uint128>* const source = nullptr;

};


} // mmx

#endif /* INCLUDE_MMX_BALANCE_CACHE_T_H_ */
