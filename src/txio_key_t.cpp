/*
 * txio_key_t.cpp
 *
 *  Created on: Feb 12, 2022
 *      Author: mad
 */

#include <mmx/txio_key_t.hpp>

#include <vnx/vnx.h>


namespace mmx {

std::string txio_key_t::to_string_value() const {
	return txid.to_string() + ":" + std::to_string(index);
}

void txio_key_t::from_string_value(const std::string& str)
{
	const auto pos = str.find(':');
	if(pos != std::string::npos && str.find_first_of("{}[]\"") == std::string::npos) {
		txid.from_string(str.substr(0, pos));
		vnx::from_string(str.substr(pos + 1), index);
	} else {
		throw std::logic_error("invalid txio_key_t string");
	}
}

void txio_key_t::vnx_read_fallback(const vnx::Variant& data)
{
	try {
		from_string_value(data.to_string_value());
	} catch(...) {
		// ignore
	}
}


} // mmx
