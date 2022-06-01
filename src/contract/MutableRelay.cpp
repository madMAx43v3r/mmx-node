/*
 * MutableRelay.cpp
 *
 *  Created on: Mar 14, 2022
 *      Author: mad
 */

#include <mmx/contract/MutableRelay.hxx>
#include <mmx/contract/MutableRelay_unlock.hxx>
#include <mmx/operation/Spend.hxx>
#include <mmx/operation/Mutate.hxx>
#include <mmx/solution/MutableRelay.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace contract {

vnx::bool_t MutableRelay::is_valid() const
{
	return Super::is_valid() && owner != addr_t();
}

hash_t MutableRelay::calc_hash(const vnx::bool_t& full_hash) const
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
	out.flush();

	return hash_t(buffer);
}

uint64_t MutableRelay::calc_cost(std::shared_ptr<const ChainParams> params) const {
	return 0;
}

std::vector<addr_t> MutableRelay::get_dependency() const {
	if(target) {
		return {owner, *target};
	}
	return {owner};
}

vnx::optional<addr_t> MutableRelay::get_owner() const {
	return owner;
}

vnx::bool_t MutableRelay::is_locked(std::shared_ptr<const Context> context) const {
	return !unlock_height || context->height < *unlock_height;
}

std::vector<txout_t> MutableRelay::validate(std::shared_ptr<const Operation> operation, std::shared_ptr<const Context> context) const
{
	if(auto claim = std::dynamic_pointer_cast<const solution::MutableRelay>(operation->solution))
	{
		if(!target) {
			throw std::logic_error("!target");
		}
		if(unlock_height && context->height >= *unlock_height) {
			throw std::logic_error("contract is not locked");
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
		if(auto method = std::dynamic_pointer_cast<const contract::MutableRelay_unlock>(mutate->method.to_value()))
		{
			if(method->height < context->height + unlock_delay) {
				throw std::logic_error("invalid unlock height");
			}
			return {};
		}
	}
	if(target) {
		if(is_locked(context)) {
			throw std::logic_error("contract is locked");
		}
	}
	return {};
}

void MutableRelay::transfer(const vnx::optional<addr_t>& new_owner)
{
	if(new_owner) {
		owner = *new_owner;
	} else {
		throw std::logic_error("!new_owner");
	}
}

void MutableRelay::unlock(const uint32_t& height)
{
	unlock_height = height;
}

void MutableRelay::lock(const vnx::optional<addr_t>& new_target, const uint32_t& new_unlock_delay)
{
	target = new_target;
	unlock_delay = new_unlock_delay;
	unlock_height = nullptr;
}


} // contract
} // mmx
