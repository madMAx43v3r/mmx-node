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

vnx::bool_t Block::is_valid() const
{
	for(const auto& tx : tx_list) {
		if(!tx) {
			return false;
		}
	}
	return BlockHeader::is_valid() && (!proof || proof->is_valid()) && tx_count == tx_list.size() && calc_tx_hash() == tx_hash;
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

void Block::finalize()
{
	tx_count = tx_list.size();
	tx_hash = calc_tx_hash();
	hash = calc_hash();
}

uint64_t Block::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	uint128_t cost = 0;
	for(const auto& tx : tx_list) {
		if(tx) {
			cost += tx->calc_cost(params);
		}
	}
	if(cost.upper()) {
		throw std::logic_error("block cost amount overflow");
	}
	return cost;
}

std::shared_ptr<const BlockHeader> Block::get_header() const
{
	auto header = BlockHeader::create();
	header->operator=(*this);
	return header;
}

std::vector<std::shared_ptr<const Transaction>> Block::get_all_transactions() const
{
	std::vector<std::shared_ptr<const Transaction>> list;
	if(auto tx = tx_base) {
		list.push_back(tx);
	}
	list.insert(list.end(), tx_list.begin(), tx_list.end());
	return list;
}

void Block::validate() const
{
	Super::validate();

	if(!farmer_sig) {
		if(nonce) {
			throw std::logic_error("invalid block nonce");
		}
		if(proof) {
			// TODO: throw std::logic_error("proof without farmer_sig");
		}
		if(tx_base || tx_list.size()) {
			throw std::logic_error("cannot have transactions");
		}
	}
}


} // mmx
