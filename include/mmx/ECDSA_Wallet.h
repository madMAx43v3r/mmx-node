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
#include <mmx/contract/NFT.hxx>
#include <mmx/operation/Mint.hxx>
#include <mmx/operation/Execute.hxx>
#include <mmx/operation/Deposit.hxx>
#include <mmx/solution/PubKey.hxx>
#include <mmx/spend_options_t.hxx>
#include <mmx/skey_t.hpp>
#include <mmx/addr_t.hpp>
#include <mmx/pubkey_t.hpp>
#include <mmx/txio_key_t.hpp>
#include <mmx/account_t.hxx>


namespace mmx {

class ECDSA_Wallet {
public:
	const account_t config;

	ECDSA_Wallet(	std::shared_ptr<const KeyFile> key_file, const account_t& config,
					std::shared_ptr<const ChainParams> params, const addr_t& genesis_hash)
		:	config(config), params(params), genesis_hash(genesis_hash)
	{
		if(key_file->seed_value == hash_t()) {
			throw std::logic_error("seed == zero");
		}
		master_sk = hash_t(key_file->seed_value);

		for(size_t i = 0; i < config.num_addresses; ++i)
		{
			const auto keys = generate_keypair(config.index, i);
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

	vnx::optional<std::pair<skey_t, pubkey_t>> find_keypair(const addr_t& addr) const
	{
		auto iter = keypair_map.find(addr);
		if(iter != keypair_map.end()) {
			return iter->second;
		}
		return nullptr;
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

	std::pair<skey_t, pubkey_t> generate_keypair(const uint32_t account, const uint32_t index) const
	{
		return generate_keypair({account, index});
	}

	void update_cache(	const std::map<std::pair<addr_t, addr_t>, uint128>& balances,
						const std::vector<tx_entry_t>& history, const uint32_t height)
	{
		this->height = height;
		balance_map.clear();
		balance_map.insert(balances.begin(), balances.end());

		for(const auto& entry : reserved_map) {
			balance_map[entry.first] -= entry.second;
		}
		for(const auto& entry : history) {
			pending_map.erase(entry.txid);
		}
		for(const auto& entry : pending_map) {
			for(const auto& pending : entry.second) {
				balance_map[pending.first] -= pending.second;
			}
		}
	}

	void update_from(std::shared_ptr<const Transaction> tx)
	{
		while(tx) {
			for(const auto& in : tx->inputs) {
				if(find_address(in.address) >= 0) {
					const auto key = std::make_pair(in.address, in.contract);
					balance_map[key] -= in.amount;
					pending_map[tx->id][key] += in.amount;
				}
			}
			tx = tx->parent;
		}
	}

	void gather_inputs(	std::shared_ptr<Transaction> tx,
						std::map<std::pair<addr_t, addr_t>, uint128_t>& spent_map,
						const uint128_t& amount, const addr_t& currency,
						const spend_options_t& options = {}) const
	{
		if(amount.upper()) {
			throw std::logic_error("amount too large");
		}
		uint64_t left = amount;

		// try to reuse existing inputs if possible
		for(auto& in : tx->inputs)
		{
			if(left == 0) {
				return;
			}
			if(in.contract == currency && find_address(in.address) >= 0)
			{
				auto iter = balance_map.find(std::make_pair(in.address, in.contract));
				if(iter != balance_map.end())
				{
					auto balance = iter->second;
					{
						auto iter2 = spent_map.find(iter->first);
						if(iter2 != spent_map.end()) {
							balance -= iter2->second;
						}
					}
					uint64_t amount = 0;
					if(balance >= left) {
						amount = left;
					} else {
						amount = balance;
					}
					if((uint128_t(in.amount) + amount).upper()) {
						amount = uint64_t(-1) - in.amount;
					}
					if(amount) {
						in.amount += amount;
						spent_map[iter->first] += amount;
						left -= amount;
					}
				}
			}
		}

		// create new inputs
		for(const auto& entry : balance_map)
		{
			if(left == 0) {
				return;
			}
			if(entry.first.second == currency)
			{
				auto balance = entry.second;
				{
					auto iter = spent_map.find(entry.first);
					if(iter != spent_map.end()) {
						balance -= iter->second;
					}
				}
				if(!balance) {
					continue;
				}
				txin_t in;
				in.address = entry.first.first;
				in.contract = currency;
				if(left < balance) {
					in.amount = left;
				} else {
					in.amount = balance;
				}
				spent_map[entry.first] += in.amount;
				left -= in.amount;
				tx->inputs.push_back(in);
			}
		}
		if(left) {
			throw std::logic_error("not enough funds");
		}
	}

	void gather_fee(std::shared_ptr<Transaction> tx,
					std::map<std::pair<addr_t, addr_t>, uint128_t>& spent_map,
					const spend_options_t& options = {}) const
	{
		tx->fee_ratio = options.fee_ratio;

		uint64_t paid_fee = 0;
		while(true) {
			std::unordered_map<addr_t, uint64_t> spend_cost;
			for(const auto& in : tx->inputs) {
				if(in.solution == txin_t::NO_SOLUTION) {
					addr_t owner = in.address;
					auto iter = options.owner_map.find(owner);
					if(iter != options.owner_map.end()) {
						spend_cost[iter->second] += params->min_txfee_exec;
					} else {
						spend_cost.emplace(owner, 0);
					}
				}
			}
			// TODO: check for multi-sig via options.contract_map
			uint64_t tx_fees = tx->calc_cost(params)
					+ spend_cost.size() * params->min_txfee_sign
					+ tx->execute.size() * params->min_txfee_sign
					+ options.extra_fee;

			for(const auto& entry : spend_cost) {
				tx_fees += entry.second;	// spend execution cost
			}
			if(std::dynamic_pointer_cast<const contract::NFT>(tx->deploy)) {
				tx_fees += params->min_txfee_sign;
			}
			tx_fees = (uint128_t(tx_fees) * tx->fee_ratio) / 1024;

			if(paid_fee >= tx_fees) {
				break;
			}
			const auto more = tx_fees - paid_fee;
			gather_inputs(tx, spent_map, more, addr_t(), options);
			paid_fee += more;
		}
	}

	void sign_off(std::shared_ptr<Transaction> tx, const spend_options_t& options = {}) const
	{
		if(!tx->sender) {
			tx->sender = get_address(0);
		}
		tx->salt = genesis_hash;
		tx->finalize();

		std::unordered_map<addr_t, uint32_t> solution_map;

		// sign all inputs
		for(auto& in : tx->inputs)
		{
			if(in.solution != txin_t::NO_SOLUTION) {
				continue;
			}
			addr_t owner = in.address;
			{
				auto iter = options.owner_map.find(owner);
				if(iter != options.owner_map.end()) {
					in.flags |= txin_t::IS_EXEC;
					owner = iter->second;
				}
			}
			{
				auto iter = solution_map.find(owner);
				if(iter != solution_map.end()) {
					// re-use solution
					in.solution = iter->second;
					continue;
				}
			}
			if(auto sol = sign_msg(owner, tx->id, options))
			{
				in.solution = tx->solutions.size();
				solution_map[owner] = in.solution;
				tx->solutions.push_back(sol);
			}
		}

		// sign all operations
		for(auto& op : tx->execute)
		{
			if(op->solution) {
				continue;
			}
			addr_t owner = op->address;

			if(auto exec = std::dynamic_pointer_cast<const operation::Execute>(op)) {
				if(exec->user) {
					owner = *exec->user;
				} else {
					continue;
				}
			} else {
				auto iter = options.owner_map.find(op->address);
				if(iter != options.owner_map.end()) {
					owner = iter->second;
				}
			}
			if(auto sol = sign_msg(owner, tx->id, options)) {
				auto copy = vnx::clone(op);
				copy->solution = sol;
				op = copy;
			}
		}

		// sign NFT mint
		if(auto nft = std::dynamic_pointer_cast<const contract::NFT>(tx->deploy))
		{
			if(!nft->solution) {
				if(auto sol = sign_msg(nft->creator, tx->id, options)) {
					auto copy = vnx::clone(nft);
					copy->solution = sol;
					tx->deploy = copy;
				}
			}
		}
	}

	std::shared_ptr<const Solution> sign_msg(const addr_t& address, const hash_t& msg, const spend_options_t& options = {}) const
	{
		// TODO: check for multi-sig via options.contract_map
		if(auto keys = find_keypair(address)) {
			auto sol = solution::PubKey::create();
			sol->pubkey = keys->second;
			sol->signature = signature_t::sign(keys->first, msg);
			return sol;
		}
		return nullptr;
	}

	void complete(std::shared_ptr<Transaction> tx, const spend_options_t& options = {}) const
	{
		std::map<addr_t, uint128_t> missing;
		{
			std::shared_ptr<const Transaction> txi = tx;
			while(txi) {
				for(const auto& out : txi->outputs) {
					missing[out.contract] += out.amount;
				}
				for(const auto& op : txi->execute) {
					if(auto deposit = std::dynamic_pointer_cast<const operation::Deposit>(op)) {
						missing[deposit->currency] += deposit->amount;
					}
				}
				txi = txi->parent;
			}
		}
		{
			std::shared_ptr<const Transaction> txi = tx;
			while(txi) {
				for(const auto& in : txi->inputs) {
					auto& amount = missing[in.contract];
					if(in.amount < amount) {
						amount -= in.amount;
					} else {
						amount = 0;
					}
				}
				txi = txi->parent;
			}
		}
		std::map<std::pair<addr_t, addr_t>, uint128_t> spent_map;
		for(const auto& entry : missing) {
			if(const auto& amount = entry.second) {
				gather_inputs(tx, spent_map, amount, entry.first, options);
			}
		}
		gather_fee(tx, spent_map, options);
		sign_off(tx, options);
	}

	void reset_cache()
	{
		last_update = 0;
		balance_map.clear();
		reserved_map.clear();
		pending_map.clear();
	}

	uint32_t height = 0;
	int64_t last_update = 0;
	std::map<std::pair<addr_t, addr_t>, uint128_t> balance_map;
	std::map<std::pair<addr_t, addr_t>, uint128_t> reserved_map;
	std::map<hash_t, std::map<std::pair<addr_t, addr_t>, uint128_t>> pending_map;

private:
	skey_t master_sk;
	std::vector<addr_t> addresses;
	std::unordered_map<addr_t, size_t> index_map;
	std::vector<std::pair<skey_t, pubkey_t>> keypairs;
	std::unordered_map<addr_t, std::pair<skey_t, pubkey_t>> keypair_map;

	const std::shared_ptr<const ChainParams> params;
	const hash_t genesis_hash;

};


} // mmx

#endif /* INCLUDE_MMX_ECDSA_WALLET_H_ */
