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

#include <vnx/Buffer.hpp>
#include <vnx/Output.hpp>

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


} // mmx

#endif /* INCLUDE_MMX_WRITE_BYTES_H_ */
