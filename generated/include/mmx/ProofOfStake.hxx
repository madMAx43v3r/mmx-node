
// AUTO GENERATED by vnxcppcodegen

#ifndef INCLUDE_mmx_ProofOfStake_HXX_
#define INCLUDE_mmx_ProofOfStake_HXX_

#include <mmx/package.hxx>
#include <mmx/ProofOfSpace.hxx>
#include <mmx/addr_t.hpp>
#include <mmx/hash_t.hpp>


namespace mmx {

class MMX_EXPORT ProofOfStake : public ::mmx::ProofOfSpace {
public:
	
	::mmx::addr_t contract;
	
	typedef ::mmx::ProofOfSpace Super;
	
	static const vnx::Hash64 VNX_TYPE_HASH;
	static const vnx::Hash64 VNX_CODE_HASH;
	
	static constexpr uint64_t VNX_TYPE_ID = 0xf5f1629c4ada2ccfull;
	
	ProofOfStake() {}
	
	vnx::Hash64 get_type_hash() const override;
	std::string get_type_name() const override;
	const vnx::TypeCode* get_type_code() const override;
	
	virtual ::mmx::hash_t calc_hash(const vnx::bool_t& full_hash = false) const override;
	virtual void validate() const override;
	
	static std::shared_ptr<ProofOfStake> create();
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
	
	friend std::ostream& operator<<(std::ostream& _out, const ProofOfStake& _value);
	friend std::istream& operator>>(std::istream& _in, ProofOfStake& _value);
	
	static const vnx::TypeCode* static_get_type_code();
	static std::shared_ptr<vnx::TypeCode> static_create_type_code();
	
protected:
	std::shared_ptr<vnx::Value> vnx_call_switch(std::shared_ptr<const vnx::Value> _method) override;
	
};

template<typename T>
void ProofOfStake::accept_generic(T& _visitor) const {
	_visitor.template type_begin<ProofOfStake>(6);
	_visitor.type_field("version", 0); _visitor.accept(version);
	_visitor.type_field("score", 1); _visitor.accept(score);
	_visitor.type_field("plot_id", 2); _visitor.accept(plot_id);
	_visitor.type_field("plot_key", 3); _visitor.accept(plot_key);
	_visitor.type_field("farmer_key", 4); _visitor.accept(farmer_key);
	_visitor.type_field("contract", 5); _visitor.accept(contract);
	_visitor.template type_end<ProofOfStake>(6);
}


} // namespace mmx


namespace vnx {

} // vnx

#endif // INCLUDE_mmx_ProofOfStake_HXX_
