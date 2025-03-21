
// AUTO GENERATED by vnxcppcodegen

#ifndef INCLUDE_mmx_query_filter_t_HXX_
#define INCLUDE_mmx_query_filter_t_HXX_

#include <vnx/Type.h>
#include <mmx/package.hxx>
#include <mmx/addr_t.hpp>
#include <mmx/tx_type_e.hxx>


namespace mmx {

struct MMX_EXPORT query_filter_t : vnx::struct_t {
	
	
	uint32_t since = 0;
	uint32_t until = -1;
	int32_t limit = -1;
	uint32_t max_search = 0;
	std::set<::mmx::addr_t> currency;
	vnx::optional<::mmx::tx_type_e> type;
	vnx::optional<std::string> memo;
	vnx::bool_t white_list = 0;
	vnx::bool_t with_pending = 0;
	
	static const vnx::Hash64 VNX_TYPE_HASH;
	static const vnx::Hash64 VNX_CODE_HASH;
	
	static constexpr uint64_t VNX_TYPE_ID = 0x92b02006aeea9a76ull;
	
	query_filter_t() {}
	
	vnx::Hash64 get_type_hash() const;
	std::string get_type_name() const;
	const vnx::TypeCode* get_type_code() const;
	
	static std::shared_ptr<query_filter_t> create();
	std::shared_ptr<query_filter_t> clone() const;
	
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
	
	friend std::ostream& operator<<(std::ostream& _out, const query_filter_t& _value);
	friend std::istream& operator>>(std::istream& _in, query_filter_t& _value);
	
	static const vnx::TypeCode* static_get_type_code();
	static std::shared_ptr<vnx::TypeCode> static_create_type_code();
	
};

template<typename T>
void query_filter_t::accept_generic(T& _visitor) const {
	_visitor.template type_begin<query_filter_t>(9);
	_visitor.type_field("since", 0); _visitor.accept(since);
	_visitor.type_field("until", 1); _visitor.accept(until);
	_visitor.type_field("limit", 2); _visitor.accept(limit);
	_visitor.type_field("max_search", 3); _visitor.accept(max_search);
	_visitor.type_field("currency", 4); _visitor.accept(currency);
	_visitor.type_field("type", 5); _visitor.accept(type);
	_visitor.type_field("memo", 6); _visitor.accept(memo);
	_visitor.type_field("white_list", 7); _visitor.accept(white_list);
	_visitor.type_field("with_pending", 8); _visitor.accept(with_pending);
	_visitor.template type_end<query_filter_t>(9);
}


} // namespace mmx


namespace vnx {

template<>
struct is_equivalent<::mmx::query_filter_t> {
	bool operator()(const uint16_t* code, const TypeCode* type_code);
};

} // vnx

#endif // INCLUDE_mmx_query_filter_t_HXX_
