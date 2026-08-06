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
#include <mmx/compile_flags_t.hxx>
#include <mmx/contract/method_t.hxx>

#include <vnx/Util.hpp>
#include <vnx/Buffer.hpp>
#include <vnx/Output.hpp>
#include <vnx/Object.hpp>
#include <vnx/Variant.hpp>
#include <vnx/optional.h>


namespace mmx {

class WriteBytes : public vnx::OutputBuffer {
public:
	const int version;

	WriteBytes(vnx::OutputStream* stream, const int version) : vnx::OutputBuffer(stream), version(version) {}
};


template<typename T>
void write_bytes(WriteBytes& out, const vnx::optional<T>& value);

template<typename T, size_t N>
void write_bytes(WriteBytes& out, const std::array<T, N>& value);

template<typename T>
void write_bytes(WriteBytes& out, const std::vector<T>& value);

template<typename T>
void write_bytes(WriteBytes& out, const std::set<T>& value);

template<typename K, typename V>
void write_bytes(WriteBytes& out, const std::map<K, V>& value);

void write_bytes(WriteBytes& out, const vnx::Object& value);

template<typename T>
void write_field(WriteBytes& out, const std::string& name, const T& value);

inline void write_bytes(WriteBytes& out, const bool& value) {
	const uint8_t tmp = value ? 1 : 0;
	out.write(&tmp, sizeof(tmp));
}

inline void write_bytes(WriteBytes& out, const int64_t& value) {
	const auto tmp = vnx::to_little_endian(value);
	out.write(&tmp, sizeof(tmp));
}

inline void write_bytes(WriteBytes& out, const int32_t& value) {
	write_bytes(out, int64_t(value));
}

inline void write_bytes(WriteBytes& out, const int16_t& value) {
	write_bytes(out, int64_t(value));
}

inline void write_bytes(WriteBytes& out, const int8_t& value) {
	write_bytes(out, int64_t(value));
}

inline void write_bytes(WriteBytes& out, const uint64_t& value) {
	const auto tmp = vnx::to_little_endian(value);
	out.write(&tmp, sizeof(tmp));
}

inline void write_bytes(WriteBytes& out, const uint32_t& value) {
	write_bytes(out, uint64_t(value));
}

inline void write_bytes(WriteBytes& out, const uint16_t& value) {
	write_bytes(out, uint64_t(value));
}

inline void write_bytes(WriteBytes& out, const uint8_t& value) {
	write_bytes(out, uint64_t(value));
}

inline void write_bytes(WriteBytes& out, const uint128& value) {
	write_bytes(out, value.lower());
	write_bytes(out, value.upper());
}

inline void write_bytes_cstr(WriteBytes& out, const char* str) {
	out.write(str, ::strlen(str));
}

inline void write_bytes(WriteBytes& out, const std::string& value)
{
	write_bytes_cstr(out, "string<>");
	write_bytes(out, uint64_t(value.size()));
	out.write(value.data(), value.size());
}

template<size_t N>
void write_bytes(WriteBytes& out, const bytes_t<N>& value)
{
	write_bytes_cstr(out, "bytes<>");
	write_bytes(out, uint64_t(value.size()));
	out.write(value.data(), value.size());
}

inline void write_bytes(WriteBytes& out, const std::vector<uint8_t>& value)
{
	write_bytes_cstr(out, "bytes<>");
	write_bytes(out, uint64_t(value.size()));
	out.write(value.data(), value.size());
}

inline void write_bytes(WriteBytes& out, const vnx::Buffer& value)
{
	write_bytes_cstr(out, "bytes<>");
	write_bytes(out, uint64_t(value.size()));
	out.write(value.data(), value.size());
}

inline void write_bytes(WriteBytes& out, const vnx::Variant& value)
{
	if(value.is_null()) {
		write_bytes_cstr(out, "NULL");
	} else if(value.is_bool()) {
		if(out.version >= 1) {
			write_bytes_cstr(out, "<bool>");
		}
		write_bytes(out, value.to<bool>());
	} else if(value.is_ulong()) {
		if(out.version >= 1) {
			write_bytes_cstr(out, "<u64>");
		}
		write_bytes(out, value.to<uint64_t>());
	} else if(value.is_long()) {
		if(out.version >= 1) {
			write_bytes_cstr(out, "<i64>");
		}
		write_bytes(out, value.to<int64_t>());
	} else if(value.is_string()) {
		write_bytes(out, value.to<std::string>());
	} else if(value.is_array()) {
		write_bytes(out, value.to<std::vector<vnx::Variant>>());
	} else if(value.is_object()) {
		write_bytes(out, value.to_object());
	} else {
		write_bytes(out, value.data);
	}
}

inline void write_bytes(WriteBytes& out, const vnx::Object& value)
{
	write_bytes_cstr(out, "object<>");
	write_bytes(out, value.field);
}

inline void write_bytes_ex(WriteBytes& out, const txio_t& value)
{
	write_bytes(out, value.address);
	write_bytes(out, value.contract);
	write_bytes(out, value.amount);
	write_bytes(out, value.memo);
}

inline void write_bytes_ex(WriteBytes& out, const txin_t& value, const bool full_hash)
{
	write_bytes_cstr(out, "txin_t<>");
	write_bytes_ex(out, static_cast<const txio_t&>(value));
	if(full_hash) {
		write_bytes(out, value.solution);
		write_bytes(out, value.flags);
	}
}

inline void write_bytes(WriteBytes& out, const txin_t& value)
{
	write_bytes_ex(out, value, false);
}

inline void write_bytes(WriteBytes& out, const txout_t& value)
{
	write_bytes_cstr(out, "txout_t<>");
	write_bytes_ex(out, value);
}

inline void write_bytes(WriteBytes& out, const ulong_fraction_t& value)
{
	write_bytes_cstr(out, "ulong_fraction_t<>");
	write_bytes(out, value.value);
	write_bytes(out, value.inverse);
}

inline void write_bytes(WriteBytes& out, const compile_flags_t& value)
{
	write_bytes_cstr(out, "compile_flags_t<>");
	write_field(out, "verbose", value.verbose);
	write_field(out, "opt_level", value.opt_level);
	write_field(out, "catch_overflow", value.catch_overflow);
}

inline void write_bytes(WriteBytes& out, const contract::method_t& value)
{
	write_bytes(out, value.get_type_hash());
	write_field(out, "name", value.name);
	write_field(out, "info", value.info);
	write_field(out, "is_init", value.is_init);
	write_field(out, "is_const", value.is_const);
	write_field(out, "is_public", value.is_public);
	write_field(out, "is_payable", value.is_payable);
	write_field(out, "entry_point", value.entry_point);
	write_field(out, "args", value.args);
}

template<typename T>
void write_bytes(WriteBytes& out, const vnx::optional<T>& value) {
	write_bytes_cstr(out, "optional<>");
	if(value) {
		write_bytes(out, true);
		write_bytes(out, *value);
	} else {
		write_bytes(out, false);
	}
}

template<typename T, typename S>
void write_bytes(WriteBytes& out, const std::pair<T, S>& value) {
	write_bytes_cstr(out, "pair<>");
	write_bytes(out, value.first);
	write_bytes(out, value.second);
}

template<typename T, size_t N>
void write_bytes(WriteBytes& out, const std::array<T, N>& value) {
	write_bytes_cstr(out, "vector<>");
	write_bytes(out, uint64_t(value.size()));
	for(const auto& elem : value) {
		write_bytes(out, elem);
	}
}

template<typename T>
void write_bytes(WriteBytes& out, const std::vector<T>& value) {
	write_bytes_cstr(out, "vector<>");
	write_bytes(out, uint64_t(value.size()));
	for(const auto& elem : value) {
		write_bytes(out, elem);
	}
}

template<typename T>
void write_bytes_ex(WriteBytes& out, const std::vector<T>& value, const bool full_hash) {
	write_bytes_cstr(out, "vector<>");
	write_bytes(out, uint64_t(value.size()));
	for(const auto& elem : value) {
		write_bytes_ex(out, elem, full_hash);
	}
}

template<typename T>
void write_bytes(WriteBytes& out, const std::set<T>& value) {
	write_bytes_cstr(out, "vector<>");
	write_bytes(out, uint64_t(value.size()));
	for(const auto& elem : value) {
		write_bytes(out, elem);
	}
}

template<typename K, typename V>
void write_bytes(WriteBytes& out, const std::map<K, V>& value) {
	write_bytes_cstr(out, "vector<>");
	write_bytes(out, uint64_t(value.size()));
	for(const auto& entry : value) {
		write_bytes(out, entry);
	}
}

inline void write_field(WriteBytes& out, const std::string& name) {
	write_bytes_cstr(out, "field<>");
	write_bytes(out, name);
}

template<typename T>
void write_field(WriteBytes& out, const std::string& name, const T& value) {
	write_field(out, name);
	write_bytes(out, value);
}

template<typename T>
void write_field_ex(WriteBytes& out, const std::string& name, const T& value, const bool full_hash) {
	write_field(out, name);
	write_bytes_ex(out, value, full_hash);
}


} // mmx

#endif /* INCLUDE_MMX_WRITE_BYTES_H_ */
