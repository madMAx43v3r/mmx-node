/*
 * Staking.cpp
 *
 *  Created on: Jan 16, 2022
 *      Author: mad
 */

#include <mmx/contract/Token.hxx>
#include <mmx/contract/Staking.hxx>
#include <mmx/operation/Spend.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace contract {

vnx::bool_t Staking::is_valid() const {
	return Contract::is_valid() && owner != addr_t() && reward_addr != addr_t();
}

hash_t Staking::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_bytes(out, version);
	write_bytes(out, owner);
	write_bytes(out, currency);
	write_bytes(out, reward_addr);
	out.flush();

	return hash_t(buffer);
}

uint64_t Staking::calc_cost(std::shared_ptr<const ChainParams> params) const {
	return (8 + 4 + 32 * 3) * params->min_txfee_byte;
}

std::vector<addr_t> Staking::get_dependency() const {
	return {owner, currency};
}

std::vector<addr_t> Staking::get_parties() const {
	return {owner};
}

vnx::optional<addr_t> Staking::get_owner() const {
	return owner;
}

std::vector<tx_out_t> Staking::validate(std::shared_ptr<const Operation> operation, std::shared_ptr<const Context> context) const
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
		const auto num_blocks = context->height - spend->utxo.height;
		if(num_blocks >= min_duration)
		{
			if(auto token = std::dynamic_pointer_cast<const Token>(context->get_contract(currency)))
			{
				auto iter = token->stake_factors.find(spend->utxo.contract);
				if(iter != token->stake_factors.end())
				{
					tx_out_t out;
					out.contract = currency;
					out.address = reward_addr;
					const auto& factor = iter->second;
					out.amount = ((uint256_t(num_blocks) * spend->utxo.amount) * factor.value) / factor.inverse;
					if(out.amount) {
						return {out};
					}
				}
			}
		}
	}
	return {};
}


} // contract
} // mmx
