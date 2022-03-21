/*
 * PuzzleLock.cpp
 *
 *  Created on: Mar 13, 2022
 *      Author: mad
 */

#include <mmx/contract/PubKey.hxx>
#include <mmx/contract/PuzzleLock.hxx>
#include <mmx/solution/PuzzleLock.hxx>
#include <mmx/operation/Spend.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace contract {

vnx::bool_t PuzzleLock::is_valid() const {
	return Locked::is_valid() && puzzle;
}

hash_t PuzzleLock::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_bytes(out, version);
	write_bytes(out, owner);
	write_bytes(out, chain_height);
	write_bytes(out, delta_height);
	write_bytes(out, puzzle ? puzzle->calc_hash() : hash_t());
	write_bytes(out, target);
	out.flush();

	return hash_t(buffer);
}

uint64_t PuzzleLock::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	return Locked::calc_cost() + (puzzle ? puzzle->calc_cost(params) : 0) + 32 * params->min_txfee_byte;
}

std::vector<addr_t> PuzzleLock::get_dependency() const {
	return {owner, target};
}

std::vector<addr_t> PuzzleLock::get_parties() const {
	return {owner, target};
}

std::vector<tx_out_t> PuzzleLock::validate(std::shared_ptr<const Operation> operation, std::shared_ptr<const Context> context) const
{
	if(auto claim = std::dynamic_pointer_cast<const solution::PuzzleLock>(operation->solution))
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
	return Locked::validate(operation, context);
}


} // contract
} // mmx
