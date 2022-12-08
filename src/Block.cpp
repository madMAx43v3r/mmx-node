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
	if(!farmer_sig && height) {
		if(nonce) {
			return false;
		}
		if(proof || tx_base || tx_list.size()) {
			return false;
		}
	}
	uint64_t static_cost_sum = 0;
	uint64_t total_cost_sum = 0;
	uint64_t tx_fees_sum = 0;
	for(const auto& tx : tx_list) {
		if(const auto& res = tx->exec_result) {
			total_cost_sum += res->total_cost;
			tx_fees_sum += res->total_fee;
		} else if(height) {
			return false;
		}
		static_cost_sum += tx->static_cost;
	}
	return BlockHeader::is_valid()
			&& static_cost == static_cost_sum
			&& total_cost == total_cost_sum
			&& tx_fees == tx_fees_sum
			&& tx_count == tx_list.size()
			&& tx_hash == calc_tx_hash();
}

hash_t Block::calc_tx_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	buffer.reserve(1024 * 1024);

	for(const auto& tx : tx_list) {
		write_bytes(out, tx ? tx->calc_hash(true) : hash_t());
	}
	out.flush();

	return hash_t(buffer);
}

void Block::finalize()
{
	static_cost = 0;
	total_cost = 0;
	tx_fees = 0;
	for(const auto& tx : tx_list) {
		if(const auto& res = tx->exec_result) {
			total_cost += res->total_cost;
			tx_fees += res->total_fee;
		}
		static_cost += tx->static_cost;
	}
	tx_count = tx_list.size();
	tx_hash = calc_tx_hash();
	const auto all_hash = calc_hash();
	hash = all_hash.first;
	content_hash = all_hash.second;
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


} // mmx
