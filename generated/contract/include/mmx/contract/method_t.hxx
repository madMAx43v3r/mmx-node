
// AUTO GENERATED by vnxcppcodegen

#ifndef INCLUDE_mmx_contract_method_t_HXX_
#define INCLUDE_mmx_contract_method_t_HXX_

#include <vnx/Type.h>
#include <mmx/contract/package.hxx>


namespace mmx {
namespace contract {

struct MMX_CONTRACT_EXPORT method_t : vnx::struct_t {
	
	
	std::string name;
	std::string info;
	vnx::bool_t is_init = 0;
	vnx::bool_t is_const = 0;
	vnx::bool_t is_public = 0;
	vnx::bool_t is_payable = 0;
	uint32_t entry_point = 0;
	std::vector<std::string> args;
	
	static const vnx::Hash64 VNX_TYPE_HASH;
	static const vnx::Hash64 VNX_CODE_HASH;
	
	static constexpr uint64_t VNX_TYPE_ID = 0x1f62512698176a39ull;
	
	method_t() {}
	
	vnx::Hash64 get_type_hash() const;
	std::string get_type_name() const;
	const vnx::TypeCode* get_type_code() const;
	
	uint64_t num_bytes() const;
	
	static std::shared_ptr<method_t> create();
	std::shared_ptr<method_t> clone() const;
	
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
	
	friend std::ostream& operator<<(std::ostream& _out, const method_t& _value);
	friend std::istream& operator>>(std::istream& _in, method_t& _value);
	
	static const vnx::TypeCode* static_get_type_code();
	static std::shared_ptr<vnx::TypeCode> static_create_type_code();
	
};

template<typename T>
void method_t::accept_generic(T& _visitor) const {
	_visitor.template type_begin<method_t>(8);
	_visitor.type_field("name", 0); _visitor.accept(name);
	_visitor.type_field("info", 1); _visitor.accept(info);
	_visitor.type_field("is_init", 2); _visitor.accept(is_init);
	_visitor.type_field("is_const", 3); _visitor.accept(is_const);
	_visitor.type_field("is_public", 4); _visitor.accept(is_public);
	_visitor.type_field("is_payable", 5); _visitor.accept(is_payable);
	_visitor.type_field("entry_point", 6); _visitor.accept(entry_point);
	_visitor.type_field("args", 7); _visitor.accept(args);
	_visitor.template type_end<method_t>(8);
}


} // namespace mmx
} // namespace contract


namespace vnx {

template<>
struct is_equivalent<::mmx::contract::method_t> {
	bool operator()(const uint16_t* code, const TypeCode* type_code);
};

} // vnx

#endif // INCLUDE_mmx_contract_method_t_HXX_
