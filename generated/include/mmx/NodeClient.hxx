
// AUTO GENERATED by vnxcppcodegen

#ifndef INCLUDE_mmx_Node_CLIENT_HXX_
#define INCLUDE_mmx_Node_CLIENT_HXX_

#include <vnx/Client.h>
#include <mmx/Block.hxx>
#include <mmx/BlockHeader.hxx>
#include <mmx/ChainParams.hxx>
#include <mmx/Contract.hxx>
#include <mmx/NetworkInfo.hxx>
#include <mmx/Partial.hxx>
#include <mmx/ProofOfTime.hxx>
#include <mmx/ProofResponse.hxx>
#include <mmx/Transaction.hxx>
#include <mmx/VDF_Point.hxx>
#include <mmx/ValidatorVote.hxx>
#include <mmx/addr_t.hpp>
#include <mmx/balance_t.hxx>
#include <mmx/exec_entry_t.hxx>
#include <mmx/exec_result_t.hxx>
#include <mmx/farmed_block_summary_t.hxx>
#include <mmx/hash_t.hpp>
#include <mmx/offer_data_t.hxx>
#include <mmx/plot_nft_info_t.hxx>
#include <mmx/pooling_error_e.hxx>
#include <mmx/pubkey_t.hpp>
#include <mmx/query_filter_t.hxx>
#include <mmx/swap_entry_t.hxx>
#include <mmx/swap_info_t.hxx>
#include <mmx/swap_user_info_t.hxx>
#include <mmx/trade_entry_t.hxx>
#include <mmx/tx_entry_t.hxx>
#include <mmx/tx_info_t.hxx>
#include <mmx/uint128.hpp>
#include <mmx/vm/varptr_t.hpp>
#include <vnx/Module.h>
#include <vnx/TopicPtr.hpp>
#include <vnx/Variant.hpp>
#include <vnx/addons/HttpData.hxx>
#include <vnx/addons/HttpRequest.hxx>
#include <vnx/addons/HttpResponse.hxx>


namespace mmx {

class NodeClient : public vnx::Client {
public:
	NodeClient(const std::string& service_name);
	
	NodeClient(vnx::Hash64 service_addr);
	
	std::shared_ptr<const ::mmx::ChainParams> get_params();
	
	std::shared_ptr<const ::mmx::NetworkInfo> get_network_info();
	
	::mmx::hash_t get_genesis_hash();
	
	uint32_t get_height();
	
	vnx::optional<uint32_t> get_synced_height();
	
	uint32_t get_vdf_height();
	
	::mmx::hash_t get_vdf_peak();
	
	std::shared_ptr<const ::mmx::Block> get_block(const ::mmx::hash_t& hash = ::mmx::hash_t());
	
	std::shared_ptr<const ::mmx::Block> get_block_at(const uint32_t& height = 0);
	
	std::shared_ptr<const ::mmx::BlockHeader> get_header(const ::mmx::hash_t& hash = ::mmx::hash_t());
	
	std::shared_ptr<const ::mmx::BlockHeader> get_header_at(const uint32_t& height = 0);
	
	vnx::optional<::mmx::hash_t> get_block_hash(const uint32_t& height = 0);
	
	vnx::optional<std::pair<::mmx::hash_t, ::mmx::hash_t>> get_block_hash_ex(const uint32_t& height = 0);
	
	vnx::optional<uint32_t> get_tx_height(const ::mmx::hash_t& id = ::mmx::hash_t());
	
	vnx::optional<::mmx::tx_info_t> get_tx_info(const ::mmx::hash_t& id = ::mmx::hash_t());
	
	vnx::optional<::mmx::tx_info_t> get_tx_info_for(std::shared_ptr<const ::mmx::Transaction> tx = nullptr);
	
	std::vector<::mmx::hash_t> get_tx_ids(const uint32_t& limit = 0);
	
	std::vector<::mmx::hash_t> get_tx_ids_at(const uint32_t& height = 0);
	
	std::vector<::mmx::hash_t> get_tx_ids_since(const uint32_t& height = 0);
	
	::mmx::exec_result_t validate(std::shared_ptr<const ::mmx::Transaction> tx = nullptr);
	
	void add_block(std::shared_ptr<const ::mmx::Block> block = nullptr);
	
	void add_block_async(std::shared_ptr<const ::mmx::Block> block = nullptr);
	
	void add_transaction(std::shared_ptr<const ::mmx::Transaction> tx = nullptr, const vnx::bool_t& pre_validate = 0);
	
	void add_transaction_async(std::shared_ptr<const ::mmx::Transaction> tx = nullptr, const vnx::bool_t& pre_validate = 0);
	
	std::shared_ptr<const ::mmx::Contract> get_contract(const ::mmx::addr_t& address = ::mmx::addr_t());
	
	std::shared_ptr<const ::mmx::Contract> get_contract_for(const ::mmx::addr_t& address = ::mmx::addr_t());
	
	std::vector<std::shared_ptr<const ::mmx::Contract>> get_contracts(const std::vector<::mmx::addr_t>& addresses = {});
	
