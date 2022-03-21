/*
 * Context.cpp
 *
 *  Created on: Mar 14, 2022
 *      Author: mad
 */

#include <mmx/Context.hxx>


namespace mmx {

std::shared_ptr<const Contract> Context::get_contract(const addr_t& address) const
{
	auto iter = depends.find(address);
	if(iter != depends.end()) {
		return iter->second;
	}
	return nullptr;
}


} // mmx
