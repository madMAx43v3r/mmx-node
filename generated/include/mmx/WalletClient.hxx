
// AUTO GENERATED by vnxcppcodegen

#ifndef INCLUDE_mmx_Wallet_CLIENT_HXX_
#define INCLUDE_mmx_Wallet_CLIENT_HXX_

#include <vnx/Client.h>
#include <mmx/Contract.hxx>
#include <mmx/FarmerKeys.hxx>
#include <mmx/Solution.hxx>
#include <mmx/Transaction.hxx>
#include <mmx/account_t.hxx>
#include <mmx/addr_t.hpp>
#include <mmx/address_info_t.hxx>
#include <mmx/balance_t.hxx>
#include <mmx/hash_t.hpp>
#include <mmx/spend_options_t.hxx>
#include <mmx/tx_entry_t.hxx>
#include <mmx/tx_log_entry_t.hxx>
#include <mmx/txin_t.hxx>
#include <mmx/uint128.hpp>
#include <vnx/Module.h>
#include <vnx/Object.hpp>
#include <vnx/Variant.hpp>
#include <vnx/addons/HttpData.hxx>
#include <vnx/addons/HttpRequest.hxx>
#include <vnx/addons/HttpResponse.hxx>


namespace mmx {

class WalletClient : public vnx::Client {
public:
	WalletClient(const std::string& service_name);
	
	WalletClient(vnx::Hash64 service_addr);
	
	std::shared_ptr<const ::mmx::Transaction> send(const uint32_t& index = 0, const uint64_t& amount = 0, const ::mmx::addr_t& dst_addr = ::mmx::addr_t(), const ::mmx::addr_t& currency = ::mmx::addr_t(), const ::mmx::spend_options_t& options = ::mmx::spend_options_t());
	
	std::shared_ptr<const ::mmx::Transaction> send_many(const uint32_t& index = 0, const std::map<::mmx::addr_t, uint64_t>& amounts = {}, const ::mmx::addr_t& currency = ::mmx::addr_t(), const ::mmx::spend_options_t& options = ::mmx::spend_options_t());
	
	std::shared_ptr<const ::mmx::Transaction> send_from(const uint32_t& index = 0, const uint64_t& amount = 0, const ::mmx::addr_t& dst_addr = ::mmx::addr_t(), const ::mmx::addr_t& src_addr = ::mmx::addr_t(), const ::mmx::addr_t& currency = ::mmx::addr_t(), const ::mmx::spend_options_t& options = ::mmx::spend_options_t());
	
	std::shared_ptr<const ::mmx::Transaction> mint(const uint32_t& index = 0, const uint64_t& amount = 0, const ::mmx::addr_t& dst_addr = ::mmx::addr_t(), const ::mmx::addr_t& currency = ::mmx::addr_t(), const ::mmx::spend_options_t& options = ::mmx::spend_options_t());
	
	std::shared_ptr<const ::mmx::Transaction> deploy(const uint32_t& index = 0, std::shared_ptr<const ::mmx::Contract> contract = nullptr, const ::mmx::spend_options_t& options = ::mmx::spend_options_t());
	
	std::shared_ptr<const ::mmx::Transaction> mutate(const uint32_t& index = 0, const ::mmx::addr_t& address = ::mmx::addr_t(), const ::vnx::Object& method = ::vnx::Object(), const ::mmx::spend_options_t& options = ::mmx::spend_options_t());
	
	std::shared_ptr<const ::mmx::Transaction> execute(const uint32_t& index = 0, const ::mmx::addr_t& address = ::mmx::addr_t(), const std::string& method = "", const std::vector<::vnx::Variant>& args = {}, const ::mmx::spend_options_t& options = ::mmx::spend_options_t());
	
	std::shared_ptr<const ::mmx::Transaction> deposit(const uint32_t& index = 0, const ::mmx::addr_t& address = ::mmx::addr_t(), const std::string& method = "", const std::vector<::vnx::Variant>& args = {}, const uint64_t& amount = 0, const ::mmx::addr_t& currency = ::mmx::addr_t(), const ::mmx::spend_options_t& options = ::mmx::spend_options_t());
	
	std::shared_ptr<const ::mmx::Transaction> make_offer(const uint32_t& index = 0, const uint32_t& address = 0, const uint64_t& bid_amount = 0, const ::mmx::addr_t& bid_currency = ::mmx::addr_t(), const uint64_t& ask_amount = 0, const ::mmx::addr_t& ask_currency = ::mmx::addr_t(), const ::mmx::spend_options_t& options = ::mmx::spend_options_t());
	
