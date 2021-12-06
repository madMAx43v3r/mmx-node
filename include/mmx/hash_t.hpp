/*
 * hash_t.hpp
 *
 *  Created on: Nov 25, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_HASH_T_HPP_
#define INCLUDE_MMX_HASH_T_HPP_

#include <mmx/bytes_t.hpp>


namespace mmx {

struct hash_t : bytes_t<32> {

	hash_t() = default;

	explicit hash_t(const std::string& data);

	explicit hash_t(const std::vector<uint8_t>& data);

	explicit hash_t(const std::array<uint8_t, 32>& data);

	explicit hash_t(const void* data, const size_t num_bytes);

};


inline
hash_t::hash_t(const std::string& data)
	:	hash_t(data.data(), data.size())
{
}

inline
hash_t::hash_t(const std::vector<uint8_t>& data)
	:	hash_t(data.data(), data.size())
{
}

inline
hash_t::hash_t(const std::array<uint8_t, 32>& data)
	:	hash_t(data.data(), data.size())
{
}

inline
hash_t::hash_t(const void* data, const size_t num_bytes)
{
	bls::Util::Hash256(bytes.data(), (const uint8_t*)data, num_bytes);
}


} // mmx


namespace vnx {

inline
void read(vnx::TypeInput& in, mmx::hash_t& value, const vnx::TypeCode* type_code, const uint16_t* code) {
	vnx::read(in, value.bytes, type_code, code);
}

inline
void write(vnx::TypeOutput& out, const mmx::hash_t& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr) {
	vnx::write(out, value.bytes, type_code, code);
}

inline
void read(std::istream& in, mmx::hash_t& value) {
	std::string tmp;
	vnx::read(in, tmp);
	value.from_string(tmp);
}

inline
void write(std::ostream& out, const mmx::hash_t& value) {
	vnx::write(out, value.to_string());
}

inline
void accept(vnx::Visitor& visitor, const mmx::hash_t& value) {
	vnx::accept(visitor, value.bytes);
}

} // vnx


namespace std {
	template<>
	struct hash<typename mmx::hash_t> {
		size_t operator()(const mmx::hash_t& x) const {
			return std::hash<mmx::bytes_t<32>>{}(x);
		}
	};
} // std

#endif /* INCLUDE_MMX_HASH_T_HPP_ */
