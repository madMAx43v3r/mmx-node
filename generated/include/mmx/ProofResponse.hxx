
// AUTO GENERATED by vnxcppcodegen

#ifndef INCLUDE_mmx_ProofResponse_HXX_
#define INCLUDE_mmx_ProofResponse_HXX_

#include <mmx/package.hxx>
#include <mmx/ProofOfSpace.hxx>
#include <mmx/hash_t.hpp>
#include <mmx/signature_t.hpp>
#include <vnx/Hash64.hpp>
#include <vnx/Value.h>


namespace mmx {

class MMX_EXPORT ProofResponse : public ::vnx::Value {
public:
	
	::mmx::hash_t hash;
	uint32_t vdf_height = 0;
	std::shared_ptr<const ::mmx::ProofOfSpace> proof;
	::mmx::signature_t farmer_sig;
	::mmx::hash_t content_hash;
	::vnx::Hash64 farmer_addr;
	std::string harvester;
	int64_t lookup_time_ms = 0;
	
	typedef ::vnx::Value Super;
	
	static const vnx::Hash64 VNX_TYPE_HASH;
	static const vnx::Hash64 VNX_CODE_HASH;
	
	static constexpr uint64_t VNX_TYPE_ID = 0x816e898b36befae0ull;
	
	ProofResponse() {}
	
	vnx::Hash64 get_type_hash() const override;
	std::string get_type_name() const override;
	const vnx::TypeCode* get_type_code() const override;
	
	virtual vnx::bool_t is_valid() const;
	virtual ::mmx::hash_t calc_hash() const;
	virtual ::mmx::hash_t calc_content_hash() const;
	virtual void validate() const;
	
	static std::shared_ptr<ProofResponse> create();
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
	
	friend std::ostream& operator<<(std::ostream& _out, const ProofResponse& _value);
	friend std::istream& operator>>(std::istream& _in, ProofResponse& _value);
	
	static const vnx::TypeCode* static_get_type_code();
	static std::shared_ptr<vnx::TypeCode> static_create_type_code();
	
protected:
	std::shared_ptr<vnx::Value> vnx_call_switch(std::shared_ptr<const vnx::Value> _method) override;
	
};

template<typename T>
void ProofResponse::accept_generic(T& _visitor) const {
	_visitor.template type_begin<ProofResponse>(8);
	_visitor.type_field("hash", 0); _visitor.accept(hash);
	_visitor.type_field("vdf_height", 1); _visitor.accept(vdf_height);
	_visitor.type_field("proof", 2); _visitor.accept(proof);
	_visitor.type_field("farmer_sig", 3); _visitor.accept(farmer_sig);
	_visitor.type_field("content_hash", 4); _visitor.accept(content_hash);
	_visitor.type_field("farmer_addr", 5); _visitor.accept(farmer_addr);
	_visitor.type_field("harvester", 6); _visitor.accept(harvester);
	_visitor.type_field("lookup_time_ms", 7); _visitor.accept(lookup_time_ms);
	_visitor.template type_end<ProofResponse>(8);
}


} // namespace mmx


namespace vnx {

} // vnx

#endif // INCLUDE_mmx_ProofResponse_HXX_
