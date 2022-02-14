/*
 * txio_key_t.cpp
 *
 *  Created on: Feb 12, 2022
 *      Author: mad
 */

#include <mmx/txio_key_t.hxx>

#include <vnx/vnx.h>


namespace mmx {

void txio_key_t::vnx_read_fallback(const vnx::Variant& data)
{
	try {
		from_string_value(data.to_string_value());
	} catch(...) {
		// ignore
	}
}


} // mmx
