/*
 * bytes_t.hpp
 *
 *  Created on: Dec 2, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_BYTES_T_HPP_
#define INCLUDE_MMX_BYTES_T_HPP_

#include <array>
#include <vector>
#include <string>
#include <cstdint>

#include <bls.hpp>
#include <vnx/Input.hpp>
#include <vnx/Output.hpp>


namespace mmx {

template<int N>
struct bytes_t {

	std::array<uint8_t, N> bytes = {};

	bytes_t() = default;

	explicit bytes_t(const std::vector<uint8_t>& data);

	explicit bytes_t(const std::array<uint8_t, N>& data);

	explicit bytes_t(const void* data, const size_t num_bytes);

	const uint8_t* data() const;

	bool is_zero() const;

	std::string to_string() const;

	void from_string(const std::string& str);

	bytes_t& operator=(const bytes_t&) = default;

	bool operator==(const bytes_t& other) const {
		return bytes == other.bytes;
	}

	bool operator!=(const bytes_t& other) const {
		return bytes != other.bytes;
	}

	bool operator!() const {
		return is_zero();
	}

};


template<int N>
bytes_t<N>::bytes_t(const void* data, const size_t num_bytes)
{
	if(num_bytes > bytes.size()) {
		throw std::logic_error("input size overflow");
	}
	::memcpy(bytes.data(), data, num_bytes);
}

template<int N>
bytes_t<N>::bytes_t(const std::vector<uint8_t>& data)
	:	bytes_t(data.data(), data.size())
{
}

template<int N>
bytes_t<N>::bytes_t(const std::array<uint8_t, N>& data)
	:	bytes_t(data.data(), data.size())
{
}

template<int N>
const uint8_t* bytes_t<N>::data() const {
	return bytes.data();
}

template<int N>
bool bytes_t<N>::is_zero() const {
	return *this == bytes_t();
}

template<int N>
std::string bytes_t<N>::to_string() const {
	return bls::Util::HexStr(bytes.data(), bytes.size());
}

template<int N>
void bytes_t<N>::from_string(const std::string& str)
{
	const auto tmp = bls::Util::HexToBytes(str);
	if(tmp.size() != bytes.size()) {
		throw std::logic_error("input size mismatch");
	}
	::memcpy(bytes.data(), tmp.data(), tmp.size());
}

template<int N>
std::ostream& operator<<(std::ostream& out, const bytes_t<N>& hash) {
	return out << "0x" << hash.to_string();
}

} // mmx


namespace vnx {

template<int N>
void read(vnx::TypeInput& in, mmx::bytes_t<N>& value, const vnx::TypeCode* type_code, const uint16_t* code) {
	vnx::read(in, value.bytes, type_code, code);
}

template<int N>
void write(vnx::TypeOutput& out, const mmx::bytes_t<N>& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr) {
	vnx::write(out, value.bytes, type_code, code);
}

template<int N>
void read(std::istream& in, mmx::bytes_t<N>& value) {
	std::string tmp;
	vnx::read(in, tmp);
	value.from_string(tmp);
}

template<int N>
void write(std::ostream& out, const mmx::bytes_t<N>& value) {
	vnx::write(out, value.to_string());
}

template<int N>
void accept(vnx::Visitor& visitor, const mmx::bytes_t<N>& value) {
	vnx::accept(visitor, value.bytes);
}

} // vnx


namespace std {
	template<int N>
	struct hash<typename mmx::bytes_t<N>> {
		size_t operator()(const mmx::bytes_t<N>& x) const {
			return *((const size_t*)x.bytes.data());
		}
	};
} // std

#endif /* INCLUDE_MMX_BYTES_T_HPP_ */
