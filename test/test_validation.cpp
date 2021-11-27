/*
 * test_validation.cpp
 *
 *  Created on: Nov 27, 2021
 *      Author: mad
 */

#include <mmx/Block.hxx>
#include <mmx/skey_t.hpp>
#include <mmx/pubkey_t.hpp>
#include <mmx/solution/PubKey.hxx>

#include <vnx/vnx.h>

using namespace mmx;


static constexpr uint64_t COIN = 1000000;

std::unordered_map<hash_t, std::shared_ptr<const Contract>> contract_map;

std::unordered_map<hash_t, std::unordered_map<size_t, tx_out_t>> utxo_map;


hash_t validate(std::shared_ptr<const Transaction> tx, bool is_base = false)
{
	const auto id = tx->calc_hash();

	bls::AugSchemeMPL MPL;
	uint64_t base_amount = 0;
	std::unordered_map<hash_t, uint64_t> amounts;

	for(const auto& in : tx->inputs)
	{
		auto iter = utxo_map.find(in.prev_tx);
		if(iter == utxo_map.end()) {
			throw std::logic_error("invalid prev_tx");
		}
		const auto& entry = iter->second;

		auto iter2 = entry.find(in.index);
		if(iter2 == entry.end()) {
			throw std::logic_error("invalid index");
		}
		const auto& out = iter2->second;

		// verify signature
		if(!in.solution) {
			throw std::logic_error("no solution");
		}
		{
			auto iter = contract_map.find(out.address);
			if(iter != contract_map.end()) {
				auto contract = iter->second;
				// TODO
			}
			else if(auto sol = std::dynamic_pointer_cast<const solution::PubKey>(in.solution))
			{
				if(sol->pubkey.get_addr() != out.address) {
					throw std::logic_error("wrong pubkey");
				}
				const auto G1 = sol->pubkey.to_bls();
				const auto G2 = sol->signature.to_bls();

				if(!MPL.Verify(G1, bls::Bytes(id.bytes.data(), id.bytes.size()), G2)) {
					throw std::logic_error("wrong signature");
				}
			}
			else {
				throw std::logic_error("invalid solution");
			}
		}
		amounts[out.contract] += out.amount;
	}
	for(const auto& op : tx->execute)
	{
		// TODO
	}
	for(const auto& out : tx->outputs)
	{
		if(is_base) {
			if(out.contract != hash_t()) {
				throw std::logic_error("invalid coin base contract");
			}
			base_amount += out.amount;
		}
		else {
			auto iter = amounts.find(out.contract);
			if(iter == amounts.end()) {
				throw std::logic_error("invalid output contract");
			}
			auto& value = iter->second;
			if(out.amount > value) {
				throw std::logic_error("output > input");
			}
			value -= out.amount;
		}
	}
	return id;
}

void process(std::shared_ptr<const Transaction> tx, const hash_t& tx_id, bool is_base = false)
{
	for(const auto& in : tx->inputs)
	{
		auto iter = utxo_map.find(in.prev_tx);
		if(iter != utxo_map.end()) {
			auto& entry = iter->second;
			auto iter2 = entry.find(in.index);
			if(iter2 != entry.end()) {
				const auto& out = iter2->second;
				entry.erase(iter2);
			}
			if(entry.empty()) {
				utxo_map.erase(iter);
			}
		}
	}
	auto& entry = utxo_map[tx_id];
	entry.reserve(tx->outputs.size());

	for(size_t i = 0; i < tx->outputs.size(); ++i) {
		entry[i] = tx->outputs[i];
	}
}

void process(std::shared_ptr<const Block> block)
{
	if(!block->is_valid()) {
		throw std::logic_error("invalid block hash");
	}
	if(auto tx = block->coin_base) {
		const auto id = validate(tx, true);
		process(tx, id, true);
	}
	std::vector<hash_t> tx_id(block->tx_list.size());

#pragma omp parallel for
	for(size_t i = 0; i < block->tx_list.size(); ++i) {
		tx_id[i] = validate(block->tx_list[i]);
	}
	for(size_t i = 0; i < block->tx_list.size(); ++i) {
		process(block->tx_list[i], tx_id[i]);
	}
}

int main(int argc, char** argv)
{
	vnx::init("test_validation", argc, argv);

	int num_keys = 1024;
	int num_blocks = 10;
	vnx::read_config("nkeys", num_keys);
	vnx::read_config("nblocks", num_blocks);

	vector<uint8_t> seed(32);

	bls::AugSchemeMPL MPL;
	const bls::PrivateKey master_sk = MPL.KeyGen(seed);

	std::vector<bls::PrivateKey> keys;
	for(int i = 0; i < num_keys; ++i) {
		keys.push_back(MPL.DeriveChildSk(master_sk, i));
	}

	std::vector<pubkey_t> pubkeys;
	std::unordered_map<pubkey_t, skey_t> skey_map;
	for(const auto& key : keys) {
		const auto G1 = key.GetG1Element();
		skey_map[G1] = key;
		pubkeys.emplace_back(G1);
	}

	std::vector<hash_t> addrs;
	std::unordered_map<hash_t, pubkey_t> pubkey_map;
	for(const auto& key : pubkeys) {
		const auto addr = key.get_addr();
		pubkey_map[addr] = key;
		addrs.push_back(addr);
	}

	auto genesis = Block::create();
	{
		auto base = Transaction::create();
		for(int i = 0; i < num_keys; ++i) {
			tx_out_t out;
			out.address = addrs[i];
			out.amount = 1 * COIN;
			base->outputs.push_back(out);
		}
		genesis->coin_base = base;
	}
	genesis->finalize();

	std::cout << "Genesis block hash: " << genesis->hash << std::endl;

	process(genesis);

	auto prev = genesis;
	for(int b = 0; b < num_blocks; ++b)
	{
		auto block = Block::create();
		block->prev = prev->hash;

		for(const auto& entry : utxo_map)
		{
			auto tx = Transaction::create();
			const auto& src = entry.second.at(0);
			const auto dst_addr = addrs[rand() % addrs.size()];
			{
				tx_out_t out;
				out.address = dst_addr;
				out.amount = src.amount;
				tx->outputs.push_back(out);
			}
			{
				tx_in_t in;
				in.prev_tx = entry.first;
				in.index = 0;
				tx->inputs.push_back(in);
			}
			const auto id = tx->calc_hash();

			for(auto& in : tx->inputs)
			{
				auto sol = solution::PubKey::create();
				sol->pubkey = pubkey_map[src.address];
				sol->signature = MPL.Sign(skey_map[sol->pubkey].to_bls(), bls::Bytes(id.bytes.data(), id.bytes.size()));
				in.solution = sol;
			}
		}
		block->finalize();

		std::cout << "Block hash: " << block->hash << std::endl;

		process(block);

		prev = block;
	}

	vnx::close();

	return 0;
}






