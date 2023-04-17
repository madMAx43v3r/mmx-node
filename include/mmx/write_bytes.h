/*
 * write_bytes.h
 *
 *  Created on: Nov 27, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_WRITE_BYTES_H_
#define INCLUDE_MMX_WRITE_BYTES_H_

#include <mmx/hash_t.hpp>
#include <mmx/uint128.hpp>
#include <mmx/txin_t.hxx>
#include <mmx/txout_t.hxx>
#include <mmx/ulong_fraction_t.hxx>
#include <mmx/time_segment_t.hxx>
#include <mmx/contract/method_t.hxx>

#include <vnx/Buffer.hpp>
#include <vnx/Output.hpp>
#include <vnx/Object.hpp>
#include <vnx/Variant.hpp>
#include <vnx/optional.h>


namespace mmx {

template<typename T>
void write_bytes(vnx::OutputBuffer& out, const vnx::optional<T>& value);

template<typename T, size_t N>
void write_bytes(vnx::OutputBuffer& out, const std::array<T, N>& value);

template<typename T>
void write_bytes(vnx::OutputBuffer& out, const std::vector<T>& value);

template<typename K, typename V>
void write_bytes(vnx::OutputBuffer& out, const std::map<K, V>& value);

template<typename T>
void write_field(vnx::OutputBuffer& out, const std::string& name, const T& value);

inline void write_bytes(vnx::OutputBuffer& out, const int64_t& value) {
	out.write(&value, sizeof(value));
}

inline void write_bytes(vnx::OutputBuffer& out, const int32_t& value) {
	write_bytes(out, int64_t(value));
}

inline void write_bytes(vnx::OutputBuffer& out, const int16_t& value) {
	write_bytes(out, int64_t(value));
}

inline void write_bytes(vnx::OutputBuffer& out, const int8_t& value) {
	write_bytes(out, int64_t(value));
}

inline void write_bytes(vnx::OutputBuffer& out, const uint64_t& value) {
	out.write(&value, sizeof(value));
}

inline void write_bytes(vnx::OutputBuffer& out, const uint32_t& value) {
	write_bytes(out, uint64_t(value));
}

inline void write_bytes(vnx::OutputBuffer& out, const uint16_t& value) {
	write_bytes(out, uint64_t(value));
}

inline void write_bytes(vnx::OutputBuffer& out, const uint8_t& value) {
	write_bytes(out, uint64_t(value));
}

inline void write_bytes(vnx::OutputBuffer& out, const uint128& value) {
	write_bytes(out, value.lower());
	write_bytes(out, value.upper());
}

inline void write_bytes_cstr(vnx::OutputBuffer& out, const char* str) {
	out.write(str, ::strlen(str));
}

template<size_t N>
void write_bytes(vnx::OutputBuffer& out, const bytes_t<N>& value) {
	write_bytes_cstr(out, "bytes<>");
	write_bytes(out, uint16_t(value.size()));
	out.write(value.data(), value.size());
}

inline void write_bytes(vnx::OutputBuffer& out, const std::string& value) {
	write_bytes_cstr(out, "string<>");
	write_bytes(out, uint32_t(value.size()));
	out.write(value.data(), value.size());
}

inline void write_bytes(vnx::OutputBuffer& out, const std::vector<uint8_t>& value) {
	write_bytes_cstr(out, "vector<>");
	write_bytes(out, uint32_t(value.size()));
	out.write(value.data(), value.size());
}

inline void write_bytes(vnx::OutputBuffer& out, const vnx::Buffer& value) {
	write_bytes_cstr(out, "vector<>");
	write_bytes(out, uint32_t(value.size()));
	out.write(value.data(), value.size());
}

inline void write_bytes(vnx::OutputBuffer& out, const vnx::Variant& value) {
	write_bytes_cstr(out, "variant<>");
	if(value.empty()) {
		write_bytes(out, vnx::Variant(nullptr));
	} else {
		write_bytes(out, value.data);
	}
}

inline void write_bytes(vnx::OutputBuffer& out, const vnx::Object& value) {
	write_bytes_cstr(out, "object<>");
	write_bytes(out, value.field);
}

inline void write_bytes(vnx::OutputBuffer& out, const txio_t& value)
{
	write_bytes_cstr(out, "txio_t<>");
	write_bytes(out, value.address);
	write_bytes(out, value.contract);
	write_bytes(out, value.amount);
}

inline void write_bytes(vnx::OutputBuffer& out, const txin_t& value) {
	write_bytes(out, (const txio_t&)value);
}

inline void write_bytes(vnx::OutputBuffer& out, const txout_t& value) {
	write_bytes(out, (const txio_t&)value);
}

inline void write_bytes(vnx::OutputBuffer& out, const ulong_fraction_t& value)
{
	write_bytes_cstr(out, "ulong_fraction_t<>");
	write_bytes(out, value.value);
	write_bytes(out, value.inverse);
}

inline void write_bytes(vnx::OutputBuffer& out, const time_segment_t& value)
{
	write_bytes_cstr(out, "time_segment_t<>");
	write_bytes(out, value.num_iters);
	write_bytes(out, value.output);
}

inline void write_bytes(vnx::OutputBuffer& out, const contract::method_t& value)
{
	write_bytes(out, value.get_type_hash());
	write_field(out, "name", value.name);
	write_field(out, "info", value.info);
	write_field(out, "is_const", value.is_const);
	write_field(out, "is_public", value.is_public);
	write_field(out, "is_payable", value.is_payable);
	write_field(out, "entry_point", value.entry_point);
	write_field(out, "args", value.args);
}

template<typename T>
void write_bytes(vnx::OutputBuffer& out, const vnx::optional<T>& value) {
	write_bytes_cstr(out, "optional<>");
	if(value) {
		write_bytes(out, uint8_t(1));
		write_bytes(out, *value);
	} else {
		write_bytes(out, uint8_t(0));
	}
}

template<typename T, typename S>
void write_bytes(vnx::OutputBuffer& out, const std::pair<T, S>& value) {
	write_bytes_cstr(out, "pair<>");
	write_bytes(out, value.first);
	write_bytes(out, value.second);
}

template<typename T, size_t N>
void write_bytes(vnx::OutputBuffer& out, const std::array<T, N>& value) {
	write_bytes_cstr(out, "vector<>");
	write_bytes(out, uint32_t(value.size()));
	for(const auto& elem : value) {
		write_bytes(out, elem);
	}
}

template<typename T>
void write_bytes(vnx::OutputBuffer& out, const std::vector<T>& value) {
	write_bytes_cstr(out, "vector<>");
	write_bytes(out, uint32_t(value.size()));
	for(const auto& elem : value) {
		write_bytes(out, elem);
	}
}

template<typename K, typename V>
void write_bytes(vnx::OutputBuffer& out, const std::map<K, V>& value) {
	write_bytes_cstr(out, "map<>");
	write_bytes(out, uint32_t(value.size()));
	for(const auto& entry : value) {
		write_bytes_cstr(out, "pair<>");
		write_bytes(out, entry.first);
		write_bytes(out, entry.second);
	}
}

inline void write_field(vnx::OutputBuffer& out, const std::string& name) {
	write_bytes_cstr(out, "field<>");
	write_bytes(out, name);
}

template<typename T>
void write_field(vnx::OutputBuffer& out, const std::string& name, const T& value) {
	write_field(out, name);
	write_bytes(out, value);
}


} // mmx

#endif /* INCLUDE_MMX_WRITE_BYTES_H_ */
