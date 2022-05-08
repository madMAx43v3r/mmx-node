/*
 * balance_cache_t.h
 *
 *  Created on: May 8, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_BALANCE_CACHE_T_H_
#define INCLUDE_MMX_BALANCE_CACHE_T_H_

#include <mmx/addr_t.hpp>

#include <uint128_t.h>

#include <map>


namespace mmx {

class balance_cache_t {
public:
	balance_cache_t(const balance_cache_t&) = default;

	balance_cache_t(balance_cache_t* parent) : parent(parent) {}

	balance_cache_t(const std::map<std::pair<addr_t, addr_t>, uint128_t>* source) : source(source) {}

	balance_cache_t& operator=(const balance_cache_t&) = default;

	uint128_t* get(const addr_t& address, const addr_t& contract)
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
				if(auto bal = parent->get(address, contract)) {
					iter = balance.emplace(key, *bal).first;
				} else {
					return nullptr;
				}
			}
		}
		return &iter->second;
	}

	void apply(const balance_cache_t& cache)
	{
		for(const auto& entry : cache.balance) {
			balance[entry.first] = entry.second;
		}
	}

private:
	std::map<std::pair<addr_t, addr_t>, uint128_t> balance;

	balance_cache_t* const parent = nullptr;
	const std::map<std::pair<addr_t, addr_t>, uint128_t>* const source = nullptr;

};


} // mmx

#endif /* INCLUDE_MMX_BALANCE_CACHE_T_H_ */
