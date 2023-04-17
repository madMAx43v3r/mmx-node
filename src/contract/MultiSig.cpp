/*
 * MultiSig.cpp
 *
 *  Created on: Jan 15, 2022
 *      Author: mad
 */

#include <mmx/contract/MultiSig.hxx>
#include <mmx/solution/MultiSig.hxx>
#include <mmx/solution/PubKey.hxx>
#include <mmx/write_bytes.h>
#include <mmx/exception.h>


namespace mmx {
namespace contract {

vnx::bool_t MultiSig::is_valid() const
{
	return Super::is_valid() && num_required > 0 && owners.size() >= num_required && owners.size() <= MAX_OWNERS;
}

hash_t MultiSig::calc_hash(const vnx::bool_t& full_hash) const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", version);
	write_field(out, "owners", owners);
	out.flush();

	return hash_t(buffer);
}

uint64_t MultiSig::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	return 32 * owners.size() * params->min_txfee_byte;
}

std::vector<txout_t> MultiSig::validate(std::shared_ptr<const Operation> operation, std::shared_ptr<const Context> context) const
{
	if(auto solution = std::dynamic_pointer_cast<const solution::PubKey>(operation->solution))
	{
		if(num_required != 1) {
			throw mmx::invalid_solution("num_required != 1");
		}
		const auto addr = solution->pubkey.get_addr();

		for(const auto& owner : owners) {
			if(addr == owner) {
				if(!solution->signature.verify(solution->pubkey, context->txid)) {
					throw mmx::invalid_solution("invalid signature");
				}
				return {};
			}
		}
		throw mmx::invalid_solution("no such owner");
	}
	if(auto solution = std::dynamic_pointer_cast<const solution::MultiSig>(operation->solution))
	{
		if(solution->solutions.size() != owners.size()) {
			throw mmx::invalid_solution("solutions count mismatch");
		}
		size_t count = 0;
		for(size_t i = 0; i < owners.size(); ++i)
		{
			if(const auto& sol = solution->solutions[i])
			{
				if(auto solution = std::dynamic_pointer_cast<const solution::PubKey>(sol))
				{
					if(solution->pubkey.get_addr() != owners[i]) {
						throw mmx::invalid_solution("wrong pubkey at index " + std::to_string(i));
					}
					if(!solution->signature.verify(solution->pubkey, context->txid)) {
						throw mmx::invalid_solution("invalid signature at index " + std::to_string(i));
					}
					count++;
				}
			}
		}
		if(count < num_required) {
			throw mmx::invalid_solution("insufficient signatures: " + std::to_string(count) + " < " + std::to_string(num_required));
		}
		return {};
	}
	throw mmx::invalid_solution("invalid type");
}


} // contract
} // mmx
