
// AUTO GENERATED by vnxcppcodegen

#ifndef INCLUDE_mmx_ChainParams_HXX_
#define INCLUDE_mmx_ChainParams_HXX_

#include <mmx/package.hxx>
#include <mmx/addr_t.hpp>
#include <mmx/uint_fraction_t.hxx>
#include <vnx/Value.h>


namespace mmx {

class MMX_EXPORT ChainParams : public ::vnx::Value {
public:
	
	uint32_t port = 0;
	int32_t decimals = 6;
	uint32_t min_ksize = 29;
	uint32_t max_ksize = 32;
	uint32_t plot_filter = 4;
	uint32_t post_filter = 10;
	uint32_t commit_delay = 18;
	uint32_t infuse_delay = 6;
	uint32_t challenge_delay = 9;
	uint32_t challenge_interval = 48;
	uint32_t max_diff_adjust = 10;
	uint32_t max_vdf_count = 100;
	uint32_t avg_proof_count = 3;
	uint32_t max_proof_count = 50;
	uint32_t max_validators = 11;
	uint64_t min_reward = 200000;
	uint64_t vdf_reward = 500000;
	uint32_t vdf_reward_interval = 50;
	uint32_t vdf_segment_size = 50000;
	uint32_t reward_adjust_div = 100;
	uint32_t reward_adjust_tick = 10000;
	uint32_t reward_adjust_interval = 8640;
	uint32_t target_mmx_gold_price = 2000;
	uint64_t time_diff_divider = 1000;
	uint64_t time_diff_constant = 1000000;
	uint64_t space_diff_constant = 100000000;
	uint64_t initial_time_diff = 50;
	uint64_t initial_space_diff = 10;
	uint64_t initial_time_stamp = 0;
	uint64_t min_txfee = 100;
	uint64_t min_txfee_io = 100;
	uint64_t min_txfee_sign = 1000;
	uint64_t min_txfee_memo = 50;
	uint64_t min_txfee_exec = 10000;
	uint64_t min_txfee_deploy = 100000;
	uint64_t min_txfee_depend = 50000;
	uint64_t min_txfee_byte = 10;
	uint64_t min_txfee_read = 1000;
	uint64_t min_txfee_read_kbyte = 1000;
	uint64_t max_block_size = 10000000;
	uint64_t max_block_cost = 100000000;
	uint64_t max_tx_cost = 1000000;
	uint32_t max_rcall_depth = 3;
	uint32_t max_rcall_width = 10;
	std::vector<uint32_t> min_fee_ratio;
	int64_t block_interval_ms = 10000;
	std::string network;
	::mmx::addr_t nft_binary;
	::mmx::addr_t swap_binary;
	::mmx::addr_t offer_binary;
	::mmx::addr_t token_binary;
	::mmx::addr_t plot_nft_binary;
	::mmx::addr_t escrow_binary;
	::mmx::addr_t time_lock_binary;
	::mmx::addr_t relay_binary;
	uint64_t fixed_project_reward = 50000;
	::mmx::uint_fraction_t project_ratio;
	uint32_t reward_activation = 50000;
	uint32_t transaction_activation = 100000;
	uint32_t hardfork1_height = 1000000;
	
	typedef ::vnx::Value Super;
	
	static const vnx::Hash64 VNX_TYPE_HASH;
	static const vnx::Hash64 VNX_CODE_HASH;
	
	static constexpr uint64_t VNX_TYPE_ID = 0x51bba8d28881e8e7ull;
	
	ChainParams() {}
	
	vnx::Hash64 get_type_hash() const override;
	std::string get_type_name() const override;
	const vnx::TypeCode* get_type_code() const override;
	
	virtual vnx::float64_t get_block_time() const;
	
	static std::shared_ptr<ChainParams> create();
	std::shared_ptr<vnx::Value> clone() const override;
	
	void read(vnx::TypeInput& _in, const vnx::TypeCode* _type_code, const uint16_t* _code) override;
	void write(vnx::TypeOutput& _out, const vnx::TypeCode* _type_code, const uint16_t* _code) const override;
	
	void read(std::istream& _in) override;
	void write(std::ostream& _out) const override;
	
	template<typename T>
	void accept_generic(T& _visitor) const;
	void accept(vnx::Visitor& _visitor) const override;
	
	vnx::Object to_object() const override;
	void from_object(const vnx::Object& object) override;
	
	vnx::Variant get_field(const std::string& name) const override;
	void set_field(const std::string& name, const vnx::Variant& value) override;
	
	friend std::ostream& operator<<(std::ostream& _out, const ChainParams& _value);
	friend std::istream& operator>>(std::istream& _in, ChainParams& _value);
	
