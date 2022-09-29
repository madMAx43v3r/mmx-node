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
	uint64_t total_cost = 0;
	uint64_t total_fees = 0;
	for(const auto& tx : tx_list) {
		if(const auto& res = tx->exec_result) {
			total_cost += res->total_cost;
			total_fees += res->total_fee;
		}
	}
	return BlockHeader::is_valid()
			&& tx_cost == total_cost
			&& tx_fees == total_fees
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
	tx_cost = 0;
	tx_fees = 0;
	for(const auto& tx : tx_list) {
		if(const auto& res = tx->exec_result) {
			tx_cost += res->total_cost;
			tx_fees += res->total_fee;
		}
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

void Block::validate() const
{
	Super::validate();

	uint64_t total_tx_cost = 0;
	uint64_t total_tx_fees = 0;
	for(const auto& tx : tx_list) {
		if(!tx || !tx->exec_result) {
			throw std::logic_error("missing tx exec_result");
		}
		total_tx_cost += tx->exec_result->total_cost;
		total_tx_fees += tx->exec_result->total_fee;
	}
	if(tx_cost != total_tx_cost) {
		throw std::logic_error("invalid tx_cost");
	}
	if(tx_fees != total_tx_fees) {
		throw std::logic_error("invalid tx_fees");
	}
	if(!farmer_sig) {
		if(nonce) {
			throw std::logic_error("invalid block nonce");
		}
		if(tx_base || tx_list.size()) {
			throw std::logic_error("cannot have transactions");
		}
	}
}


} // mmx
