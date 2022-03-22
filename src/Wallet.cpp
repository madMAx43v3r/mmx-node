/*
 * Wallet.cpp
 *
 *  Created on: Dec 11, 2021
 *      Author: mad
 */

#include <mmx/Wallet.h>
#include <mmx/utxo_t.hpp>
#include <mmx/utxo_entry_t.hpp>
#include <mmx/stxo_entry_t.hpp>
#include <mmx/contract/Token.hxx>
#include <mmx/operation/Mutate.hxx>
#include <mmx/solution/PubKey.hxx>
#include <mmx/utils.h>

#include <vnx/vnx.h>
#include <algorithm>


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
	if(key_files.size() > max_key_files) {
		throw std::logic_error("too many key files");
	}
	params = get_params();

	vnx::Directory(config_path).create();
	vnx::Directory(storage_path).create();
	vnx::Directory(database_path).create();
	{
		::rocksdb::Options options;
		options.max_open_files = 4;
		options.keep_log_file_num = 3;
		options.max_manifest_file_size = 8 * 1024 * 1024;
		options.OptimizeForSmallDb();

		tx_log.open(database_path + "tx_log", options);
	}

	node = std::make_shared<NodeClient>(node_server);
	http = std::make_shared<vnx::addons::HttpInterface<Wallet>>(this, vnx_name);
	add_async_client(http);

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

hash_t Wallet::send(const uint32_t& index, const uint64_t& amount, const addr_t& dst_addr,
					const addr_t& currency, const spend_options_t& options) const
{
	const auto wallet = get_wallet(index);
	update_cache(index);

	auto tx = wallet->send(amount, dst_addr, currency, options);
	send_off(index, tx);

	log(INFO) << "Sent " << amount << " with fee " << tx->calc_cost(params) << " to " << dst_addr << " (" << tx->id << ")";
	return tx->id;
}

hash_t Wallet::send_from(	const uint32_t& index, const uint64_t& amount,
							const addr_t& dst_addr, const addr_t& src_addr,
							const addr_t& currency, const spend_options_t& options) const
{
	const auto wallet = get_wallet(index);
	update_cache(index);

	auto src_owner = src_addr;
	if(auto contract = node->get_contract(src_addr)) {
		if(auto owner = contract->get_owner()) {
			src_owner = *owner;
		} else {
			throw std::logic_error("contract has no owner");
		}
	}
	auto tx = wallet->send_from(amount, dst_addr, src_addr, src_owner,
								node->get_spendable_utxo_list({src_addr}, options.min_confirm),
								currency, options);
	send_off(index, tx);

	log(INFO) << "Sent " << amount << " with fee " << tx->calc_cost(params) << " to " << dst_addr << " (" << tx->id << ")";
	return tx->id;
}

hash_t Wallet::mint(const uint32_t& index, const uint64_t& amount, const addr_t& dst_addr,
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

	if(wallet->find_address(owner) < 0) {
		throw std::logic_error("token not owned by wallet");
	}
	auto tx = wallet->mint(amount, dst_addr, currency, owner, options);
	send_off(index, tx);

	log(INFO) << "Minted " << amount << " with fee " << tx->calc_cost(params) << " to " << dst_addr << " (" << tx->id << ")";
	return tx->id;
}

vnx::optional<hash_t> Wallet::split(const uint32_t& index, const uint64_t& max_amount, const addr_t& currency, const spend_options_t& options) const
{
	if(max_amount == 0) {
		throw std::logic_error("invalid max_amount");
	}
	const auto wallet = get_wallet(index);
	const std::unordered_set<txio_key_t> exclude(options.exclude.begin(), options.exclude.end());

	auto tx = Transaction::create();
	tx->note = tx_note_e::SPLIT;

	uint64_t total = 0;
	for(const auto& entry : get_utxo_list_for(index, currency, options.min_confirm)) {
		const auto& utxo = entry.output;
		if(utxo.amount > max_amount && wallet->is_spendable(entry.key) && !exclude.count(entry.key)) {
			tx_in_t in;
			in.prev = entry.key;
			tx->inputs.push_back(in);
			total += utxo.amount;
		}
	}
	if(!total) {
		return nullptr;
	}
	const auto dst_addr = wallet->get_address(0);
	const auto num_split = (total - (currency == addr_t() ? std::min<uint64_t>(1000000, total) : 0)) / max_amount;

	auto left = total;
	for(size_t i = 0; i < num_split; ++i) {
		tx_out_t out;
		out.contract = currency;
		out.address = dst_addr;
		out.amount = max_amount;
		tx->outputs.push_back(out);
		left -= out.amount;
	}
	if(left && currency != addr_t()) {
		tx_out_t out;
		out.contract = currency;
		out.address = dst_addr;
		out.amount = left;
		tx->outputs.push_back(out);
		left = 0;
	}
	auto signed_tx = sign_off(index, tx, true, {});
	send_off(index, signed_tx);
	return signed_tx->id;
}

