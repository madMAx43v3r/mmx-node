/*
 * skey_t.hpp
 *
 *  Created on: Nov 25, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_SKEY_T_HPP_
#define INCLUDE_MMX_SKEY_T_HPP_

#include <array>
#include <bls.hpp>

#include <vnx/Input.hpp>
#include <vnx/Output.hpp>


namespace mmx {

struct skey_t {

	std::array<uint8_t, 32> bytes = {};

	skey_t() = default;

	skey_t(const bls::PrivateKey& key);

	bls::PrivateKey to_bls() const;

	bool operator==(const skey_t& other) const {
		return bytes == other.bytes;
	}

	bool operator!=(const skey_t& other) const {
		return bytes != other.bytes;
	}

};


inline
skey_t::skey_t(const bls::PrivateKey& key)
{
	key.Serialize(bytes.data());
}

inline
bls::PrivateKey skey_t::to_bls() const
{
	return bls::PrivateKey::FromBytes(bls::Bytes(bytes.data(), bytes.size()));
}

inline
std::ostream& operator<<(std::ostream& out, const skey_t& key) {
	return out << "0x" << bls::Util::HexStr(key.bytes.data(), key.bytes.size());
}

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
	vnx::read(in, value.bytes);
}

inline
void write(std::ostream& out, const mmx::skey_t& value) {
	vnx::write(out, value.bytes);
}

inline
void accept(vnx::Visitor& visitor, const mmx::skey_t& value) {
	vnx::accept(visitor, value.bytes);
}

} // vnx

#endif /* INCLUDE_MMX_SKEY_T_HPP_ */