	std::vector<::mmx::addr_t> get_contracts_by(const std::vector<::mmx::addr_t>& addresses = {}, const vnx::optional<::mmx::hash_t>& type_hash = nullptr);
	
	std::vector<::mmx::addr_t> get_contracts_owned_by(const std::vector<::mmx::addr_t>& addresses = {}, const vnx::optional<::mmx::hash_t>& type_hash = nullptr);
	
	std::shared_ptr<const ::mmx::Transaction> get_transaction(const ::mmx::hash_t& id = ::mmx::hash_t(), const vnx::bool_t& pending = 0);
	
	std::vector<std::shared_ptr<const ::mmx::Transaction>> get_transactions(const std::vector<::mmx::hash_t>& ids = {});
	
	std::vector<::mmx::tx_entry_t> get_history(const std::vector<::mmx::addr_t>& addresses = {}, const ::mmx::query_filter_t& filter = ::mmx::query_filter_t());
	
	std::vector<::mmx::tx_entry_t> get_history_memo(const std::vector<::mmx::addr_t>& addresses = {}, const std::string& memo = "", const ::mmx::query_filter_t& filter = ::mmx::query_filter_t());
	
	::mmx::uint128 get_balance(const ::mmx::addr_t& address = ::mmx::addr_t(), const ::mmx::addr_t& currency = ::mmx::addr_t());
	
	std::map<::mmx::addr_t, ::mmx::uint128> get_balances(const ::mmx::addr_t& address = ::mmx::addr_t(), const std::set<::mmx::addr_t>& whitelist = {}, const int32_t& limit = 100);
	
	std::map<::mmx::addr_t, ::mmx::balance_t> get_contract_balances(const ::mmx::addr_t& address = ::mmx::addr_t(), const std::set<::mmx::addr_t>& whitelist = {}, const int32_t& limit = 100);
	
	::mmx::uint128 get_total_balance(const std::vector<::mmx::addr_t>& addresses = {}, const ::mmx::addr_t& currency = ::mmx::addr_t());
	
	std::map<::mmx::addr_t, ::mmx::uint128> get_total_balances(const std::vector<::mmx::addr_t>& addresses = {}, const std::set<::mmx::addr_t>& whitelist = {}, const int32_t& limit = 100);
	
	std::map<std::pair<::mmx::addr_t, ::mmx::addr_t>, ::mmx::uint128> get_all_balances(const std::vector<::mmx::addr_t>& addresses = {}, const std::set<::mmx::addr_t>& whitelist = {}, const int32_t& limit = 100);
	
	std::vector<::mmx::exec_entry_t> get_exec_history(const ::mmx::addr_t& address = ::mmx::addr_t(), const int32_t& limit = 100, const vnx::bool_t& recent = 0);
	
	std::map<std::string, ::mmx::vm::varptr_t> read_storage(const ::mmx::addr_t& contract = ::mmx::addr_t(), const uint32_t& height = -1);
	
	std::map<uint64_t, ::mmx::vm::varptr_t> dump_storage(const ::mmx::addr_t& contract = ::mmx::addr_t(), const uint32_t& height = -1);
	
	::mmx::vm::varptr_t read_storage_var(const ::mmx::addr_t& contract = ::mmx::addr_t(), const uint64_t& address = 0, const uint32_t& height = -1);
	
	::mmx::vm::varptr_t read_storage_entry_var(const ::mmx::addr_t& contract = ::mmx::addr_t(), const uint64_t& address = 0, const uint64_t& key = 0, const uint32_t& height = -1);
	
	std::pair<::mmx::vm::varptr_t, uint64_t> read_storage_field(const ::mmx::addr_t& contract = ::mmx::addr_t(), const std::string& name = "", const uint32_t& height = -1);
	
	std::tuple<::mmx::vm::varptr_t, uint64_t, uint64_t> read_storage_entry_addr(const ::mmx::addr_t& contract = ::mmx::addr_t(), const std::string& name = "", const ::mmx::addr_t& key = ::mmx::addr_t(), const uint32_t& height = -1);
	
	std::tuple<::mmx::vm::varptr_t, uint64_t, uint64_t> read_storage_entry_string(const ::mmx::addr_t& contract = ::mmx::addr_t(), const std::string& name = "", const std::string& key = "", const uint32_t& height = -1);
	
	std::vector<::mmx::vm::varptr_t> read_storage_array(const ::mmx::addr_t& contract = ::mmx::addr_t(), const uint64_t& address = 0, const uint32_t& height = -1);
	
	std::map<::mmx::vm::varptr_t, ::mmx::vm::varptr_t> read_storage_map(const ::mmx::addr_t& contract = ::mmx::addr_t(), const uint64_t& address = 0, const uint32_t& height = -1);
	
	std::map<std::string, ::mmx::vm::varptr_t> read_storage_object(const ::mmx::addr_t& contract = ::mmx::addr_t(), const uint64_t& address = 0, const uint32_t& height = -1);
	
	::vnx::Variant call_contract(const ::mmx::addr_t& address = ::mmx::addr_t(), const std::string& method = "", const std::vector<::vnx::Variant>& args = {}, const vnx::optional<::mmx::addr_t>& user = nullptr, const vnx::optional<std::pair<::mmx::addr_t, ::mmx::uint128>>& deposit = nullptr);
	
