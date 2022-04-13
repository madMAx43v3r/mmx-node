/*
 * TimeLock.cpp
 *
 *  Created on: Mar 10, 2022
 *      Author: mad
 */

#include <mmx/contract/TimeLock.hxx>
#include <mmx/operation/Spend.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace contract {

vnx::bool_t TimeLock::is_valid() const
{
	return Super::is_valid() && owner != addr_t() && (chain_height || delta_height);
}

hash_t TimeLock::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", version);
	write_field(out, "owner", 	owner);
	write_field(out, "chain_height", chain_height);
	write_field(out, "delta_height", delta_height);
	out.flush();

	return hash_t(buffer);
}

uint64_t TimeLock::calc_cost(std::shared_ptr<const ChainParams> params) const {
	return 0;
}

std::vector<addr_t> TimeLock::get_dependency() const {
	return {owner};
}

std::vector<addr_t> TimeLock::get_parties() const {
	return {owner};
}

vnx::optional<addr_t> TimeLock::get_owner() const {
	return owner;
}

vnx::bool_t TimeLock::is_spendable(const utxo_t& utxo, std::shared_ptr<const Context> context) const
{
	if(chain_height && context->height >= *chain_height) {
		return true;
	}
	if(delta_height && context->height - utxo.height >= *delta_height) {
		return true;
	}
	return false;
}

std::vector<tx_out_t> TimeLock::validate(std::shared_ptr<const Operation> operation, std::shared_ptr<const Context> context) const
{
	{
		auto contract = context->get_contract(owner);
		if(!contract) {
			throw std::logic_error("missing dependency");
		}
		contract->validate(operation, context);
	}
	if(auto spend = std::dynamic_pointer_cast<const operation::Spend>(operation))
	{
		if(!is_spendable(spend->utxo, context)) {
			throw std::logic_error("invalid spend");
		}
	}
	return {};
}


} // contract
} // mmx
