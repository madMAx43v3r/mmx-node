/*
 * method_t.cpp
 *
 *  Created on: May 15, 2022
 *      Author: mad
 */

#include <mmx/contract/method_t.hxx>


namespace mmx {
namespace contract {

uint64_t method_t::num_bytes() const
{
	uint64_t payload = 3 + 4 + 4 * 3 + name.size() + info.size() + args.size() * 4;
	for(const auto& entry : args) {
		payload += entry.size();
	}
	return payload;
}


} // contract
} // mmx
