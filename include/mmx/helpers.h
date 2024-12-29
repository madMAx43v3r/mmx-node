/*
 * helpers.h
 *
 *  Created on: Dec 29, 2024
 *      Author: mad
 */

#ifndef INCLUDE_MMX_HELPERS_H_
#define INCLUDE_MMX_HELPERS_H_

#include <map>
#include <unordered_map>

#include <vnx/optional.h>


namespace mmx {

template<typename T>
typename T::mapped_type find_value(const T& map, const typename T::key_type& key, const typename T::mapped_type& value)
{
	auto iter = map.find(key);
	if(iter != map.end()) {
		return iter->second;
	}
	return value;
}

template<typename T>
vnx::optional<typename T::mapped_type> find_value(const T& map, const typename T::key_type& key)
{
	auto iter = map.find(key);
	if(iter != map.end()) {
		return iter->second;
	}
	return nullptr;
}


} // mmx

#endif /* INCLUDE_MMX_HELPERS_H_ */