	static const vnx::TypeCode* static_get_type_code();
	static std::shared_ptr<vnx::TypeCode> static_create_type_code();
	
protected:
	std::shared_ptr<vnx::Value> vnx_call_switch(std::shared_ptr<const vnx::Value> _method) override;
	
};

template<typename T>
void ChainParams::accept_generic(T& _visitor) const {
	_visitor.template type_begin<ChainParams>(60);
	_visitor.type_field("port", 0); _visitor.accept(port);
	_visitor.type_field("decimals", 1); _visitor.accept(decimals);
	_visitor.type_field("min_ksize", 2); _visitor.accept(min_ksize);
	_visitor.type_field("max_ksize", 3); _visitor.accept(max_ksize);
	_visitor.type_field("plot_filter", 4); _visitor.accept(plot_filter);
	_visitor.type_field("post_filter", 5); _visitor.accept(post_filter);
	_visitor.type_field("commit_delay", 6); _visitor.accept(commit_delay);
	_visitor.type_field("infuse_delay", 7); _visitor.accept(infuse_delay);
	_visitor.type_field("challenge_delay", 8); _visitor.accept(challenge_delay);
	_visitor.type_field("challenge_interval", 9); _visitor.accept(challenge_interval);
	_visitor.type_field("max_diff_adjust", 10); _visitor.accept(max_diff_adjust);
	_visitor.type_field("max_vdf_count", 11); _visitor.accept(max_vdf_count);
	_visitor.type_field("avg_proof_count", 12); _visitor.accept(avg_proof_count);
	_visitor.type_field("max_proof_count", 13); _visitor.accept(max_proof_count);
	_visitor.type_field("max_validators", 14); _visitor.accept(max_validators);
	_visitor.type_field("min_reward", 15); _visitor.accept(min_reward);
	_visitor.type_field("vdf_reward", 16); _visitor.accept(vdf_reward);
	_visitor.type_field("vdf_reward_interval", 17); _visitor.accept(vdf_reward_interval);
	_visitor.type_field("vdf_segment_size", 18); _visitor.accept(vdf_segment_size);
	_visitor.type_field("reward_adjust_div", 19); _visitor.accept(reward_adjust_div);
	_visitor.type_field("reward_adjust_tick", 20); _visitor.accept(reward_adjust_tick);
	_visitor.type_field("reward_adjust_interval", 21); _visitor.accept(reward_adjust_interval);
	_visitor.type_field("target_mmx_gold_price", 22); _visitor.accept(target_mmx_gold_price);
	_visitor.type_field("time_diff_divider", 23); _visitor.accept(time_diff_divider);
	_visitor.type_field("time_diff_constant", 24); _visitor.accept(time_diff_constant);
	_visitor.type_field("space_diff_constant", 25); _visitor.accept(space_diff_constant);
	_visitor.type_field("initial_time_diff", 26); _visitor.accept(initial_time_diff);
	_visitor.type_field("initial_space_diff", 27); _visitor.accept(initial_space_diff);
	_visitor.type_field("initial_time_stamp", 28); _visitor.accept(initial_time_stamp);
	_visitor.type_field("min_txfee", 29); _visitor.accept(min_txfee);
	_visitor.type_field("min_txfee_io", 30); _visitor.accept(min_txfee_io);
	_visitor.type_field("min_txfee_sign", 31); _visitor.accept(min_txfee_sign);
	_visitor.type_field("min_txfee_memo", 32); _visitor.accept(min_txfee_memo);
	_visitor.type_field("min_txfee_exec", 33); _visitor.accept(min_txfee_exec);
	_visitor.type_field("min_txfee_deploy", 34); _visitor.accept(min_txfee_deploy);
	_visitor.type_field("min_txfee_depend", 35); _visitor.accept(min_txfee_depend);
	_visitor.type_field("min_txfee_byte", 36); _visitor.accept(min_txfee_byte);
	_visitor.type_field("min_txfee_read", 37); _visitor.accept(min_txfee_read);
	_visitor.type_field("min_txfee_read_kbyte", 38); _visitor.accept(min_txfee_read_kbyte);
	_visitor.type_field("max_block_size", 39); _visitor.accept(max_block_size);
	_visitor.type_field("max_block_cost", 40); _visitor.accept(max_block_cost);
	_visitor.type_field("max_tx_cost", 41); _visitor.accept(max_tx_cost);
	_visitor.type_field("max_rcall_depth", 42); _visitor.accept(max_rcall_depth);
	_visitor.type_field("max_rcall_width", 43); _visitor.accept(max_rcall_width);
	_visitor.type_field("min_fee_ratio", 44); _visitor.accept(min_fee_ratio);
	_visitor.type_field("block_interval_ms", 45); _visitor.accept(block_interval_ms);
	_visitor.type_field("network", 46); _visitor.accept(network);
	_visitor.type_field("nft_binary", 47); _visitor.accept(nft_binary);
	_visitor.type_field("swap_binary", 48); _visitor.accept(swap_binary);
	_visitor.type_field("offer_binary", 49); _visitor.accept(offer_binary);
	_visitor.type_field("token_binary", 50); _visitor.accept(token_binary);
	_visitor.type_field("plot_nft_binary", 51); _visitor.accept(plot_nft_binary);
	_visitor.type_field("escrow_binary", 52); _visitor.accept(escrow_binary);
	_visitor.type_field("time_lock_binary", 53); _visitor.accept(time_lock_binary);
	_visitor.type_field("relay_binary", 54); _visitor.accept(relay_binary);
	_visitor.type_field("fixed_project_reward", 55); _visitor.accept(fixed_project_reward);
	_visitor.type_field("project_ratio", 56); _visitor.accept(project_ratio);
	_visitor.type_field("reward_activation", 57); _visitor.accept(reward_activation);
	_visitor.type_field("transaction_activation", 58); _visitor.accept(transaction_activation);
	_visitor.type_field("hardfork1_height", 59); _visitor.accept(hardfork1_height);
	_visitor.template type_end<ChainParams>(60);
}


} // namespace mmx


namespace vnx {

} // vnx

#endif // INCLUDE_mmx_ChainParams_HXX_
