/*
 * pubkey_t.hpp
 *
 *  Created on: Nov 25, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_PUBKEY_T_HPP_
#define INCLUDE_MMX_PUBKEY_T_HPP_

#include <mmx/hash_t.h>

#include <vnx/Input.hpp>
#include <vnx/Output.hpp>

#include <bls.hpp>


namespace mmx {

struct pubkey_t {

	std::array<uint8_t, 48> bytes = {};

	pubkey_t() = default;

	pubkey_t(const bls::G1Element& key);

	hash_t get_addr() const;

	bls::G1Element to_bls() const;

	bool operator==(const pubkey_t& other) const {
		return bytes == other.bytes;
	}

	bool operator!=(const pubkey_t& other) const {
		return bytes != other.bytes;
	}

};


inline
pubkey_t::pubkey_t(const bls::G1Element& key)
{
	const auto tmp = key.Serialize();
	if(tmp.size() != bytes.size()) {
		throw std::logic_error("key size mismatch");
	}
	::memcpy(bytes.data(), tmp.data(), tmp.size());
}

inline
bls::G1Element pubkey_t::to_bls() const
{
	return bls::G1Element::FromBytes(bls::Bytes(bytes.data(), bytes.size()));
}

inline
hash_t pubkey_t::get_addr() const
{
	return hash_t(bytes.data(), bytes.size());
}

inline
std::ostream& operator<<(std::ostream& out, const pubkey_t& key) {
	return out << "0x" << bls::Util::HexStr(key.bytes.data(), key.bytes.size());
}

} // mmx


namespace vnx {

inline
void read(vnx::TypeInput& in, mmx::pubkey_t& value, const vnx::TypeCode* type_code, const uint16_t* code) {
	vnx::read(in, value.bytes, type_code, code);
}

inline
void write(vnx::TypeOutput& out, const mmx::pubkey_t& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr) {
	vnx::write(out, value.bytes, type_code, code);
}

inline
void read(std::istream& in, mmx::pubkey_t& value) {
	vnx::read(in, value.bytes);
}

inline
void write(std::ostream& out, const mmx::pubkey_t& value) {
	vnx::write(out, value.bytes);
}

inline
void accept(vnx::Visitor& visitor, const mmx::pubkey_t& value) {
	vnx::accept(visitor, value.bytes);
}


} // vnx


namespace std {
	template<> struct hash<mmx::pubkey_t> {
		size_t operator()(const mmx::pubkey_t& x) const {
			return *((const size_t*)x.bytes.data());
		}
	};
} // std

#endif /* INCLUDE_MMX_PUBKEY_T_HPP_ */