hash_t Wallet::deploy(const uint32_t& index, std::shared_ptr<const Contract> contract, const spend_options_t& options) const
{
	if(!contract) {
		throw std::logic_error("contract cannot be null");
	}
	const auto wallet = get_wallet(index);
	update_cache(index);

	auto tx = wallet->deploy(contract, options);
	send_off(index, tx);

	log(INFO) << "Deployed " << contract->get_type_name() << " with fee " << tx->calc_cost(params) << " as " << addr_t(tx->id) << " (" << tx->id << ")";
	return tx->id;
}

hash_t Wallet::execute(const uint32_t& index, const addr_t& address, const vnx::Object& method, const spend_options_t& options) const
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
	options_.owner_map.emplace_back(address, *owner);

	wallet->complete(tx, wallet->utxo_cache, options_);
	send_off(index, tx);

	log(INFO) << "Executed " << method["__type"] << " on [" << address << "] with fee " << tx->calc_cost(params) << " (" << tx->id << ")";
	return tx->id;
}

std::shared_ptr<const Transaction>
Wallet::complete(const uint32_t& index, std::shared_ptr<const Transaction> tx, const spend_options_t& options) const
{
	if(!tx) {
		return nullptr;
	}
	const auto wallet = get_wallet(index);
	update_cache(index);

	auto copy = vnx::clone(tx);
	wallet->complete(copy, wallet->utxo_cache, options);
	return copy;
}

std::shared_ptr<const Transaction>
Wallet::sign_off(	const uint32_t& index, std::shared_ptr<const Transaction> tx,
					const vnx::bool_t& cover_fee, const spend_options_t& options) const
{
	if(!tx) {
		return nullptr;
	}
	const auto wallet = get_wallet(index);
	update_cache(index);

	const std::unordered_map<txio_key_t, utxo_t> utxo_map(options.utxo_map.begin(), options.utxo_map.end());

	int64_t native_change = 0;
	std::unordered_set<txio_key_t> spent_keys;
	for(const auto& in : tx->inputs) {
		auto iter = utxo_map.find(in.prev);
		if(iter != utxo_map.end()) {
			const auto& utxo = iter->second;
			if(utxo.contract == addr_t()) {
				native_change += utxo.amount;
			}
		}
		spent_keys.insert(in.prev);
	}
	std::unordered_map<txio_key_t, utxo_t> spent_map;
	for(const auto& entry : wallet->utxo_cache) {
		if(spent_keys.erase(entry.key)) {
			const auto& utxo = entry.output;
			if(utxo.contract == addr_t() && !utxo_map.count(entry.key)) {
				native_change += utxo.amount;
			}
			spent_map[entry.key] = utxo;
		}
		if(spent_keys.empty()) {
			break;
		}
	}
	for(const auto& out : tx->outputs) {
		if(out.contract == addr_t()) {
			native_change -= out.amount;
		}
	}

	auto copy = vnx::clone(tx);
	if(cover_fee) {
		for(const auto& key : spent_keys) {
			if(!utxo_map.count(key)) {
				throw std::logic_error("unknown input");
			}
		}
		if(native_change < 0) {
			throw std::logic_error("negative change");
		}
		wallet->gather_fee(copy, spent_map, options, native_change);
	}
	wallet->sign_off(copy, spent_map, options);
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
	node->add_transaction(tx);
	wallet->update_from(tx);
	{
		tx_log_entry_t entry;
		entry.time = vnx::get_time_millis();
		entry.tx = tx;
		tx_log.insert(wallet->get_address(0), entry);
	}
}

void Wallet::mark_spent(const uint32_t& index, const std::vector<txio_key_t>& keys)
{
	const auto wallet = get_wallet(index);
	for(const auto& key : keys) {
		wallet->reserved_set.erase(key);
	}
	wallet->spent_set.insert(keys.begin(), keys.end());
}

void Wallet::reserve(const uint32_t& index, const std::vector<txio_key_t>& keys)
{
	const auto wallet = get_wallet(index);
	wallet->reserved_set.insert(keys.begin(), keys.end());
}

void Wallet::release(const uint32_t& index, const std::vector<txio_key_t>& keys)
{
	const auto wallet = get_wallet(index);
	for(const auto& key : keys) {
		wallet->reserved_set.erase(key);
	}
}

