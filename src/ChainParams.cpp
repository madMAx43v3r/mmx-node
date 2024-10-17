/*
 * ChainParams.cpp
 *
 *  Created on: Sep 28, 2024
 *      Author: mad
 */

#include <mmx/ChainParams.hxx>


namespace mmx {

vnx::float64_t ChainParams::get_block_time() const
{
	return block_interval_ms * 1e-3;
}


} // mmx
