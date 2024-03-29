
// AUTO GENERATED by vnxcppcodegen

#ifndef INCLUDE_mmx_address_info_t_HXX_
#define INCLUDE_mmx_address_info_t_HXX_

#include <vnx/Type.h>
#include <mmx/package.hxx>
#include <mmx/addr_t.hpp>
#include <mmx/uint128.hpp>


namespace mmx {

struct MMX_EXPORT address_info_t {
	
	
	::mmx::addr_t address;
	uint32_t num_active = 0;
	uint32_t num_spend = 0;
	uint32_t num_receive = 0;
	uint32_t last_spend_height = 0;
	uint32_t last_receive_height = 0;
	std::map<::mmx::addr_t, ::mmx::uint128> total_spend;
	std::map<::mmx::addr_t, ::mmx::uint128> total_receive;
	
	static const vnx::Hash64 VNX_TYPE_HASH;
	static const vnx::Hash64 VNX_CODE_HASH;
	
	static constexpr uint64_t VNX_TYPE_ID = 0xbafe22d4f9e3d761ull;
	
	address_info_t() {}
	
	vnx::Hash64 get_type_hash() const;
	std::string get_type_name() const;
	const vnx::TypeCode* get_type_code() const;
	
	static std::shared_ptr<address_info_t> create();
	std::shared_ptr<address_info_t> clone() const;
	
	void read(vnx::TypeInput& _in, const vnx::TypeCode* _type_code, const uint16_t* _code);
	void write(vnx::TypeOutput& _out, const vnx::TypeCode* _type_code, const uint16_t* _code) const;
	
	void read(std::istream& _in);
	void write(std::ostream& _out) const;
	
	template<typename T>
	void accept_generic(T& _visitor) const;
	void accept(vnx::Visitor& _visitor) const;
	
	vnx::Object to_object() const;
	void from_object(const vnx::Object& object);
	
	vnx::Variant get_field(const std::string& name) const;
	void set_field(const std::string& name, const vnx::Variant& value);
	
	friend std::ostream& operator<<(std::ostream& _out, const address_info_t& _value);
	friend std::istream& operator>>(std::istream& _in, address_info_t& _value);
	
	static const vnx::TypeCode* static_get_type_code();
	static std::shared_ptr<vnx::TypeCode> static_create_type_code();
	
};

template<typename T>
void address_info_t::accept_generic(T& _visitor) const {
	_visitor.template type_begin<address_info_t>(8);
	_visitor.type_field("address", 0); _visitor.accept(address);
	_visitor.type_field("num_active", 1); _visitor.accept(num_active);
	_visitor.type_field("num_spend", 2); _visitor.accept(num_spend);
	_visitor.type_field("num_receive", 3); _visitor.accept(num_receive);
	_visitor.type_field("last_spend_height", 4); _visitor.accept(last_spend_height);
	_visitor.type_field("last_receive_height", 5); _visitor.accept(last_receive_height);
	_visitor.type_field("total_spend", 6); _visitor.accept(total_spend);
	_visitor.type_field("total_receive", 7); _visitor.accept(total_receive);
	_visitor.template type_end<address_info_t>(8);
}


} // namespace mmx


namespace vnx {

template<>
struct is_equivalent<::mmx::address_info_t> {
	bool operator()(const uint16_t* code, const TypeCode* type_code);
};

} // vnx

#endif // INCLUDE_mmx_address_info_t_HXX_
