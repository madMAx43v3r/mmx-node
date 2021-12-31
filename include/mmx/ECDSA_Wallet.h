/*
 * ECDSA_Wallet.h
 *
 *  Created on: Dec 11, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_ECDSA_WALLET_H_
#define INCLUDE_MMX_ECDSA_WALLET_H_

#include <mmx/KeyFile.hxx>
#include <mmx/Transaction.hxx>
#include <mmx/ChainParams.hxx>
#include <mmx/solution/PubKey.hxx>

#include <mmx/skey_t.hpp>
#include <mmx/addr_t.hpp>
#include <mmx/pubkey_t.hpp>
#include <mmx/txio_key_t.hpp>
#include <mmx/utxo_entry_t.hpp>


namespace mmx {

class ECDSA_Wallet {
public:
	int64_t last_utxo_update = 0;
	std::vector<utxo_entry_t> utxo_cache;
	std::unordered_set<txio_key_t> spent_txo_set;
	std::unordered_map<txio_key_t, addr_t> addr_map;
	std::unordered_map<txio_key_t, tx_out_t> utxo_change_cache;

	ECDSA_Wallet(std::shared_ptr<const KeyFile> key_file, std::shared_ptr<const ChainParams> params, size_t num_addresses)
		:	params(params)
	{
		master_sk = hash_t(key_file->seed_value);

		for(size_t i = 0; i < num_addresses + 1; ++i)
		{
			const auto keys = generate_keypair(i);
			const auto addr = keys.second;
			keypair_map[addr] = keys;
			keypairs.push_back(keys);
			addresses.push_back(addr);
			index_map[addr] = i;
		}
	}

	skey_t get_skey(const uint32_t index) const
	{
		return keypairs.at(index).first;
	}

	pubkey_t get_pubkey(const uint32_t index) const
	{
		return keypairs.at(index).second;
	}

	addr_t get_address(const uint32_t index) const
	{
		return addresses.at(index);
	}

	std::vector<addr_t> get_all_addresses() const
	{
		return addresses;
	}

	ssize_t find_address(const addr_t& address) const
	{
		auto iter = index_map.find(address);
		if(iter != index_map.end()) {
			return iter->second;
		}
		return -1;
	}

	std::pair<skey_t, pubkey_t> get_keypair(const uint32_t index) const
	{
		return keypairs.at(index);
	}

	std::pair<skey_t, pubkey_t> get_keypair(const addr_t& addr) const
	{
		auto iter = keypair_map.find(addr);
		if(iter == keypair_map.end()) {
			throw std::logic_error("unknown address");
		}
		return iter->second;
	}

	skey_t generate_skey(const std::vector<uint32_t>& path) const
	{
		skey_t key = master_sk;
		for(const uint32_t i : path) {
			key = hash_t(hash_t(key + hash_t(&i, sizeof(i))) + hash_t(master_sk.bytes));
		}
		return key;
	}

	std::pair<skey_t, pubkey_t> generate_keypair(const std::vector<uint32_t>& path) const
	{
		std::pair<skey_t, pubkey_t> keys;
		keys.first = generate_skey(path);
		keys.second = pubkey_t::from_skey(keys.first);
		return keys;
	}

	std::pair<skey_t, pubkey_t> generate_keypair(const uint32_t index) const
	{
		return generate_keypair({0, index});
	}

	void update_cache(const std::vector<utxo_entry_t>& utxo_list)
	{
		utxo_cache.clear();
		for(const auto& entry : utxo_list) {
			if(!spent_txo_set.count(entry.key)) {
				utxo_cache.push_back(entry);
			}
			utxo_change_cache.erase(entry.key);
		}

		// add pending change outputs
		for(const auto& entry : utxo_change_cache) {
			utxo_cache.push_back(utxo_entry_t::create_ex(entry.first, utxo_t::create_ex(entry.second)));
		}

		// create lookup map
		addr_map.clear();
		for(const auto& entry : utxo_cache) {
			addr_map[entry.key] = entry.output.address;
		}

		// sort by height, such as to use oldest coins first (which are more likely to be spend-able right now)
		std::sort(utxo_cache.begin(), utxo_cache.end(),
			[](const utxo_entry_t& lhs, const utxo_entry_t& rhs) -> bool {
				return lhs.output.height < rhs.output.height;
			});
	}

	void update_from(std::shared_ptr<const Transaction> tx)
	{
		for(const auto& in : tx->inputs) {
			spent_txo_set.insert(in.prev);
			utxo_change_cache.erase(in.prev);
		}

		// remove spent utxo from cache
		std::vector<utxo_entry_t> utxo_list;
		for(const auto& entry : utxo_cache) {
			if(!spent_txo_set.count(entry.key)) {
				utxo_list.push_back(entry);
			}
		}
		utxo_cache = std::move(utxo_list);

		// add change outputs to cache
		for(size_t i = 0; i < tx->outputs.size(); ++i) {
			const auto& out = tx->outputs[i];
			if(find_address(out.address) >= 0) {
				const auto key = txio_key_t::create_ex(tx->id, i);
				utxo_change_cache[key] = out;
				utxo_cache.push_back(utxo_entry_t::create_ex(key, utxo_t::create_ex(out)));
			}
		}
	}

	uint64_t gather_inputs(	std::shared_ptr<Transaction> tx,
							std::unordered_set<txio_key_t>& tmp_spent_txo,
							uint64_t amount, const addr_t& contract)
	{
		uint64_t change = 0;
		for(const auto& entry : utxo_cache)
		{
			if(amount == 0) {
				break;
			}
			const auto& out = entry.output;
			if(out.contract != contract) {
				continue;
			}
			const auto& key = entry.key;
			if(tmp_spent_txo.count(key) || spent_txo_set.count(key)) {
				continue;
			}
			if(out.amount > amount) {
				change += out.amount - amount;
				amount = 0;
			} else {
				amount -= out.amount;
			}
			tx_in_t in;
			in.prev = key;
			tx->inputs.push_back(in);
			tmp_spent_txo.insert(key);
		}
		if(amount != 0) {
			throw std::logic_error("not enough funds");
		}
		return change;
	}

	std::shared_ptr<Transaction> send(const uint64_t& amount, const addr_t& dst_addr, const addr_t& contract)
	{
		auto tx = Transaction::create();
		{
			// add primary output
			tx_out_t out;
			out.address = dst_addr;
			out.contract = contract;
			out.amount = amount;
			tx->outputs.push_back(out);
		}
		std::unordered_set<txio_key_t> tmp_spent_txo;

		uint64_t change = gather_inputs(tx, tmp_spent_txo, amount, contract);

		if(contract != addr_t() && change > 0) {
			// token change cannot be used as tx fee
			tx_out_t out;
			out.address = get_address(0);
			out.contract = contract;
			out.amount = change;
			tx->outputs.push_back(out);
			change = 0;
		}

		// gather inputs for tx fee
		uint64_t tx_fees = 0;
		while(true) {
			// count number of solutions needed
			std::unordered_set<addr_t> used_addr;
			for(const auto& in : tx->inputs) {
				auto iter = addr_map.find(in.prev);
				if(iter == addr_map.end()) {
					throw std::logic_error("cannot sign input");
				}
				used_addr.insert(iter->second);
			}
			tx_fees = tx->calc_min_fee(params)
					+ params->min_txfee_io	// for change output
					+ used_addr.size() * params->min_txfee_sign;

			if(change > tx_fees) {
				// we got more than enough, add change output
				tx_out_t out;
				out.address = get_address(0);
				out.amount = change - tx_fees;
				tx->outputs.push_back(out);
				change = 0;
				break;
			}
			if(change == tx_fees) {
				// perfect match
				change = 0;
				break;
			}
			// gather more
			const auto left = tx_fees - change;
			change += gather_inputs(tx, tmp_spent_txo, left, addr_t());
			change += left;
		}
		tx->finalize();

		std::unordered_map<addr_t, uint32_t> solution_map;

		for(auto& in : tx->inputs)
		{
			// sign all inputs
			auto iter = addr_map.find(in.prev);
			if(iter == addr_map.end()) {
				throw std::logic_error("cannot sign input");
			}
			auto iter2 = solution_map.find(iter->second);
			if(iter2 != solution_map.end()) {
				// re-use solution
				in.solution = iter2->second;
				continue;
			}
			const auto& keys = get_keypair(iter->second);

			auto sol = solution::PubKey::create();
			sol->pubkey = keys.second;
			sol->signature = signature_t::sign(keys.first, tx->id);

			in.solution = tx->solutions.size();
			solution_map[iter->second] = in.solution;
			tx->solutions.push_back(sol);
		}
		return tx;
	}

private:
	skey_t master_sk;
	std::vector<addr_t> addresses;
	std::unordered_map<addr_t, size_t> index_map;
	std::vector<std::pair<skey_t, pubkey_t>> keypairs;
	std::unordered_map<addr_t, std::pair<skey_t, pubkey_t>> keypair_map;

	std::shared_ptr<const ChainParams> params;

};


} // mmx

#endif /* INCLUDE_MMX_ECDSA_WALLET_H_ */
