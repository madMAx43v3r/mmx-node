
// AUTO GENERATED by vnxcppcodegen

#ifndef INCLUDE_mmx_Wallet_swap_trade_HXX_
#define INCLUDE_mmx_Wallet_swap_trade_HXX_

#include <mmx/package.hxx>
#include <mmx/addr_t.hpp>
#include <mmx/spend_options_t.hxx>
#include <vnx/Value.h>


namespace mmx {

class MMX_EXPORT Wallet_swap_trade : public ::vnx::Value {
public:
	
	uint32_t index = 0;
	::mmx::addr_t address;
	uint64_t amount = 0;
	::mmx::addr_t currency;
	vnx::optional<uint64_t> min_trade;
	int32_t num_iter = 20;
	::mmx::spend_options_t options;
	
	typedef ::vnx::Value Super;
	
	static const vnx::Hash64 VNX_TYPE_HASH;
	static const vnx::Hash64 VNX_CODE_HASH;
	
	static constexpr uint64_t VNX_TYPE_ID = 0x4b5a42cbf6657910ull;
	
	Wallet_swap_trade() {}
	
	vnx::Hash64 get_type_hash() const override;
	std::string get_type_name() const override;
	const vnx::TypeCode* get_type_code() const override;
	
	static std::shared_ptr<Wallet_swap_trade> create();
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
	
	friend std::ostream& operator<<(std::ostream& _out, const Wallet_swap_trade& _value);
	friend std::istream& operator>>(std::istream& _in, Wallet_swap_trade& _value);
	
	static const vnx::TypeCode* static_get_type_code();
	static std::shared_ptr<vnx::TypeCode> static_create_type_code();
	
};

template<typename T>
void Wallet_swap_trade::accept_generic(T& _visitor) const {
	_visitor.template type_begin<Wallet_swap_trade>(7);
	_visitor.type_field("index", 0); _visitor.accept(index);
	_visitor.type_field("address", 1); _visitor.accept(address);
	_visitor.type_field("amount", 2); _visitor.accept(amount);
	_visitor.type_field("currency", 3); _visitor.accept(currency);
	_visitor.type_field("min_trade", 4); _visitor.accept(min_trade);
	_visitor.type_field("num_iter", 5); _visitor.accept(num_iter);
	_visitor.type_field("options", 6); _visitor.accept(options);
	_visitor.template type_end<Wallet_swap_trade>(7);
}


} // namespace mmx


namespace vnx {

} // vnx

#endif // INCLUDE_mmx_Wallet_swap_trade_HXX_
