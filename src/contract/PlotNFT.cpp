/*
 * PlotNFT.cpp
 *
 *  Created on: May 5, 2022
 *      Author: mad
 */

#include <mmx/contract/PlotNFT.hxx>


namespace mmx {
namespace contract {

vnx::bool_t PlotNFT::is_valid() const
{
	return Super::is_valid() && unlock_delay <= MAX_UNLOCK_DELAY;
}

void PlotNFT::lock(const vnx::optional<addr_t>& new_target, const uint32_t& new_unlock_delay)
{
	Super::lock(new_target, new_unlock_delay);
	server_url = nullptr;
}

void PlotNFT::lock_pool(const vnx::optional<addr_t>& new_target, const uint32_t& new_unlock_delay,
						const vnx::optional<std::string>& new_server_url)
{
	Super::lock(new_target, new_unlock_delay);
	server_url = new_server_url;
}


} // contract
} // mmx