	vnx::optional<::mmx::plot_nft_info_t> get_plot_nft_info(const ::mmx::addr_t& address = ::mmx::addr_t());
	
	::mmx::addr_t get_plot_nft_target(const ::mmx::addr_t& address = ::mmx::addr_t(), const vnx::optional<::mmx::addr_t>& farmer_addr = nullptr);
	
	::mmx::offer_data_t get_offer(const ::mmx::addr_t& address = ::mmx::addr_t());
	
	std::vector<::mmx::offer_data_t> get_offers(const uint32_t& since = 0, const vnx::bool_t& state = 0);
	
	std::vector<::mmx::offer_data_t> get_offers_by(const std::vector<::mmx::addr_t>& owners = {}, const vnx::bool_t& state = 0);
	
	std::vector<::mmx::offer_data_t> fetch_offers(const std::vector<::mmx::addr_t>& addresses = {}, const vnx::bool_t& state = 0, const vnx::bool_t& closed = 0);
	
	std::vector<::mmx::offer_data_t> get_recent_offers(const int32_t& limit = 100, const vnx::bool_t& state = true);
	
	std::vector<::mmx::offer_data_t> get_recent_offers_for(const vnx::optional<::mmx::addr_t>& bid = nullptr, const vnx::optional<::mmx::addr_t>& ask = nullptr, const ::mmx::uint128& min_bid = ::mmx::uint128(), const int32_t& limit = 100, const vnx::bool_t& state = true);
	
	std::vector<::mmx::trade_entry_t> get_trade_history(const int32_t& limit = 100, const uint32_t& since = 0);
	
	std::vector<::mmx::trade_entry_t> get_trade_history_for(const vnx::optional<::mmx::addr_t>& bid = nullptr, const vnx::optional<::mmx::addr_t>& ask = nullptr, const int32_t& limit = 100, const uint32_t& since = 0);
	
	std::vector<::mmx::swap_info_t> get_swaps(const uint32_t& since = 0, const vnx::optional<::mmx::addr_t>& token = nullptr, const vnx::optional<::mmx::addr_t>& currency = nullptr, const int32_t& limit = 100);
	
	::mmx::swap_info_t get_swap_info(const ::mmx::addr_t& address = ::mmx::addr_t());
	
	::mmx::swap_user_info_t get_swap_user_info(const ::mmx::addr_t& address = ::mmx::addr_t(), const ::mmx::addr_t& user = ::mmx::addr_t());
	
	std::vector<::mmx::swap_entry_t> get_swap_history(const ::mmx::addr_t& address = ::mmx::addr_t(), const int32_t& limit = 100);
	
	std::array<::mmx::uint128, 2> get_swap_trade_estimate(const ::mmx::addr_t& address = ::mmx::addr_t(), const uint32_t& i = 0, const ::mmx::uint128& amount = ::mmx::uint128(), const int32_t& num_iter = 20);
	
	std::array<::mmx::uint128, 2> get_swap_fees_earned(const ::mmx::addr_t& address = ::mmx::addr_t(), const ::mmx::addr_t& user = ::mmx::addr_t());
	
	std::array<::mmx::uint128, 2> get_swap_equivalent_liquidity(const ::mmx::addr_t& address = ::mmx::addr_t(), const ::mmx::addr_t& user = ::mmx::addr_t());
	
	std::map<::mmx::addr_t, std::array<std::pair<::mmx::addr_t, ::mmx::uint128>, 2>> get_swap_liquidity_by(const std::vector<::mmx::addr_t>& addresses = {});
	
	::mmx::uint128 get_total_supply(const ::mmx::addr_t& currency = ::mmx::addr_t());
	
	std::vector<std::shared_ptr<const ::mmx::BlockHeader>> get_farmed_blocks(const std::vector<::mmx::pubkey_t>& farmer_keys = {}, const vnx::bool_t& full_blocks = 0, const uint32_t& since = 0, const int32_t& limit = 100);
	
	::mmx::farmed_block_summary_t get_farmed_block_summary(const std::vector<::mmx::pubkey_t>& farmer_keys = {}, const uint32_t& since = 0);
	
	std::vector<std::pair<::mmx::pubkey_t, uint32_t>> get_farmer_ranking(const int32_t& limit = -1);
	
	std::tuple<::mmx::pooling_error_e, std::string> verify_plot_nft_target(const ::mmx::addr_t& address = ::mmx::addr_t(), const ::mmx::addr_t& pool_target = ::mmx::addr_t());
	
	std::tuple<::mmx::pooling_error_e, std::string> verify_partial(std::shared_ptr<const ::mmx::Partial> partial = nullptr, const vnx::optional<::mmx::addr_t>& pool_target = nullptr);
	
	void start_sync(const vnx::bool_t& force = 0);
	
	void start_sync_async(const vnx::bool_t& force = 0);
	
	void revert_sync(const uint32_t& height = 0);
	
	void revert_sync_async(const uint32_t& height = 0);
	
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

#endif // INCLUDE_mmx_Node_CLIENT_HXX_
