/*
 * PlotNFT.cpp
 *
 *  Created on: May 5, 2022
 *      Author: mad
 */

#include <mmx/contract/PlotNFT.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace contract {

vnx::bool_t PlotNFT::is_valid() const
{
	return Super::is_valid() && unlock_delay <= MAX_UNLOCK_DELAY;
}

hash_t PlotNFT::calc_hash(const vnx::bool_t& full_hash) const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", version);
	write_field(out, "owner", 	owner);
	write_field(out, "target", 	target);
	write_field(out, "unlock_height", 	unlock_height);
	write_field(out, "unlock_delay", 	unlock_delay);
	write_field(out, "name", 	name);
	write_field(out, "server_url", 		server_url);
	out.flush();

	return hash_t(buffer);
}

uint64_t PlotNFT::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	return Super::calc_cost() + (name.size() + (server_url ? server_url->size() : 0)) * params->min_txfee_byte;
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


