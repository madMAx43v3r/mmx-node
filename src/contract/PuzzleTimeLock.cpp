/*
 * PuzzleTimeLock.cpp
 *
 *  Created on: Mar 13, 2022
 *      Author: mad
 */

#include <mmx/contract/PubKey.hxx>
#include <mmx/contract/PuzzleTimeLock.hxx>
#include <mmx/solution/PuzzleTimeLock.hxx>
#include <mmx/operation/Spend.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace contract {

vnx::bool_t PuzzleTimeLock::is_valid() const
{
	return Super::is_valid() && puzzle;
}

hash_t PuzzleTimeLock::calc_hash(const vnx::bool_t& full_hash) const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", version);
	write_field(out, "owner", 	owner);
	write_field(out, "unlock_height", unlock_height);
	write_field(out, "puzzle", puzzle ? puzzle->calc_hash() : hash_t());
	write_field(out, "target", target);
	out.flush();

	return hash_t(buffer);
}

uint64_t PuzzleTimeLock::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	return Super::calc_cost() + (puzzle ? puzzle->calc_cost(params) : 0);
}

std::vector<addr_t> PuzzleTimeLock::get_dependency() const {
	return {owner, target};
}

std::vector<txout_t> PuzzleTimeLock::validate(std::shared_ptr<const Operation> operation, std::shared_ptr<const Context> context) const
{
	if(auto claim = std::dynamic_pointer_cast<const solution::PuzzleTimeLock>(operation->solution))
	{
		if(puzzle) {
			auto op = vnx::clone(operation);
			op->solution = claim->puzzle;
			puzzle->validate(op, context);
		}
		{
			auto contract = context->get_contract(target);
			if(!contract) {
				throw std::logic_error("missing dependency");
			}
			auto op = vnx::clone(operation);
			op->solution = claim->target;
			contract->validate(op, context);
		}
		if(!std::dynamic_pointer_cast<const operation::Spend>(operation)) {
			throw std::logic_error("invalid operation");
		}
		return {};
	}
	return Super::validate(operation, context);
}


} // contract
} // mmx
