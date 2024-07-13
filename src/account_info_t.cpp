/*
 * account_info_t.cpp
 *
 *  Created on: Jun 28, 2024
 *      Author: mad
 */

#include <mmx/account_info_t.hxx>


namespace mmx {

account_info_t account_info_t::make(const uint32_t& account, const vnx::optional<addr_t>& address, const account_t& config)
{
	account_info_t out;
	static_cast<account_t&>(out) = config;
	out.account = account;
	out.address = address;
	return out;
}


} // mmx
