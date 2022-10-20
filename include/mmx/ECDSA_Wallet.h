/*
 * ECDSA_Wallet.h
 *
 *  Created on: Dec 11, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_ECDSA_WALLET_H_
#define INCLUDE_MMX_ECDSA_WALLET_H_

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
#include <mmx/account_t.hxx>
#include <mmx/hmac_sha512.hpp>
#include <mmx/utils.h>


namespace mmx {

class ECDSA_Wallet {
public:
	const account_t config;

	ECDSA_Wallet(	const hash_t& seed_value,
					const account_t& config, std::shared_ptr<const ChainParams> params)
		:	config(config), seed_value(seed_value), params(params)
	{
		if(seed_value == hash_t()) {
			throw std::logic_error("seed == zero");
		}
	}

	ECDSA_Wallet(	const hash_t& seed_value, const std::vector<addr_t>& addresses,
					const account_t& config, std::shared_ptr<const ChainParams> params)
		:	config(config), seed_value(seed_value), params(params)
	{
		if(seed_value == hash_t()) {
			throw std::logic_error("seed == zero");
		}
		size_t i = 0;
		for(const auto& addr : addresses) {
			index_map[addr] = i++;
		}
		this->addresses = addresses;
	}

	void lock()
	{
		// clear memory
		for(auto& entry : keypairs) {
			entry.first = skey_t();
			entry.second = pubkey_t();
		}
		keypairs.clear();
		keypair_map.clear();
	}

	void unlock() {
		unlock(std::string());
	}

	void unlock(const std::string& passphrase)
	{
		if(config.with_passphrase) {
			const auto finger_print = get_finger_print(seed_value, passphrase);
			if(finger_print != config.finger_print) {
				throw std::logic_error("invalid passphrase");
			}
		}
		unlock(hash_t("MMX/seed/" + passphrase));
	}

	void unlock(const hash_t& passphrase)
	{
		vnx::optional<addr_t> first_addr;
		if(addresses.size()) {
			first_addr = addresses[0];
		}
		const auto master = pbkdf2_hmac_sha512(seed_value, passphrase, PBKDF2_ITERS);
		const auto chain = hmac_sha512_n(master.first, master.second, 11337);	// TODO(mainnet): use params.port
		const auto account = hmac_sha512_n(chain.first, chain.second, config.index);

		for(size_t i = 0; i < config.num_addresses; ++i)
		{
			std::pair<skey_t, pubkey_t> keys;
			{
				const auto tmp = hmac_sha512_n(account.first, account.second, i);
				keys.first = tmp.first;
				keys.second = pubkey_t::from_skey(tmp.first);
			}
			const auto addr = keys.second.get_addr();
			if(i == 0) {
				if(first_addr && addr != *first_addr) {
					throw std::runtime_error("invalid passphrase");
				}
				keypairs.resize(config.num_addresses);
				addresses.resize(config.num_addresses);
			}
			keypairs[i] = keys;
			keypair_map[addr] = i;
			addresses[i] = addr;
			index_map[addr] = i;
		}
	}

	bool is_locked() const
	{
		return addresses.empty() || keypairs.size() < addresses.size();
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
		if(index >= addresses.size()) {
			throw std::logic_error("address index out of range: " + std::to_string(index) + " >= " + std::to_string(addresses.size()));
		}
		return addresses[index];
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
			return keypairs[iter->second];
		}
		return nullptr;
	}

	void update_cache(	const std::map<std::pair<addr_t, addr_t>, uint128>& balances,
						const std::vector<hash_t>& history, const uint32_t height)
	{
		this->height = height;
		balance_map.clear();
		balance_map.insert(balances.begin(), balances.end());

		std::vector<hash_t> expired;
		for(const auto& entry : pending_tx) {
			if(entry.second < height) {
				expired.push_back(entry.first);
			}
		}
		for(const auto& txid : expired) {
			pending_tx.erase(txid);
			pending_map.erase(txid);
		}
		for(const auto& txid : history) {
			pending_tx.erase(txid);
			pending_map.erase(txid);
		}
		for(const auto& entry : reserved_map) {
			clamped_sub_assign(balance_map[entry.first], entry.second);
		}
		for(const auto& entry : pending_map) {
			for(const auto& pending : entry.second) {
				clamped_sub_assign(balance_map[pending.first], pending.second);
			}
		}
	}

	void update_from(std::shared_ptr<const Transaction> tx)
	{
		if(tx->sender) {
			const auto key = std::make_pair(*tx->sender, addr_t());
			const auto static_fee = uint64_t(tx->static_cost) * tx->fee_ratio / 1024;
			clamped_sub_assign(balance_map[key], static_fee);
			pending_map[tx->id][key] += static_fee;
		}
		for(const auto& in : tx->inputs) {
			if(find_address(in.address) >= 0) {
				const auto key = std::make_pair(in.address, in.contract);
				clamped_sub_assign(balance_map[key], in.amount);
				pending_map[tx->id][key] += in.amount;
			}
		}
		pending_tx[tx->id] = tx->expires;
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

	void sign_off(std::shared_ptr<Transaction> tx, const spend_options_t& options = {})
	{
		if(is_locked()) {
			throw std::logic_error("wallet is locked");
		}
		bool was_locked = false;
		if(is_locked()) {
			was_locked = true;
			if(auto passphrase = options.passphrase) {
				unlock(*passphrase);
			} else {
				throw std::logic_error("wallet is locked");
			}
		}
		try {
			tx->finalize();

			std::unordered_map<addr_t, uint32_t> solution_map;

			// sign sender
			if(tx->sender && tx->solutions.empty())
			{
				if(auto sol = sign_msg(*tx->sender, tx->id, options))
				{
					solution_map[*tx->sender] = tx->solutions.size();
					tx->solutions.push_back(sol);
				}
			}

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

			// compute final content hash
			tx->static_cost = tx->calc_cost(params);
			tx->content_hash = tx->calc_hash(true);
		}
		catch(...) {
			if(was_locked) {
				lock();
			}
			throw;
		}
		if(was_locked) {
			lock();
		}
	}

	std::shared_ptr<const Solution> sign_msg(const addr_t& address, const hash_t& msg, const spend_options_t& options = {}) const
	{
		if(is_locked()) {
			throw std::logic_error("wallet is locked");
		}
		// TODO: check for multi-sig via options.contract_map
		if(auto keys = find_keypair(address)) {
			auto sol = solution::PubKey::create();
			sol->pubkey = keys->second;
			sol->signature = signature_t::sign(keys->first, msg);
			return sol;
		}
		return nullptr;
	}

	void complete(std::shared_ptr<Transaction> tx, const spend_options_t& options = {})
	{
		if(is_locked()) {
			throw std::logic_error("wallet is locked");
		}
		if(options.note) {
			tx->note = *options.note;
		}
		if(options.expire_at) {
			tx->expires = std::min(tx->expires, *options.expire_at);
		} else if(options.expire_delta) {
			tx->expires = std::min(tx->expires, height + *options.expire_delta);
		}
		tx->fee_ratio = std::max(tx->fee_ratio, options.fee_ratio);

		std::map<addr_t, uint128_t> missing;
		for(const auto& out : tx->outputs) {
			missing[out.contract] += out.amount;
		}
		for(const auto& op : tx->execute) {
			if(auto deposit = std::dynamic_pointer_cast<const operation::Deposit>(op)) {
				missing[deposit->currency] += deposit->amount;
			}
		}
		for(const auto& in : tx->inputs) {
			auto& amount = missing[in.contract];
			if(in.amount < amount) {
				amount -= in.amount;
			} else {
				amount = 0;
			}
		}
		std::map<std::pair<addr_t, addr_t>, uint128_t> spent_map;
		for(const auto& entry : missing) {
			if(const auto& amount = entry.second) {
				gather_inputs(tx, spent_map, amount, entry.first, options);
			}
		}
		const auto static_cost = tx->calc_cost(params);
		const auto static_fee = (static_cost * tx->fee_ratio) / 1024;
		tx->max_fee_amount = ((static_cost + options.max_extra_cost) * tx->fee_ratio) / 1024;

		if(!tx->sender) {
			if(options.sender) {
				tx->sender = *options.sender;
			} else {
				addr_t max_address;
				uint128_t max_amount = 0;
				for(const auto& entry : balance_map) {
					if(entry.first.second == addr_t()) {
						auto balance = entry.second;
						auto iter = spent_map.find(entry.first);
						if(iter != spent_map.end()) {
							balance -= iter->second;
						}
						if(balance > max_amount) {
							max_amount = balance;
							max_address = entry.first.first;
						}
					}
				}
				if(max_amount >= static_fee) {
					tx->sender = max_address;
				} else {
					throw std::logic_error("insufficient funds for tx fee");
				}
			}
		}
		sign_off(tx, options);
	}

	void reset_cache()
	{
		last_update = 0;
		balance_map.clear();
		reserved_map.clear();
		external_balance_map.clear();
		pending_tx.clear();
		pending_map.clear();
	}

	uint32_t height = 0;
	int64_t last_update = 0;
	std::map<std::pair<addr_t, addr_t>, uint128_t> balance_map;									// [[address, currency] => balance]
	std::map<std::pair<addr_t, addr_t>, uint128_t> reserved_map;								// [[address, currency] => balance]
	std::map<addr_t, uint128_t> external_balance_map;											// [currency => balance]
	std::unordered_map<hash_t, uint32_t> pending_tx;											// [txid => expired height]
	std::unordered_map<hash_t, std::map<std::pair<addr_t, addr_t>, uint128_t>> pending_map;		// [txid => [[address, currency] => balance]]

private:
	const hash_t seed_value;
	std::vector<addr_t> addresses;
	std::unordered_map<addr_t, size_t> index_map;
	std::vector<std::pair<skey_t, pubkey_t>> keypairs;
	std::unordered_map<addr_t, size_t> keypair_map;

	const std::shared_ptr<const ChainParams> params;

	static constexpr size_t PBKDF2_ITERS = 4096;

};


} // mmx

#endif /* INCLUDE_MMX_ECDSA_WALLET_H_ */
