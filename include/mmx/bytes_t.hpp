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
#include <vnx/Variant.hpp>
#include <vnx/Hash64.hpp>


namespace mmx {

template<size_t N>
struct bytes_t {

	std::array<uint8_t, N> bytes = {};

	bytes_t() = default;

	bytes_t(const std::vector<uint8_t>& data);

	bytes_t(const std::array<uint8_t, N>& data);

	bytes_t(const void* data, const size_t num_bytes);

	uint8_t* data();

	const uint8_t* data() const;

	size_t size() const;

	bool is_zero() const;

	vnx::Hash64 crc64() const;

	std::string to_string() const;

	std::vector<uint8_t> to_vector() const;

	void from_string(const std::string& str);

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


template<size_t N>
bytes_t<N>::bytes_t(const void* data, const size_t num_bytes)
{
	if(num_bytes > bytes.size()) {
		throw std::logic_error("input size overflow");
	}
	::memcpy(bytes.data(), data, num_bytes);
}

template<size_t N>
bytes_t<N>::bytes_t(const std::vector<uint8_t>& data)
	:	bytes_t(data.data(), data.size())
{
}

template<size_t N>
bytes_t<N>::bytes_t(const std::array<uint8_t, N>& data)
	:	bytes_t(data.data(), data.size())
{
}

template<size_t N>
uint8_t* bytes_t<N>::data() {
	return bytes.data();
}

template<size_t N>
const uint8_t* bytes_t<N>::data() const {
	return bytes.data();
}

template<size_t N>
size_t bytes_t<N>::size() const
{
	return N;
}

template<size_t N>
bool bytes_t<N>::is_zero() const {
	return *this == bytes_t();
}

template<size_t N>
std::string bytes_t<N>::to_string() const {
	return bls::Util::HexStr(bytes.data(), bytes.size());
}

template<size_t N>
vnx::Hash64 bytes_t<N>::crc64() const {
	return vnx::Hash64(bytes.data(), bytes.size());
}

template<size_t N>
std::vector<uint8_t> bytes_t<N>::to_vector() const {
	std::vector<uint8_t> res(N);
	::memcpy(res.data(), bytes.data(), N);
	return res;
}

template<size_t N>
void bytes_t<N>::from_string(const std::string& str)
{
	const auto tmp = bls::Util::HexToBytes(str);
	if(tmp.size() != bytes.size()) {
		throw std::logic_error("input size mismatch");
	}
	::memcpy(bytes.data(), tmp.data(), tmp.size());
}

template<size_t N>
std::ostream& operator<<(std::ostream& out, const bytes_t<N>& bytes) {
	return out << bytes.to_string();
}

template<size_t N, size_t M>
std::vector<uint8_t> operator+(const bytes_t<N>& lhs, const bytes_t<M>& rhs) {
	std::vector<uint8_t> res(N + M);
	::memcpy(res.data(), lhs.bytes.data(), N);
	::memcpy(res.data() + N, rhs.bytes.data(), M);
	return res;
}

} // mmx


namespace vnx {

template<size_t N>
void read(vnx::TypeInput& in, mmx::bytes_t<N>& value, const vnx::TypeCode* type_code, const uint16_t* code)
{
	switch(code[0]) {
		case CODE_STRING:
		case CODE_ALT_STRING: {
			std::string tmp;
			vnx::read(in, tmp, type_code, code);
			try {
				value.from_string(tmp);
			} catch(...) {
				value = mmx::bytes_t<N>();
			}
			break;
		}
		case CODE_UINT64:
		case CODE_ALT_UINT64: {
			uint64_t tmp = 0;
			vnx::read(in, tmp, type_code, code);
			::memcpy(value.data(), &tmp, std::min(N, sizeof(tmp)));
			break;
		}
		case CODE_DYNAMIC:
		case CODE_ALT_DYNAMIC: {
			vnx::Variant tmp;
			vnx::read(in, tmp, type_code, code);
			if(tmp.is_string()) {
				std::string str;
				tmp.to(str);
				try {
					value.from_string(str);
				} catch(...) {
					value = mmx::bytes_t<N>();
				}
			} else {
				std::array<uint8_t, N> array;
				tmp.to(array);
				value = array;
			}
			break;
		}
		default:
			vnx::read(in, value.bytes, type_code, code);
	}
}

template<size_t N>
void write(vnx::TypeOutput& out, const mmx::bytes_t<N>& value, const vnx::TypeCode* type_code = nullptr, const uint16_t* code = nullptr) {
	vnx::write(out, value.bytes, type_code, code);
}

template<size_t N>
void read(std::istream& in, mmx::bytes_t<N>& value) {
	std::string tmp;
	vnx::read(in, tmp);
	value.from_string(tmp);
}

template<size_t N>
void write(std::ostream& out, const mmx::bytes_t<N>& value) {
	vnx::write(out, value.to_string());
}

template<size_t N>
void accept(vnx::Visitor& visitor, const mmx::bytes_t<N>& value) {
	vnx::accept(visitor, value.to_string());
}

} // vnx


namespace std {
	template<size_t N>
	struct hash<typename mmx::bytes_t<N>> {
		size_t operator()(const mmx::bytes_t<N>& x) const {
			return *((const size_t*)x.bytes.data());
		}
	};
} // std

#endif /* INCLUDE_MMX_BYTES_T_HPP_ */
