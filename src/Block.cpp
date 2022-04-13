/*
 * Block.cpp
 *
 *  Created on: Nov 27, 2021
 *      Author: mad
 */

#include <mmx/Block.hxx>
#include <mmx/write_bytes.h>
#include <mmx/Transaction.hxx>


namespace mmx {

void Block::finalize()
{
	tx_count = tx_list.size();
	tx_hash = calc_tx_hash();
	hash = calc_hash();
}

vnx::bool_t Block::is_valid() const
{
	if(tx_base && !std::dynamic_pointer_cast<const Transaction>(tx_base)) {
		return false;
	}
	for(const auto& base : tx_list) {
		if(!std::dynamic_pointer_cast<const Transaction>(base)) {
			return false;
		}
	}
	return BlockHeader::is_valid() && (!proof || proof->is_valid()) && calc_tx_hash() == tx_hash;
}

mmx::hash_t Block::calc_tx_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	buffer.reserve(1024 * 1024);

	for(const auto& tx : tx_list) {
		write_bytes(out, tx ? tx->calc_hash() : hash_t());
	}
	out.flush();

	return hash_t(buffer);
}

uint64_t Block::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	uint64_t cost = 0;
	for(const auto& tx : tx_list) {
		if(tx) {
			cost += tx->calc_cost(params);
		}
	}
	return cost;
}

std::shared_ptr<const BlockHeader> Block::get_header() const
{
	auto header = BlockHeader::create();
	header->operator=(*this);
	return header;
}


} // mmx
