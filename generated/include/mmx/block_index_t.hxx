
// AUTO GENERATED by vnxcppcodegen

#ifndef INCLUDE_mmx_block_index_t_HXX_
#define INCLUDE_mmx_block_index_t_HXX_

#include <vnx/Type.h>
#include <mmx/package.hxx>
#include <mmx/hash_t.hpp>


namespace mmx {

struct MMX_EXPORT block_index_t : vnx::struct_t {
	
	
	::mmx::hash_t hash;
	::mmx::hash_t content_hash;
	uint32_t static_cost = 0;
	uint32_t total_cost = 0;
	int64_t file_offset = 0;
	
	static const vnx::Hash64 VNX_TYPE_HASH;
	static const vnx::Hash64 VNX_CODE_HASH;
	
	static constexpr uint64_t VNX_TYPE_ID = 0xd00c722670bca900ull;
	
	block_index_t() {}
	
	vnx::Hash64 get_type_hash() const;
	std::string get_type_name() const;
	const vnx::TypeCode* get_type_code() const;
	
	static std::shared_ptr<block_index_t> create();
	std::shared_ptr<block_index_t> clone() const;
	
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
	
	friend std::ostream& operator<<(std::ostream& _out, const block_index_t& _value);
	friend std::istream& operator>>(std::istream& _in, block_index_t& _value);
	
	static const vnx::TypeCode* static_get_type_code();
	static std::shared_ptr<vnx::TypeCode> static_create_type_code();
	
};

template<typename T>
void block_index_t::accept_generic(T& _visitor) const {
	_visitor.template type_begin<block_index_t>(5);
	_visitor.type_field("hash", 0); _visitor.accept(hash);
	_visitor.type_field("content_hash", 1); _visitor.accept(content_hash);
	_visitor.type_field("static_cost", 2); _visitor.accept(static_cost);
	_visitor.type_field("total_cost", 3); _visitor.accept(total_cost);
	_visitor.type_field("file_offset", 4); _visitor.accept(file_offset);
	_visitor.template type_end<block_index_t>(5);
}


} // namespace mmx


namespace vnx {

template<>
struct is_equivalent<::mmx::block_index_t> {
	bool operator()(const uint16_t* code, const TypeCode* type_code);
};

} // vnx

#endif // INCLUDE_mmx_block_index_t_HXX_