void Wallet::release_all()
{
	for(auto wallet : wallets) {
		if(wallet) {
			wallet->reserved_set.clear();
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

	if(!wallet->last_utxo_update || now - wallet->last_utxo_update > utxo_timeout_ms)
	{
		const auto height = node->get_height();
		const auto all_utxo = node->get_utxo_list(wallet->get_all_addresses(), 1);
		wallet->update_cache(all_utxo, height);
		wallet->last_utxo_update = now;

		log(DEBUG) << "Updated cache: " << wallet->utxo_cache.size() << " utxo, " << wallet->utxo_change_cache.size() << " pending change";
	}
}

std::vector<utxo_entry_t> Wallet::get_utxo_list(const uint32_t& index, const uint32_t& min_confirm) const
{
	const auto wallet = get_wallet(index);
	update_cache(index);

	if(min_confirm == 0) {
		return wallet->utxo_cache;
	}
	std::vector<utxo_entry_t> list;
	const auto height = node->get_height();
	for(const auto& entry : wallet->utxo_cache) {
		const auto& utxo = entry.output;
		if(utxo.height <= height && (height - utxo.height) + 1 >= min_confirm) {
			list.push_back(entry);
		}
	}
	return list;
}

std::vector<utxo_entry_t> Wallet::get_utxo_list_for(const uint32_t& index, const addr_t& currency, const uint32_t& min_confirm) const
{
	std::vector<utxo_entry_t> res;
	for(const auto& entry : get_utxo_list(index, min_confirm)) {
		if(entry.output.contract == currency) {
			res.push_back(entry);
		}
	}
	return res;
}

std::vector<stxo_entry_t> Wallet::get_stxo_list(const uint32_t& index) const
{
	const auto wallet = get_wallet(index);
	return node->get_stxo_list(wallet->get_all_addresses());
}

std::vector<stxo_entry_t> Wallet::get_stxo_list_for(const uint32_t& index, const addr_t& currency) const
{
	std::vector<stxo_entry_t> res;
	for(const auto& entry : get_stxo_list(index)) {
		if(entry.output.contract == currency) {
			res.push_back(entry);
		}
	}
	return res;
}

std::vector<utxo_entry_t> Wallet::gather_utxos_for(	const uint32_t& index, const uint64_t& amount,
													const addr_t& currency, const spend_options_t& options) const
{
	const auto wallet = get_wallet(index);
	get_utxo_list(index);	// update utxo_cache

	auto tx = Transaction::create();

	std::vector<utxo_entry_t> res;
	std::unordered_map<txio_key_t, utxo_t> spent_map;
	wallet->gather_inputs(tx, wallet->utxo_cache, spent_map, amount, currency, options);

	for(const auto& in : tx->inputs) {
		utxo_entry_t entry;
		entry.key = in.prev;
		entry.output = spent_map[in.prev];
		res.push_back(entry);
	}
	return res;
}

std::vector<tx_entry_t> Wallet::get_history(const uint32_t& index, const int32_t& since) const
{
	const auto wallet = get_wallet(index);
	return node->get_history_for(wallet->get_all_addresses(), since);
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

	std::map<addr_t, balance_t> amounts;
	for(const auto& entry : get_utxo_list(index, min_confirm)) {
		const auto& utxo = entry.output;
		auto& out = amounts[utxo.contract];
		if(wallet->reserved_set.count(entry.key)) {
			out.reserved += utxo.amount;
		} else if(!wallet->spent_set.count(entry.key)) {
			out.spendable += utxo.amount;
		}
		out.total += utxo.amount;
	}
	return amounts;
}

std::map<addr_t, std::shared_ptr<const Contract>> Wallet::get_contracts(const uint32_t& index) const
{
	const auto wallet = get_wallet(index);
	return node->get_contracts_owned(wallet->get_all_addresses());
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
	if(index >= max_accounts) {
		throw std::logic_error("index >= max_accounts");
	}
	if(index >= wallets.size()) {
		wallets.resize(index + 1);
		bls_wallets.resize(index + 1);
	} else if(wallets[index]) {
		throw std::logic_error("account already exists");
	}
	if(auto key_file = vnx::read_from_file<KeyFile>(storage_path + config.key_file)) {
		if(enable_bls) {
			bls_wallets[index] = std::make_shared<BLS_Wallet>(key_file, 11337);
		}
		wallets[index] = std::make_shared<ECDSA_Wallet>(key_file, config, params);
	} else {
		throw std::runtime_error("failed to read key file");
	}
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
		key_file.seed_value = *seed;
	} else {
		key_file.seed_value = mmx::hash_t::random();
	}

	auto config = config_;
	if(config.name.empty()) {
		throw std::logic_error("name cannot be empty");
	}
	if(config.num_addresses < 1) {
		throw std::logic_error("num_addresses cannot be zero");
	}
	if(config.key_file.empty()) {
		config.key_file = "wallet_" + std::to_string(uint32_t(std::hash<hash_t>{}(key_file.seed_value))) + ".dat";
	}
	if(vnx::File(config.key_file).exists()) {
		throw std::logic_error("key file already exists");
	}
	vnx::write_to_file(storage_path + config.key_file, key_file);

	create_account(config);
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
	http->http_request(request, sub_path, request_id);
}

void Wallet::http_request_chunk_async(	std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
										const int64_t& offset, const int64_t& max_bytes, const vnx::request_id_t& request_id) const
{
	throw std::logic_error("not implemented");
}


} // mmx
