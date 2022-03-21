/*
 * PlotNFT.cpp
 *
 *  Created on: Mar 14, 2022
 *      Author: mad
 */

#include <mmx/contract/PlotNFT.hxx>
#include <mmx/contract/PlotNFT_unlock.hxx>
#include <mmx/operation/Spend.hxx>
#include <mmx/operation/Mutate.hxx>
#include <mmx/solution/PlotNFT.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace contract {

vnx::bool_t PlotNFT::is_valid() const {
	return Contract::is_valid() && owner != addr_t();
}

hash_t PlotNFT::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_bytes(out, version);
	write_bytes(out, owner);
	write_bytes(out, target);
	write_bytes(out, unlock_height);
	out.flush();

	return hash_t(buffer);
}

uint64_t PlotNFT::calc_cost(std::shared_ptr<const ChainParams> params) const {
	return (8 + 4 + 2 * 32 + 4) * params->min_txfee_byte;
}

std::vector<addr_t> PlotNFT::get_dependency() const {
	if(target) {
		return {owner, *target};
	}
	return {owner};
}

std::vector<addr_t> PlotNFT::get_parties() const {
	return get_dependency();
}

vnx::optional<addr_t> PlotNFT::get_owner() const {
	return owner;
}

std::vector<tx_out_t> PlotNFT::validate(std::shared_ptr<const Operation> operation, std::shared_ptr<const Context> context) const
{
	if(auto claim = std::dynamic_pointer_cast<const solution::PlotNFT>(operation->solution))
	{
		if(!target) {
			throw std::logic_error("!target");
		}
		auto contract = context->get_contract(*target);
		if(!contract) {
			throw std::logic_error("missing dependency");
		}
		auto op = vnx::clone(operation);
		op->solution = claim->target;
		contract->validate(op, context);

		if(!std::dynamic_pointer_cast<const operation::Spend>(operation)) {
			throw std::logic_error("invalid operation");
		}
		return {};
	}
	{
		auto contract = context->get_contract(owner);
		if(!contract) {
			throw std::logic_error("missing dependency");
		}
		contract->validate(operation, context);
	}
	if(auto mutate = std::dynamic_pointer_cast<const operation::Mutate>(operation))
	{
		if(auto method = std::dynamic_pointer_cast<const contract::PlotNFT_unlock>(mutate->method.to_value()))
		{
			if(method->height < context->height + UNLOCK_DELAY) {
				throw std::logic_error("invalid unlock height");
			}
			return {};
		}
	}
	if(target) {
		if(!unlock_height || context->height < *unlock_height) {
			throw std::logic_error("contract is locked");
		}
	}
	return {};
}

void PlotNFT::unlock(const uint32_t& height)
{
	unlock_height = height;
}

void PlotNFT::lock_target(const vnx::optional<addr_t>& new_target)
{
	target = new_target;
	unlock_height = nullptr;
}


} // contract
} // mmx
