/*
 * chiapos.h
 *
 *  Created on: Dec 9, 2021
 *      Author: mad
 */

#ifndef MMX_NODE_CHIAPOS_CHIAPOS_H_
#define MMX_NODE_CHIAPOS_CHIAPOS_H_

#include <array>
#include <cstdint>


namespace mmx {
namespace chiapos {

/*
 * Returns quality hash if valid.
 * Throws exception if not valid.
 */
std::array<uint8_t, 32> verify(	const uint8_t ksize,
								const std::array<uint8_t, 32>& plot_id,
								const std::array<uint8_t, 32>& challenge,
								const void* proof_bytes,
								const size_t proof_size);


} // chiapos
} // mmx

#endif /* MMX_NODE_CHIAPOS_CHIAPOS_H_ */