	std::shared_ptr<const ::mmx::Transaction> accept_offer(const uint32_t& index = 0, std::shared_ptr<const ::mmx::Transaction> offer = nullptr, const ::mmx::spend_options_t& options = ::mmx::spend_options_t());
	
	std::shared_ptr<const ::mmx::Transaction> revoke(const uint32_t& index = 0, const ::mmx::hash_t& txid = ::mmx::hash_t(), const ::mmx::addr_t& address = ::mmx::addr_t(), const ::mmx::spend_options_t& options = ::mmx::spend_options_t());
	
	std::shared_ptr<const ::mmx::Transaction> complete(const uint32_t& index = 0, std::shared_ptr<const ::mmx::Transaction> tx = nullptr, const ::mmx::spend_options_t& options = ::mmx::spend_options_t());
	
	std::shared_ptr<const ::mmx::Transaction> sign_off(const uint32_t& index = 0, std::shared_ptr<const ::mmx::Transaction> tx = nullptr, const ::mmx::spend_options_t& options = ::mmx::spend_options_t());
	
	std::shared_ptr<const ::mmx::Solution> sign_msg(const uint32_t& index = 0, const ::mmx::addr_t& address = ::mmx::addr_t(), const ::mmx::hash_t& msg = ::mmx::hash_t());
	
	void send_off(const uint32_t& index = 0, std::shared_ptr<const ::mmx::Transaction> tx = nullptr);
	
	void send_off_async(const uint32_t& index = 0, std::shared_ptr<const ::mmx::Transaction> tx = nullptr);
	
	void mark_spent(const uint32_t& index = 0, const std::map<std::pair<::mmx::addr_t, ::mmx::addr_t>, ::mmx::uint128>& amounts = {});
	
	void mark_spent_async(const uint32_t& index = 0, const std::map<std::pair<::mmx::addr_t, ::mmx::addr_t>, ::mmx::uint128>& amounts = {});
	
	void reserve(const uint32_t& index = 0, const std::map<std::pair<::mmx::addr_t, ::mmx::addr_t>, ::mmx::uint128>& amounts = {});
	
	void reserve_async(const uint32_t& index = 0, const std::map<std::pair<::mmx::addr_t, ::mmx::addr_t>, ::mmx::uint128>& amounts = {});
	
	void release(const uint32_t& index = 0, const std::map<std::pair<::mmx::addr_t, ::mmx::addr_t>, ::mmx::uint128>& amounts = {});
	
	void release_async(const uint32_t& index = 0, const std::map<std::pair<::mmx::addr_t, ::mmx::addr_t>, ::mmx::uint128>& amounts = {});
	
	void release_all();
	
	void release_all_async();
	
	void reset_cache(const uint32_t& index = 0);
	
	void reset_cache_async(const uint32_t& index = 0);
	
	void update_cache(const uint32_t& index = 0);
	
	void update_cache_async(const uint32_t& index = 0);
	
	std::vector<::mmx::tx_entry_t> get_history(const uint32_t& index = 0, const int32_t& since = 0);
	
	std::vector<::mmx::tx_log_entry_t> get_tx_history(const uint32_t& index = 0, const int32_t& limit = -1, const uint32_t& offset = 0);
	
	std::vector<::mmx::txin_t> gather_inputs_for(const uint32_t& index = 0, const uint64_t& amount = 0, const ::mmx::addr_t& currency = ::mmx::addr_t(), const ::mmx::spend_options_t& options = ::mmx::spend_options_t());
	
	::mmx::balance_t get_balance(const uint32_t& index = 0, const ::mmx::addr_t& currency = ::mmx::addr_t(), const uint32_t& min_confirm = 0);
	
	std::map<::mmx::addr_t, ::mmx::balance_t> get_balances(const uint32_t& index = 0, const uint32_t& min_confirm = 0);
	
	std::map<::mmx::addr_t, ::mmx::balance_t> get_total_balances_for(const std::vector<::mmx::addr_t>& addresses = {}, const uint32_t& min_confirm = 1);
	
	std::map<::mmx::addr_t, ::mmx::balance_t> get_contract_balances(const ::mmx::addr_t& address = ::mmx::addr_t(), const uint32_t& min_confirm = 1);
	
