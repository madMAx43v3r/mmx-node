/*
 * tree_hash.h
 *
 *  Created on: Apr 20, 2024
 *      Author: mad
 */

#ifndef INCLUDE_MMX_TREE_HASH_H_
#define INCLUDE_MMX_TREE_HASH_H_

#include <mmx/hash_t.hpp>

#include <vector>


namespace mmx {

inline
hash_t calc_btree_hash(const std::vector<hash_t>& input)
{
	auto tmp = input;
	while(tmp.size() > 1) {
		std::vector<hash_t> next;
		for(size_t i = 0; i < tmp.size(); i += 2)
		{
			if(i + 1 < tmp.size()) {
				next.emplace_back(tmp[i] + tmp[i + 1]);
			} else {
				next.push_back(tmp[i]);
			}
		}
		tmp = next;
	}
	return tmp.empty() ? hash_t() : tmp[0];
}


} // mmx

#endif /* INCLUDE_MMX_TREE_HASH_H_ */
