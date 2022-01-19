/*
 * write_bytes.h
 *
 *  Created on: Nov 27, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_WRITE_BYTES_H_
#define INCLUDE_MMX_WRITE_BYTES_H_

#include <mmx/hash_t.hpp>
#include <mmx/tx_in_t.hxx>
#include <mmx/tx_out_t.hxx>
#include <mmx/ulong_fraction_t.hxx>

#include <vnx/Buffer.hpp>
#include <vnx/Output.hpp>
#include <vnx/Object.hpp>
#include <vnx/Variant.hpp>
#include <vnx/optional.h>

#include <string>


namespace mmx {

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

template<size_t N>
void write_bytes(vnx::OutputBuffer& out, const bytes_t<N>& value) {
	out.write(value.bytes.data(), value.bytes.size());
}

inline void write_bytes(vnx::OutputBuffer& out, const std::string& value) {
	out.write(value.data(), value.size());
}

inline void write_bytes(vnx::OutputBuffer& out, const std::vector<uint8_t>& value) {
	out.write(value.data(), value.size());
}

inline void write_bytes(vnx::OutputBuffer& out, const vnx::Buffer& value) {
	out.write(value.data(), value.size());
}

inline void write_bytes(vnx::OutputBuffer& out, const vnx::Variant& value) {
	write_bytes(out, value.data);
}

inline void write_bytes(vnx::OutputBuffer& out, const vnx::Object& value)
{
	for(const auto& entry : value.field) {
		write_bytes(out, entry.first);
		write_bytes(out, entry.second);
	}
}

inline void write_bytes(vnx::OutputBuffer& out, const txio_key_t& value)
{
	write_bytes(out, value.txid);
	write_bytes(out, value.index);
}

inline void write_bytes(vnx::OutputBuffer& out, const tx_in_t& value)
{
	write_bytes(out, value.prev);
}

inline void write_bytes(vnx::OutputBuffer& out, const tx_out_t& value)
{
	write_bytes(out, value.address);
	write_bytes(out, value.contract);
	write_bytes(out, value.amount);
}

inline void write_bytes(vnx::OutputBuffer& out, const ulong_fraction_t& value) {
	write_bytes(out, value.value);
	write_bytes(out, value.inverse);
}

template<typename T>
void write_bytes(vnx::OutputBuffer& out, const vnx::optional<T>& value) {
	if(value) {
		write_bytes(out, uint8_t(1));
		write_bytes(out, *value);
	} else {
		write_bytes(out, uint8_t(0));
	}
}

template<typename T, size_t N>
void write_bytes(vnx::OutputBuffer& out, const std::array<T, N>& value) {
	for(const auto& elem : value) {
		write_bytes(out, elem);
	}
}


} // mmx

#endif /* INCLUDE_MMX_WRITE_BYTES_H_ */
