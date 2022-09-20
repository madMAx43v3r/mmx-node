/*
 * addr_t.hpp
 *
 *  Created on: Dec 12, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_ADDR_T_HPP_
#define INCLUDE_MMX_ADDR_T_HPP_

#include <mmx/hash_t.hpp>
#include <mmx/pubkey_t.hpp>


namespace mmx {

class addr_t : public hash_t {
public:
	typedef hash_t super_t;

	addr_t() = default;

	addr_t(const hash_t& hash);

	addr_t(const pubkey_t& key);

	addr_t(const uint256_t& value);

	addr_t(const std::string& addr);

	std::string to_string() const;

	void from_string(const std::string& addr);

};


inline
addr_t::addr_t(const hash_t& hash)
	:	super_t(hash)
{
}

inline
addr_t::addr_t(const pubkey_t& key)
	:	super_t(key.get_addr())
{
}

inline
addr_t::addr_t(const uint256_t& value)
	:	super_t(hash_t::from_bytes(&value))
{
}

inline
addr_t::addr_t(const std::string& addr)
{
	from_string(addr);
}

inline
std::ostream& operator<<(std::ostream& out, const addr_t& addr) {
	return out << addr.to_string();
}


namespace vm {

class var_t;

addr_t to_addr(const var_t* var);

} // vm
} //mmx


namespace vnx {

void read(vnx::TypeInput& in, mmx::addr_t& value, const vnx::TypeCode* type_code, const uint16_t* code);

inline
void write(vnx::TypeOutput& out, const mmx::addr_t& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr) {
	vnx::write(out, value.bytes, type_code, code);
}

inline
void read(std::istream& in, mmx::addr_t& value) {
	std::string tmp;
	vnx::read(in, tmp);
	value.from_string(tmp);
}

inline
void write(std::ostream& out, const mmx::addr_t& value) {
	vnx::write(out, value.to_string());
}

inline
void accept(vnx::Visitor& visitor, const mmx::addr_t& value) {
	vnx::accept(visitor, value.to_string());
}

} // vnx


namespace std {
	template<>
	struct hash<typename mmx::addr_t> {
		size_t operator()(const mmx::addr_t& x) const {
			return std::hash<mmx::hash_t>{}(x);
		}
	};
} // std

#endif /* INCLUDE_MMX_ADDR_T_HPP_ */
