/*
 * Block.cpp
 *
 *  Created on: Nov 27, 2021
 *      Author: mad
 */

#include <mmx/Block.hxx>
#include <mmx/write_bytes.h>
#include <mmx/Transaction.hxx>
#include <mmx/txio_entry_t.hpp>


namespace mmx {

vnx::bool_t Block::is_valid() const
{
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

std::vector<std::shared_ptr<const Transaction>> Block::get_transactions() const {
	return tx_list;
}

std::vector<txio_entry_t> Block::get_inputs(std::shared_ptr<const ChainParams> params) const
{
	std::vector<txio_entry_t> res;
	for(const auto& tx : tx_list) {
		if(tx->exec_result && tx->sender) {
			txio_t in;
			in.address = *tx->sender;
			in.amount = tx->exec_result->total_fee;
			res.push_back(txio_entry_t::create_ex(tx->id, height, tx_type_e::TXFEE, in));
		}
		if(!tx->exec_result || !tx->exec_result->did_fail) {
			for(const auto& in : tx->get_inputs()) {
				res.push_back(txio_entry_t::create_ex(tx->id, height, tx_type_e::SPEND, in));
			}
		}
	}
	return res;
}

std::vector<txio_entry_t> Block::get_outputs(std::shared_ptr<const ChainParams> params) const
{
	std::vector<txio_entry_t> res;

	if(vdf_reward) {
		txio_t out;
		out.address = vdf_reward;
		out.amount = time_diff / params->vdf_reward_divider;
		res.push_back(txio_entry_t::create_ex(hash_t(), height, tx_type_e::VDF_REWARD, out));
	}
	if(reward_addr) {
		txio_t out;
		out.address = *reward_addr;
		out.amount = reward_amount;
		res.push_back(txio_entry_t::create_ex(hash_t(), height, tx_type_e::REWARD, out));

		// project reward
		out.address = params->project_addr;
		out.amount = params->fixed_project_reward + (params->project_ratio.value * out.amount) / params->project_ratio.inverse;
		res.push_back(txio_entry_t::create_ex(hash_t(), height, tx_type_e::PROJECT_REWARD, out));
	}
	for(const auto& tx : tx_list) {
		if(!tx->exec_result || !tx->exec_result->did_fail) {
			for(const auto& out : tx->get_outputs()) {
				txio_entry_t::create_ex(tx->id, height, tx_type_e::RECEIVE, out);
			}
		}
	}
	return res;
}


} // mmx
