/*
 * memo_t.hpp
 *
 *  Created on: May 19, 2023
 *      Author: mad
 */

#ifndef INCLUDE_MMX_MEMO_T_HPP_
#define INCLUDE_MMX_MEMO_T_HPP_

#include <mmx/package.hxx>
#include <mmx/bytes_t.hpp>
#include <mmx/hash_t.hpp>

#include <vnx/Input.hpp>
#include <vnx/Output.hpp>
#include <vnx/Visitor.h>

#include <uint256_t.h>


namespace mmx {

class memo_t : public bytes_t<20> {
public:
	typedef bytes_t<20> super_t;

	memo_t() = default;

	memo_t(const hash_t& hash);

	memo_t(const uint256_t& value);

	memo_t(const std::string& str);

	explicit memo_t(const char* str) : memo_t(std::string(str)) {}

	uint256_t to_uint() const;

	std::string to_string() const;

};


inline
std::ostream& operator<<(std::ostream& out, const memo_t& value) {
	return out << value.to_string();
}

} // mmx


namespace vnx {

inline
void read(vnx::TypeInput& in, mmx::memo_t& value, const vnx::TypeCode* type_code, const uint16_t* code) {
	vnx::read(in, (mmx::memo_t::super_t&)value, type_code, code);
}

inline
void write(vnx::TypeOutput& out, const mmx::memo_t& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr) {
	vnx::write(out, (const mmx::memo_t::super_t&)value, type_code, code);
}

inline
void read(std::istream& in, mmx::memo_t& value)
{
	std::string str;
	vnx::read(in, str);
	value = mmx::memo_t(str);
}

inline
void write(std::ostream& out, const mmx::memo_t& value) {
	vnx::write(out, value.to_string());
}

inline
void accept(vnx::Visitor& visitor, const mmx::memo_t& value) {
	vnx::accept(visitor, value.to_string());
}

} // vnx

#endif /* INCLUDE_MMX_MEMO_T_HPP_ */
