/*
 * skey_t.hpp
 *
 *  Created on: Nov 25, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_SKEY_T_HPP_
#define INCLUDE_MMX_SKEY_T_HPP_

#include <mmx/bytes_t.hpp>


namespace mmx {

struct skey_t : bytes_t<32> {

};

} //mmx


namespace vnx {

inline
void read(vnx::TypeInput& in, mmx::skey_t& value, const vnx::TypeCode* type_code, const uint16_t* code) {
	vnx::read(in, value.bytes, type_code, code);
}

inline
void write(vnx::TypeOutput& out, const mmx::skey_t& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr) {
	vnx::write(out, value.bytes, type_code, code);
}

inline
void read(std::istream& in, mmx::skey_t& value) {
	std::string tmp;
	vnx::read(in, tmp);
	value.from_string(tmp);
}

inline
void write(std::ostream& out, const mmx::skey_t& value) {
	vnx::write(out, value.to_string());
}

inline
void accept(vnx::Visitor& visitor, const mmx::skey_t& value) {
	vnx::accept(visitor, value.bytes);
}

} // vnx

#endif /* INCLUDE_MMX_SKEY_T_HPP_ */