	std::map<::mmx::addr_t, std::shared_ptr<const ::mmx::Contract>> get_contracts(const uint32_t& index = 0);
	
	::mmx::addr_t get_address(const uint32_t& index = 0, const uint32_t& offset = 0);
	
	std::vector<::mmx::addr_t> get_all_addresses(const int32_t& index = 0);
	
	::mmx::address_info_t get_address_info(const uint32_t& index = 0, const uint32_t& offset = 0);
	
	std::vector<::mmx::address_info_t> get_all_address_infos(const int32_t& index = 0);
	
	::mmx::account_t get_account(const uint32_t& index = 0);
	
	std::map<uint32_t, ::mmx::account_t> get_all_accounts();
	
	vnx::bool_t is_locked(const uint32_t& index = 0);
	
	void lock(const uint32_t& index = 0);
	
	void lock_async(const uint32_t& index = 0);
	
	void unlock(const uint32_t& index = 0, const std::string& passphrase = "");
	
	void unlock_async(const uint32_t& index = 0, const std::string& passphrase = "");
	
	void add_account(const uint32_t& index = 0, const ::mmx::account_t& config = ::mmx::account_t(), const vnx::optional<std::string>& passphrase = nullptr);
	
	void add_account_async(const uint32_t& index = 0, const ::mmx::account_t& config = ::mmx::account_t(), const vnx::optional<std::string>& passphrase = nullptr);
	
	void create_account(const ::mmx::account_t& config = ::mmx::account_t(), const vnx::optional<std::string>& passphrase = nullptr);
	
	void create_account_async(const ::mmx::account_t& config = ::mmx::account_t(), const vnx::optional<std::string>& passphrase = nullptr);
	
	void create_wallet(const ::mmx::account_t& config = ::mmx::account_t(), const vnx::optional<std::string>& words = nullptr, const vnx::optional<std::string>& passphrase = nullptr);
	
	void create_wallet_async(const ::mmx::account_t& config = ::mmx::account_t(), const vnx::optional<std::string>& words = nullptr, const vnx::optional<std::string>& passphrase = nullptr);
	
	std::vector<std::string> get_mnemonic_wordlist(const std::string& lang = "en");
	
	std::set<::mmx::addr_t> get_token_list();
	
	void add_token(const ::mmx::addr_t& address = ::mmx::addr_t());
	
	void add_token_async(const ::mmx::addr_t& address = ::mmx::addr_t());
	
	void rem_token(const ::mmx::addr_t& address = ::mmx::addr_t());
	
	void rem_token_async(const ::mmx::addr_t& address = ::mmx::addr_t());
	
	::mmx::hash_t get_master_seed(const uint32_t& index = 0);
	
	std::vector<std::string> get_mnemonic_seed(const uint32_t& index = 0);
	
	std::shared_ptr<const ::mmx::FarmerKeys> get_farmer_keys(const uint32_t& index = 0);
	
	std::vector<std::shared_ptr<const ::mmx::FarmerKeys>> get_all_farmer_keys();
	
	std::shared_ptr<const ::vnx::addons::HttpResponse> http_request(std::shared_ptr<const ::vnx::addons::HttpRequest> request = nullptr, const std::string& sub_path = "");
	
	std::shared_ptr<const ::vnx::addons::HttpData> http_request_chunk(std::shared_ptr<const ::vnx::addons::HttpRequest> request = nullptr, const std::string& sub_path = "", const int64_t& offset = 0, const int64_t& max_bytes = 0);
	
	::vnx::Object vnx_get_config_object();
	
	::vnx::Variant vnx_get_config(const std::string& name = "");
	
	void vnx_set_config_object(const ::vnx::Object& config = ::vnx::Object());
	
	void vnx_set_config_object_async(const ::vnx::Object& config = ::vnx::Object());
	
	void vnx_set_config(const std::string& name = "", const ::vnx::Variant& value = ::vnx::Variant());
	
	void vnx_set_config_async(const std::string& name = "", const ::vnx::Variant& value = ::vnx::Variant());
	
	::vnx::TypeCode vnx_get_type_code();
	
	std::shared_ptr<const ::vnx::ModuleInfo> vnx_get_module_info();
	
	void vnx_restart();
	
	void vnx_restart_async();
	
	void vnx_stop();
	
	void vnx_stop_async();
	
	vnx::bool_t vnx_self_test();
	
};


} // namespace mmx

#endif // INCLUDE_mmx_Wallet_CLIENT_HXX_
