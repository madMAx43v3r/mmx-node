/*
 * Wallet.cpp
 *
 *  Created on: Dec 11, 2021
 *      Author: mad
 */

#include <mmx/Wallet.h>
#include <mmx/contract/Token.hxx>
#include <mmx/operation/Mutate.hxx>
#include <mmx/operation/Execute.hxx>
#include <mmx/operation/Deposit.hxx>
#include <mmx/operation/Revoke.hxx>
#include <mmx/solution/PubKey.hxx>
#include <mmx/utils.h>

#include <vnx/vnx.h>
#include <algorithm>
#include <filesystem>


namespace mmx {

Wallet::Wallet(const std::string& _vnx_name)
	:	WalletBase(_vnx_name)
{
}

void Wallet::init()
{
	vnx::open_pipe(vnx_name, this, 1000);
}

void Wallet::main()
{
	if(key_files.empty()) {
		if(vnx::File(storage_path + "wallet.dat").exists()) {
			key_files = {"wallet.dat"};
		}
	}
	if(key_files.size() > max_key_files) {
		throw std::logic_error("too many key files");
	}
	params = get_params();
	token_whitelist.insert(addr_t());

	vnx::Directory(config_path).create();
	vnx::Directory(storage_path).create();
	vnx::Directory(database_path).create();
	{
		::rocksdb::Options options;
		options.max_open_files = 4;
		options.keep_log_file_num = 3;
		options.target_file_size_multiplier = 4;
		options.avoid_flush_during_recovery = true;
		options.max_manifest_file_size = 8 * 1024 * 1024;

		tx_log.open(database_path + "tx_log", options);
	}

	node = std::make_shared<NodeClient>(node_server);
	http = std::make_shared<vnx::addons::HttpInterface<Wallet>>(this, vnx_name);
	add_async_client(http);

	genesis_hash = node->get_genesis_hash();

	for(size_t i = 0; i < key_files.size(); ++i) {
		account_t config;
		config.name = "Default";
		config.key_file = key_files[i];
		config.num_addresses = num_addresses;
		try {
			add_account(i, config);
		} catch(const std::exception& ex) {
			log(WARN) << ex.what();
		}
	}
	for(size_t i = 0; i < accounts.size(); ++i) {
		try {
			add_account(max_key_files + i, accounts[i]);
		} catch(const std::exception& ex) {
			log(WARN) << ex.what();
		}
	}

	Super::main();
}

std::shared_ptr<ECDSA_Wallet> Wallet::get_wallet(const uint32_t& index) const
{
	if(index >= wallets.size()) {
		throw std::logic_error("invalid wallet index: " + std::to_string(index));
	}
	if(auto wallet = wallets[index]) {
		return wallet;
	}
	throw std::logic_error("no such wallet");
}

std::shared_ptr<const Transaction>
Wallet::send(	const uint32_t& index, const uint64_t& amount, const addr_t& dst_addr,
				const addr_t& currency, const spend_options_t& options) const
{
	const auto wallet = get_wallet(index);
	update_cache(index);

	if(amount == 0) {
		throw std::logic_error("amount cannot be zero");
	}
	if(dst_addr == addr_t()) {
		throw std::logic_error("dst_addr cannot be zero");
	}
	auto tx = Transaction::create();
	tx->note = tx_note_e::TRANSFER;
	tx->add_output(currency, dst_addr, amount, options.sender);

	auto options_ = options;
	if(!options_.expire_delta) {
		options_.expire_delta = default_expire;
	}
	wallet->complete(tx, options_);

	if(tx->is_signed() && options_.auto_send) {
		send_off(index, tx);
		log(INFO) << "Sent " << amount << " with fee " << tx->calc_cost(params) << " to " << dst_addr << " (" << tx->id << ")";
	}
	return tx;
}

std::shared_ptr<const Transaction>
Wallet::send_many(	const uint32_t& index, const std::map<addr_t, uint64_t>& amounts,
					const addr_t& currency, const spend_options_t& options) const
{
	const auto wallet = get_wallet(index);
	update_cache(index);

	auto tx = Transaction::create();
	tx->note = tx_note_e::TRANSFER;

	for(const auto& entry : amounts) {
		if(entry.first == addr_t()) {
			throw std::logic_error("address cannot be zero");
		}
		if(entry.second == 0) {
			throw std::logic_error("amount cannot be zero");
		}
		tx->add_output(currency, entry.first, entry.second, options.sender);
	}
	auto options_ = options;
	if(!options_.expire_delta) {
		options_.expire_delta = default_expire;
	}
	wallet->complete(tx, options_);

	if(tx->is_signed() && options_.auto_send) {
		send_off(index, tx);
		log(INFO) << "Sent many with fee " << tx->calc_cost(params) << " (" << tx->id << ")";
	}
	return tx;
}

std::shared_ptr<const Transaction>
Wallet::send_from(	const uint32_t& index, const uint64_t& amount,
					const addr_t& dst_addr, const addr_t& src_addr,
					const addr_t& currency, const spend_options_t& options) const
{
	const auto wallet = get_wallet(index);
	update_cache(index);

	auto options_ = options;
	if(!options_.expire_delta) {
		options_.expire_delta = default_expire;
	}
	if(auto contract = node->get_contract(src_addr)) {
		if(auto owner = contract->get_owner()) {
			options_.owner_map.emplace(src_addr, *owner);
		} else {
			throw std::logic_error("contract has no owner");
		}
	}
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

	wallet->complete(tx, options_);

	if(tx->is_signed() && options_.auto_send) {
		send_off(index, tx);
		log(INFO) << "Sent " << amount << " with fee " << tx->calc_cost(params) << " to " << dst_addr << " (" << tx->id << ")";
	}
	return tx;
}

std::shared_ptr<const Transaction>
Wallet::mint(	const uint32_t& index, const uint64_t& amount, const addr_t& dst_addr,
				const addr_t& currency, const spend_options_t& options) const
{
	const auto token = std::dynamic_pointer_cast<const contract::Token>(node->get_contract(currency));
	if(!token) {
		throw std::logic_error("no such currency");
	}
	if(!token->owner) {
		throw std::logic_error("token has no owner");
	}
	const auto owner = *token->owner;

	const auto wallet = get_wallet(index);
	update_cache(index);

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
	if(!options_.expire_delta) {
		options_.expire_delta = default_expire;
	}
	options_.owner_map.emplace(currency, owner);

	wallet->complete(tx, options_);

	if(tx->is_signed() && options_.auto_send) {
		send_off(index, tx);
		log(INFO) << "Minted " << amount << " with fee " << tx->calc_cost(params) << " to " << dst_addr << " (" << tx->id << ")";
	}
	return tx;
}

std::shared_ptr<const Transaction>
Wallet::deploy(const uint32_t& index, std::shared_ptr<const Contract> contract, const spend_options_t& options) const
{
	if(!contract) {
		throw std::logic_error("contract cannot be null");
	}
	const auto wallet = get_wallet(index);
	update_cache(index);

	if(!contract || !contract->is_valid()) {
		throw std::logic_error("invalid contract");
	}
	auto tx = Transaction::create();
	tx->note = tx_note_e::DEPLOY;
	tx->deploy = contract;

	auto options_ = options;
	if(!options_.expire_delta) {
		options_.expire_delta = default_expire;
	}
	wallet->complete(tx, options_);

	if(tx->is_signed() && options_.auto_send) {
		send_off(index, tx);
		log(INFO) << "Deployed " << contract->get_type_name() << " with fee " << tx->calc_cost(params) << " as " << addr_t(tx->id) << " (" << tx->id << ")";
	}
	return tx;
}

std::shared_ptr<const Transaction>
Wallet::mutate(const uint32_t& index, const addr_t& address, const vnx::Object& method, const spend_options_t& options) const
{
	auto contract = node->get_contract(address);
	if(!contract) {
		throw std::logic_error("no such contract");
	}
	auto owner = contract->get_owner();
	if(!owner) {
		throw std::logic_error("contract has no owner");
	}
	const auto wallet = get_wallet(index);
	update_cache(index);

	auto op = operation::Mutate::create();
	op->address = address;
	op->method = method;

	auto tx = Transaction::create();
	tx->note = tx_note_e::MUTATE;
	tx->execute.push_back(op);

	if(wallet->find_address(*owner) < 0) {
		throw std::logic_error("contract not owned by wallet");
	}
	auto options_ = options;
	if(!options_.expire_delta) {
		options_.expire_delta = default_expire;
	}
	options_.owner_map.emplace(address, *owner);

	wallet->complete(tx, options_);

	if(tx->is_signed() && options_.auto_send) {
		send_off(index, tx);
		log(INFO) << "Mutated [" << address << "] via " << method["__type"] << " with fee " << tx->calc_cost(params) << " (" << tx->id << ")";
	}
	return tx;
}

std::shared_ptr<const Transaction> Wallet::execute(
			const uint32_t& index, const addr_t& address, const std::string& method,
			const std::vector<vnx::Variant>& args, const spend_options_t& options) const
{
	const auto wallet = get_wallet(index);
	update_cache(index);

	auto op = operation::Execute::create();
	op->address = address;
	op->method = method;
	op->args = args;
	op->user = options.user;

	if(!op->user) {
		op->user = wallet->get_address(0);
	}
	auto tx = Transaction::create();
	tx->note = tx_note_e::EXECUTE;
	tx->execute.push_back(op);

	auto options_ = options;
	if(!options_.expire_delta) {
		options_.expire_delta = default_expire;
	}
	wallet->complete(tx, options_);

	if(tx->is_signed() && options_.auto_send) {
		send_off(index, tx);
		log(INFO) << "Executed " << method << "() on [" << address << "] with fee " << tx->calc_cost(params) << " (" << tx->id << ")";
	}
	return tx;
}

std::shared_ptr<const Transaction> Wallet::deposit(
			const uint32_t& index, const addr_t& address, const std::string& method, const std::vector<vnx::Variant>& args,
			const uint64_t& amount, const addr_t& currency, const spend_options_t& options) const
{
	const auto wallet = get_wallet(index);
	update_cache(index);

	auto op = operation::Deposit::create();
	op->address = address;
	op->method = method;
	op->args = args;
	op->amount = amount;
	op->currency = currency;
	op->user = options.user;

	if(!op->user) {
		op->user = wallet->get_address(0);
	}
	auto tx = Transaction::create();
	tx->note = tx_note_e::DEPOSIT;
	tx->execute.push_back(op);

	auto options_ = options;
	if(!options_.expire_delta) {
		options_.expire_delta = default_expire;
	}
	wallet->complete(tx, options_);

	if(tx->is_signed() && options_.auto_send) {
		send_off(index, tx);
		log(INFO) << "Executed " << method << "() on [" << address << "] with fee " << tx->calc_cost(params) << " (" << tx->id << ")";
	}
	return tx;
}

std::shared_ptr<const Transaction> Wallet::make_offer(
			const uint32_t& index, const uint32_t& address, const uint64_t& bid_amount, const addr_t& bid_currency,
			const uint64_t& ask_amount, const addr_t& ask_currency, const spend_options_t& options) const
{
	if(bid_amount == 0 || ask_amount == 0) {
		throw std::logic_error("amount cannot be zero");
	}
	const auto wallet = get_wallet(index);
	update_cache(index);

	auto tx = Transaction::create();
	tx->note = tx_note_e::OFFER;
	tx->sender = wallet->get_address(0);
	tx->is_extendable = true;
	tx->add_output(ask_currency, wallet->get_address(address), ask_amount);

	std::map<std::pair<addr_t, addr_t>, uint128_t> spent_map;
	wallet->gather_inputs(tx, spent_map, bid_amount, bid_currency, options);
	wallet->sign_off(tx, options);
	return tx;
}

std::shared_ptr<const Transaction> Wallet::accept_offer(
			const uint32_t& index, std::shared_ptr<const Transaction> offer, const spend_options_t& options) const
{
	if(!offer) {
		throw std::logic_error("offer == null");
	}
	const auto wallet = get_wallet(index);
	update_cache(index);

	auto tx = Transaction::create();
	tx->note = tx_note_e::TRADE;
	tx->parent = offer;

	for(const auto& entry : tx->get_balance()) {
		const auto& input = entry.second.first;
		const auto& output = entry.second.second;
		if(input > output) {
			const auto amount = input - output;
			if(amount.upper()) {
				throw std::logic_error("amount overflow");
			}
			tx->add_output(entry.first, wallet->get_address(0), amount);
		}
	}
	auto options_ = options;
	if(!options_.expire_delta) {
		options_.expire_delta = default_expire;
	}
	wallet->complete(tx, options_);

	if(tx->is_signed() && options_.auto_send) {
		send_off(index, tx);
		log(INFO) << "Accepted offer with fee " << tx->calc_cost(params) << " (" << tx->id << ")";
	}
	return tx;
}

std::shared_ptr<const Transaction> Wallet::revoke(
		const uint32_t& index, const hash_t& txid, const addr_t& address, const spend_options_t& options) const
{
	const auto wallet = get_wallet(index);
	update_cache(index);

	auto tx = Transaction::create();
	tx->note = tx_note_e::REVOKE;

	auto op = operation::Revoke::create();
	op->address = address;
	op->txid = txid;
	tx->execute.push_back(op);

	auto options_ = options;
	if(!options_.expire_delta) {
		options_.expire_delta = default_expire;
	}
	wallet->complete(tx, options_);

	if(tx->is_signed() && options_.auto_send) {
		send_off(index, tx);
		log(INFO) << "Revoked " << txid << " with fee " << tx->calc_cost(params) << " (" << tx->id << ")";
	}
	return tx;
}

std::shared_ptr<const Transaction> Wallet::complete(
		const uint32_t& index, std::shared_ptr<const Transaction> tx, const spend_options_t& options) const
{
	if(!tx) {
		return nullptr;
	}
	const auto wallet = get_wallet(index);
	update_cache(index);

	auto options_ = options;
	if(!options_.expire_delta) {
		options_.expire_delta = default_expire;
	}
	auto copy = vnx::clone(tx);
	wallet->complete(copy, options_);
	return copy;
}

std::shared_ptr<const Transaction> Wallet::sign_off(
		const uint32_t& index, std::shared_ptr<const Transaction> tx, const vnx::bool_t& cover_fee, const spend_options_t& options) const
{
	if(!tx) {
		return nullptr;
	}
	const auto wallet = get_wallet(index);
	update_cache(index);

	auto copy = vnx::clone(tx);
	if(cover_fee) {
		std::map<std::pair<addr_t, addr_t>, uint128_t> spent_map;
		for(const auto& in : tx->get_all_inputs()) {
			if(wallet->find_address(in.address) >= 0) {
				spent_map[std::make_pair(in.address, in.contract)] += in.amount;
			}
		}
		wallet->gather_fee(copy, spent_map, options);
	}
	wallet->sign_off(copy, options);
	return copy;
}

std::shared_ptr<const Solution> Wallet::sign_msg(const uint32_t& index, const addr_t& address, const hash_t& msg) const
{
	const auto wallet = get_wallet(index);
	return wallet->sign_msg(address, msg);
}

void Wallet::send_off(const uint32_t& index, std::shared_ptr<const Transaction> tx) const
{
	if(!tx) {
		return;
	}
	const auto wallet = get_wallet(index);
	node->add_transaction(tx, true);
	wallet->update_from(tx);
	{
		tx_log_entry_t entry;
		entry.time = vnx::get_time_millis();
		entry.tx = tx;
		tx_log.insert(wallet->get_address(0), entry);
	}
}

void Wallet::mark_spent(const uint32_t& index, const std::map<std::pair<addr_t, addr_t>, uint128>& amounts)
{
	const auto wallet = get_wallet(index);
	// TODO
}

void Wallet::reserve(const uint32_t& index, const std::map<std::pair<addr_t, addr_t>, uint128>& amounts)
{
	const auto wallet = get_wallet(index);
	// TODO
}

void Wallet::release(const uint32_t& index, const std::map<std::pair<addr_t, addr_t>, uint128>& amounts)
{
	const auto wallet = get_wallet(index);
	// TODO
}

void Wallet::release_all()
{
	for(auto wallet : wallets) {
		if(wallet) {
			wallet->reserved_map.clear();
		}
	}
}

void Wallet::reset_cache(const uint32_t& index)
{
	const auto wallet = get_wallet(index);
	wallet->reset_cache();
	update_cache(index);
}

void Wallet::update_cache(const uint32_t& index) const
{
	const auto wallet = get_wallet(index);
	const auto now = vnx::get_wall_time_millis();

	if(now - wallet->last_update > cache_timeout_ms)
	{
		const auto height = node->get_height();
		const auto balances = node->get_all_balances(wallet->get_all_addresses());
		const auto history = wallet->pending_tx.empty() ? std::vector<hash_t>() :
				node->get_tx_ids_since(wallet->height - std::min(params->commit_delay, wallet->height));
		wallet->update_cache(balances, history, height);
		wallet->last_update = now;
	}
}

std::vector<txin_t> Wallet::gather_inputs_for(	const uint32_t& index, const uint64_t& amount,
												const addr_t& currency, const spend_options_t& options) const
{
	const auto wallet = get_wallet(index);
	update_cache(index);

	auto tx = Transaction::create();
	std::map<std::pair<addr_t, addr_t>, uint128_t> spent_map;
	wallet->gather_inputs(tx, spent_map, amount, currency, options);
	return tx->inputs;
}

std::vector<tx_entry_t> Wallet::get_history(const uint32_t& index, const int32_t& since) const
{
	const auto wallet = get_wallet(index);
	auto result = node->get_history(wallet->get_all_addresses(), since);
	for(auto& entry : result) {
		entry.is_validated = token_whitelist.count(entry.contract);
	}
	return result;
}

std::vector<tx_log_entry_t> Wallet::get_tx_history(const uint32_t& index, const int32_t& limit_, const uint32_t& offset) const
{
	const auto wallet = get_wallet(index);
	const size_t limit = limit_ >= 0 ? limit_ : uint32_t(-1);

	std::vector<tx_log_entry_t> res;
	tx_log.find_last(wallet->get_address(0), res, limit + offset);
	if(offset) {
		const auto tmp = std::move(res);
		res.clear();
		for(size_t i = 0; i < limit && i + offset < tmp.size(); ++i) {
			res.push_back(tmp[i + offset]);
		}
	}
	return res;
}

balance_t Wallet::get_balance(const uint32_t& index, const addr_t& currency, const uint32_t& min_confirm) const
{
	const auto balances = get_balances(index, min_confirm);

	auto iter = balances.find(currency);
	if(iter != balances.end()) {
		return iter->second;
	}
	return balance_t();
}

std::map<addr_t, balance_t> Wallet::get_balances(const uint32_t& index, const uint32_t& min_confirm) const
{
	const auto wallet = get_wallet(index);
	update_cache(index);

	std::map<addr_t, balance_t> amounts;
	for(const auto& entry : wallet->balance_map) {
		amounts[entry.first.second].spendable += entry.second;
	}
	for(const auto& entry : wallet->reserved_map) {
		amounts[entry.first.second].reserved += entry.second;
	}
	for(const auto& entry : wallet->pending_map) {
		for(const auto& entry2 : entry.second) {
			amounts[entry2.first.second].reserved += entry2.second;
		}
	}
	for(auto& entry : amounts) {
		entry.second.total = entry.second.spendable + entry.second.reserved + entry.second.locked;
		entry.second.is_validated = token_whitelist.count(entry.first);
	}
	return amounts;
}

std::map<addr_t, balance_t> Wallet::get_total_balances_for(const std::vector<addr_t>& addresses, const uint32_t& min_confirm) const
{
	std::map<addr_t, balance_t> amounts;
	for(const auto& entry : node->get_total_balances(addresses, min_confirm)) {
		auto& balance = amounts[entry.first];
		balance.total = entry.second;
		balance.spendable = balance.total;
		balance.is_validated = token_whitelist.count(entry.first);
	}
	return amounts;
}

std::map<addr_t, balance_t> Wallet::get_contract_balances(const addr_t& address, const uint32_t& min_confirm) const
{
	auto amounts = node->get_contract_balances(address, min_confirm);
	for(auto& entry : amounts) {
		entry.second.is_validated = token_whitelist.count(entry.first);
	}
	return amounts;
}

std::map<addr_t, std::shared_ptr<const Contract>> Wallet::get_contracts(const uint32_t& index) const
{
	const auto wallet = get_wallet(index);
	return node->get_contracts_by(wallet->get_all_addresses());
}

addr_t Wallet::get_address(const uint32_t& index, const uint32_t& offset) const
{
	const auto wallet = get_wallet(index);
	return wallet->get_address(offset);
}

std::vector<addr_t> Wallet::get_all_addresses(const int32_t& index) const
{
	if(index >= 0) {
		return get_wallet(index)->get_all_addresses();
	}
	std::vector<addr_t> list;
	for(const auto& wallet : wallets) {
		if(wallet) {
			const auto set = wallet->get_all_addresses();
			list.insert(list.end(), set.begin(), set.end());
		}
	}
	return list;
}

address_info_t Wallet::get_address_info(const uint32_t& index, const uint32_t& offset) const
{
	return node->get_address_info(get_address(index, offset));
}

std::vector<address_info_t> Wallet::get_all_address_infos(const int32_t& index) const
{
	const auto addresses = get_all_addresses(index);
	std::unordered_map<addr_t, size_t> index_map;
	std::vector<address_info_t> result(addresses.size());
	for(size_t i = 0; i < addresses.size(); ++i) {
		index_map[addresses[i]] = i;
		result[i].address = addresses[i];
	}
	for(const auto& entry : node->get_history(addresses)) {
		auto& info = result[index_map[entry.address]];
		switch(entry.type) {
			case tx_type_e::REWARD:
			case tx_type_e::RECEIVE:
				info.num_receive++;
				info.total_receive[entry.contract] += entry.amount;
				info.last_receive_height = std::max(info.last_receive_height, entry.height);
				break;
			case tx_type_e::SPEND:
				info.num_spend++;
				info.total_spend[entry.contract] += entry.amount;
				info.last_spend_height = std::max(info.last_spend_height, entry.height);
		}
	}
	return result;
}

std::shared_ptr<const FarmerKeys> Wallet::get_farmer_keys(const uint32_t& index) const
{
	if(auto wallet = bls_wallets.at(index)) {
		return wallet->get_farmer_keys();
	}
	return nullptr;
}

std::vector<std::shared_ptr<const FarmerKeys>> Wallet::get_all_farmer_keys() const
{
	std::vector<std::shared_ptr<const FarmerKeys>> res;
	for(const auto& wallet : bls_wallets) {
		if(wallet) {
			res.push_back(wallet->get_farmer_keys());
		}
	}
	return res;
}

account_t Wallet::get_account(const uint32_t& index) const
{
	const auto wallet = get_wallet(index);
	return wallet->config;
}

std::map<uint32_t, account_t> Wallet::get_all_accounts() const
{
	std::map<uint32_t, account_t> res;
	for(size_t i = 0; i < wallets.size(); ++i) {
		if(auto wallet = wallets[i]) {
			res[i] = wallet->config;
		}
	}
	return res;
}

void Wallet::add_account(const uint32_t& index, const account_t& config)
{
	if(index >= wallets.size()) {
		wallets.resize(index + 1);
		bls_wallets.resize(index + 1);
	} else if(wallets[index]) {
		throw std::logic_error("account already exists: " + std::to_string(index));
	}
	const auto key_path = storage_path + config.key_file;

	if(auto key_file = vnx::read_from_file<KeyFile>(key_path)) {
		if(enable_bls) {
			bls_wallets[index] = std::make_shared<BLS_Wallet>(key_file, 11337);
		}
		wallets[index] = std::make_shared<ECDSA_Wallet>(key_file, config, params, genesis_hash);
	} else {
		throw std::runtime_error("failed to read key file: " + key_path);
	}
	std::filesystem::permissions(key_path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
}

void Wallet::create_account(const account_t& config)
{
	if(config.name.empty()) {
		throw std::logic_error("name cannot be empty");
	}
	if(config.num_addresses < 1) {
		throw std::logic_error("num_addresses cannot be zero");
	}
	add_account(std::max<uint32_t>(max_key_files, wallets.size()), config);

	const std::string path = config_path + "Wallet.json";
	{
		auto object = vnx::read_config_file(path);
		auto& accounts = object["accounts+"];
		auto list = accounts.to<std::vector<account_t>>();
		list.push_back(config);
		accounts = list;
		vnx::write_config_file(path, object);
	}
}

void Wallet::create_wallet(const account_t& config_, const vnx::optional<hash_t>& seed)
{
	mmx::KeyFile key_file;
	if(seed) {
		if(*seed == hash_t()) {
			throw std::logic_error("seed cannot be all zeros");
		}
		key_file.seed_value = *seed;
	} else {
		key_file.seed_value = mmx::hash_t::random();
	}

	auto config = config_;
	if(config.name.empty()) {
		throw std::logic_error("name cannot be empty");
	}
	if(config.num_addresses < 1) {
		throw std::logic_error("num_addresses <= 0");
	}
	if(config.key_file.empty()) {
		config.key_file = "wallet_" + std::to_string(uint32_t(std::hash<hash_t>{}(key_file.seed_value))) + ".dat";
	}
	if(vnx::File(config.key_file).exists()) {
		throw std::logic_error("key file already exists");
	}
	const auto key_path = storage_path + config.key_file;
	vnx::write_to_file(key_path, key_file);
	std::filesystem::permissions(key_path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);

	create_account(config);
}

std::set<addr_t> Wallet::get_token_list() const
{
	return token_whitelist;
}

void Wallet::add_token(const addr_t& address)
{
	const std::string path = config_path + "Wallet.json";
	auto object = vnx::read_config_file(path);
	{
		auto& whitelist = object["token_whitelist+"];
		std::set<std::string> tmp;
		for(const auto& token : whitelist.to<std::set<addr_t>>()) {
			tmp.insert(token.to_string());
		}
		tmp.insert(address.to_string());
		whitelist = tmp;
	}
	vnx::write_config_file(path, object);
	token_whitelist.insert(address);
}

void Wallet::rem_token(const addr_t& address)
{
	const std::string path = config_path + "Wallet.json";
	auto object = vnx::read_config_file(path);
	{
		auto& whitelist = object["token_whitelist+"];
		std::set<std::string> tmp;
		for(const auto& token : whitelist.to<std::set<addr_t>>()) {
			tmp.insert(token.to_string());
		}
		if(!tmp.erase(address.to_string())) {
			throw std::logic_error("cannot remove token: " + address.to_string());
		}
		whitelist = tmp;
	}
	vnx::write_config_file(path, object);

	if(address != addr_t()) {
		token_whitelist.erase(address);
	}
}

hash_t Wallet::get_master_seed(const uint32_t& index) const
{
	const auto wallet = get_wallet(index);

	if(auto key_file = vnx::read_from_file<KeyFile>(storage_path + wallet->config.key_file)) {
		return key_file->seed_value;
	}
	throw std::logic_error("failed to read key file");
}

void Wallet::http_request_async(std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
								const vnx::request_id_t& request_id) const
{
	http->http_request(request, sub_path, request_id, vnx_request->session);
}

void Wallet::http_request_chunk_async(	std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
										const int64_t& offset, const int64_t& max_bytes, const vnx::request_id_t& request_id) const
{
	throw std::logic_error("not implemented");
}


} // mmx
