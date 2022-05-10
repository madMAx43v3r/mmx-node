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

	ECDSA_Wallet(std::shared_ptr<const KeyFile> key_file, const account_t& config, std::shared_ptr<const ChainParams> params)
		:	config(config), params(params)
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
				balance_map[std::make_pair(in.address, in.contract)] -= in.amount;
				pending_map[tx->id][std::make_pair(in.address, in.contract)] += in.amount;
			}
			tx = tx->parent;
		}
	}

	void gather_inputs(	std::shared_ptr<Transaction> tx,
						std::map<std::pair<addr_t, addr_t>, uint128_t>& spent_map,
						const uint128_t& amount, const addr_t& currency,
						const spend_options_t& options = {})
	{
		if(amount.upper()) {
			throw std::logic_error("amount too large");
		}
		if(options.budget_map) {
			throw std::logic_error("not yet supported");
		}
		uint64_t left = amount;

		for(const auto& entry : balance_map)
		{
			if(left == 0) {
				break;
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
				left -= in.amount;
				tx->inputs.push_back(in);
				spent_map[entry.first] += in.amount;
			}
		}
		if(left) {
			throw std::logic_error("not enough funds");
		}
	}

	void gather_fee(std::shared_ptr<Transaction> tx,
					std::map<std::pair<addr_t, addr_t>, uint128_t>& spent_map,
					const spend_options_t& options = {})
	{
		uint64_t paid_fee = 0;
		while(true) {
			std::unordered_map<addr_t, uint64_t> spend_cost;
			{
				std::shared_ptr<const Transaction> parent = tx;
				while(parent) {
					for(const auto& in : parent->inputs)
					{
						addr_t owner = in.address;
						auto iter = options.owner_map.find(owner);
						if(iter != options.owner_map.end()) {
							spend_cost[iter->second] += params->min_txfee_exec;
						} else {
							spend_cost.emplace(owner, 0);
						}
					}
					parent = parent->parent;
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
			const auto more = (tx_fees + params->min_txfee_io) - paid_fee;
			gather_inputs(tx, spent_map, more, addr_t(), options);
			paid_fee += more;
		}
		if(!tx->sender) {
			tx->sender = get_address(0);
		}
	}

	void sign_off(std::shared_ptr<Transaction> tx, const spend_options_t& options = {})
	{
		if(options.budget_map) {
			throw std::logic_error("not yet supported");
		}
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
			const auto sol = sign_msg(owner, tx->id, options);

			in.solution = tx->solutions.size();
			solution_map[owner] = in.solution;
			tx->solutions.push_back(sol);
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
			auto copy = vnx::clone(op);
			copy->solution = sign_msg(owner, tx->id, options);
			op = copy;
		}

		// sign NFT mint
		if(auto nft = std::dynamic_pointer_cast<const contract::NFT>(tx->deploy))
		{
			if(!nft->solution) {
				auto copy = vnx::clone(nft);
				copy->solution = sign_msg(nft->creator, tx->id, options);
				tx->deploy = copy;
			}
		}
	}

	std::shared_ptr<const Solution> sign_msg(const addr_t& address, const hash_t& msg, const spend_options_t& options = {}) const
	{
		// TODO: check for multi-sig via options.contract_map
		const auto& keys = get_keypair(address);

		auto sol = solution::PubKey::create();
		sol->pubkey = keys.second;
		sol->signature = signature_t::sign(keys.first, msg);
		return sol;
	}

	void complete(std::shared_ptr<Transaction> tx, const spend_options_t& options = {})
	{
		std::map<addr_t, uint128_t> missing;
		std::shared_ptr<const Transaction> parent = tx;
		while(parent) {
			for(const auto& out : parent->outputs) {
				missing[out.contract] += out.amount;
			}
			parent = parent->parent;
		}
		parent = tx;
		while(parent) {
			for(const auto& in : parent->inputs) {
				auto& amount = missing[in.contract];
				if(in.amount < amount) {
					amount -= in.amount;
				} else {
					amount = 0;
				}
			}
			parent = parent->parent;
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

	std::shared_ptr<Transaction> send(	const uint64_t& amount, const addr_t& dst_addr,
										const addr_t& currency, const spend_options_t& options)
	{
		if(amount == 0) {
			throw std::logic_error("amount cannot be zero");
		}
		if(dst_addr == addr_t()) {
			throw std::logic_error("dst_addr cannot be zero");
		}
		auto tx = Transaction::create();
		tx->note = tx_note_e::TRANSFER;
		tx->add_output(currency, dst_addr, amount, options.sender);
		complete(tx, options);
		return tx;
	}

	std::shared_ptr<Transaction> send_from(	const uint64_t& amount, const addr_t& dst_addr,
											const addr_t& src_addr, const addr_t& src_owner,
											const addr_t& currency, const spend_options_t& options)
	{
		if(amount == 0) {
			throw std::logic_error("amount cannot be zero");
		}
		if(dst_addr == addr_t()) {
			throw std::logic_error("dst_addr cannot be zero");
		}
		auto tx = Transaction::create();
		tx->note = tx_note_e::WITHDRAW;
		tx->add_input(currency, src_addr, amount);
		tx->add_output(currency, dst_addr, amount, options.sender);

		auto options_ = options;
		options_.owner_map.emplace(src_addr, src_owner);

		complete(tx, options_);
		return tx;
	}

	std::shared_ptr<Transaction> mint(	const uint64_t& amount, const addr_t& dst_addr, const addr_t& currency,
										const addr_t& owner, const spend_options_t& options)
	{
		if(amount == 0) {
			throw std::logic_error("amount cannot be zero");
		}
		if(dst_addr == addr_t()) {
			throw std::logic_error("dst_addr cannot be zero");
		}
		auto tx = Transaction::create();
		tx->note = tx_note_e::MINT;

		auto op = operation::Mint::create();
		op->amount = amount;
		op->target = dst_addr;
		op->address = currency;
		if(!op->is_valid()) {
			throw std::logic_error("invalid operation");
		}
		tx->execute.push_back(op);

		auto options_ = options;
		options_.owner_map.emplace(currency, owner);

		complete(tx, options_);
		return tx;
	}

	std::shared_ptr<Transaction> deploy(std::shared_ptr<const Contract> contract, const spend_options_t& options)
	{
		if(!contract || !contract->is_valid()) {
			throw std::logic_error("invalid contract");
		}
		auto tx = Transaction::create();
		tx->note = tx_note_e::DEPLOY;
		tx->deploy = contract;

		complete(tx, options);
		return tx;
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

	std::shared_ptr<const ChainParams> params;

};


} // mmx

#endif /* INCLUDE_MMX_ECDSA_WALLET_H_ */
