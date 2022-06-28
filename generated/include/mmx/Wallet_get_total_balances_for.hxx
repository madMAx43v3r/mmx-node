
// AUTO GENERATED by vnxcppcodegen

#ifndef INCLUDE_mmx_Wallet_get_total_balances_for_HXX_
#define INCLUDE_mmx_Wallet_get_total_balances_for_HXX_

#include <mmx/package.hxx>
#include <mmx/addr_t.hpp>
#include <vnx/Value.h>


namespace mmx {

class MMX_EXPORT Wallet_get_total_balances_for : public ::vnx::Value {
public:
	
	std::vector<::mmx::addr_t> addresses;
	uint32_t min_confirm = 1;
	
	typedef ::vnx::Value Super;
	
	static const vnx::Hash64 VNX_TYPE_HASH;
	static const vnx::Hash64 VNX_CODE_HASH;
	
	static constexpr uint64_t VNX_TYPE_ID = 0x784998cd7be76f95ull;
	
	Wallet_get_total_balances_for() {}
	
	vnx::Hash64 get_type_hash() const override;
	std::string get_type_name() const override;
	const vnx::TypeCode* get_type_code() const override;
	
	static std::shared_ptr<Wallet_get_total_balances_for> create();
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
	
	friend std::ostream& operator<<(std::ostream& _out, const Wallet_get_total_balances_for& _value);
	friend std::istream& operator>>(std::istream& _in, Wallet_get_total_balances_for& _value);
	
	static const vnx::TypeCode* static_get_type_code();
	static std::shared_ptr<vnx::TypeCode> static_create_type_code();
	
};

template<typename T>
void Wallet_get_total_balances_for::accept_generic(T& _visitor) const {
	_visitor.template type_begin<Wallet_get_total_balances_for>(2);
	_visitor.type_field("addresses", 0); _visitor.accept(addresses);
	_visitor.type_field("min_confirm", 1); _visitor.accept(min_confirm);
	_visitor.template type_end<Wallet_get_total_balances_for>(2);
}


} // namespace mmx


namespace vnx {

} // vnx

#endif // INCLUDE_mmx_Wallet_get_total_balances_for_HXX_
