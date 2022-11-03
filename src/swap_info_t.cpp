/*
 * swap_info_t.cpp
 *
 *  Created on: Nov 3, 2022
 *      Author: mad
 */

#include <mmx/swap_info_t.hxx>


namespace mmx {

vnx::float64_t swap_info_t::get_price() const
{
	return balance[1].to_double() / balance[0].to_double();
}


} // mmx
