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


namespace mmx {
namespace contract {

vnx::bool_t MultiSig::is_valid() const {
	return Contract::is_valid() && num_required > 0 && owners.size() >= num_required;
}

hash_t MultiSig::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_bytes(out, version);
	for(const auto& address : owners) {
		write_bytes(out, address);
	}
	out.flush();

	return hash_t(buffer);
}

uint64_t MultiSig::calc_min_fee(std::shared_ptr<const ChainParams> params) const {
	return (8 + 4 + 32 * owners.size()) * params->min_txfee_byte;
}

std::vector<addr_t> MultiSig::get_dependency() const {
	return {};
}

std::vector<tx_out_t> MultiSig::validate(std::shared_ptr<const Operation> operation, std::shared_ptr<const Context> context) const
{
	if(!operation) {
		throw std::logic_error("!operation");
	}
	if(auto solution = std::dynamic_pointer_cast<const solution::MultiSig>(operation->solution))
	{
		if(solution->solutions.size() != owners.size()) {
			throw std::logic_error("solutions count mismatch");
		}
		uint32_t count = 0;
		for(size_t i = 0; i < owners.size(); ++i)
		{
			if(const auto& sol = solution->solutions[i])
			{
				if(sol->pubkey.get_addr() != owners[i]) {
					throw std::logic_error("invalid pubkey at index " + std::to_string(i));
				}
				if(!sol->signature.verify(sol->pubkey, context->txid)) {
					throw std::logic_error("invalid signature at index " + std::to_string(i));
				}
				count++;
			}
		}
		if(count < num_required) {
			throw std::logic_error("insufficient signatures: " + std::to_string(count) + " < " + std::to_string(num_required));
		}
		return {};
	}
	throw std::logic_error("invalid solution");
}


} // contract
} // mmx
