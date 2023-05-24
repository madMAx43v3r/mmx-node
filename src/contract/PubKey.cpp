/*
 * PubKey.cpp
 *
 *  Created on: Dec 10, 2021
 *      Author: mad
 */

#include <mmx/contract/PubKey.hxx>
#include <mmx/solution/PubKey.hxx>
#include <mmx/write_bytes.h>
#include <mmx/exception.h>


namespace mmx {
namespace contract {

vnx::bool_t PubKey::is_valid() const
{
	return Super::is_valid() && address != addr_t();
}

hash_t PubKey::calc_hash(const vnx::bool_t& full_hash) const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", version);
	write_field(out, "address", address);
	out.flush();

	return hash_t(buffer);
}

uint64_t PubKey::calc_cost(std::shared_ptr<const ChainParams> params, const vnx::bool_t& is_read) const {
	return 0;
}

vnx::optional<addr_t> PubKey::get_owner() const {
	return address;
}

void PubKey::validate(std::shared_ptr<const Operation> operation, const hash_t& txid) const
{
	if(auto solution = std::dynamic_pointer_cast<const solution::PubKey>(operation->solution))
	{
		const auto sol_address = solution->pubkey.get_addr();
		if(sol_address != address) {
			throw mmx::invalid_solution("wrong pubkey: " + sol_address.to_string() + " != " + address.to_string());
		}
		if(!solution->signature.verify(solution->pubkey, txid)) {
			throw mmx::invalid_solution("invalid signature for " + address.to_string());
		}
		return;
	}
	if(operation->solution) {
		throw mmx::invalid_solution("invalid type");
	} else {
		throw mmx::invalid_solution("missing");
	}
}


} // contract
} // mmx
