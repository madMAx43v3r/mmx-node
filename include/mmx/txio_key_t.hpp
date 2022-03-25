/*
 * txio_key_t.hpp
 *
 *  Created on: Nov 30, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_TXIO_KEY_T_HPP_
#define INCLUDE_MMX_TXIO_KEY_T_HPP_

#include <mmx/txio_key_t.hxx>


namespace mmx {

inline
txio_key_t txio_key_t::create_ex(const hash_t& txid, const uint32_t& index)
{
	txio_key_t key;
	key.txid = txid;
	key.index = index;
	return key;
}

inline
bool operator==(const txio_key_t& lhs, const txio_key_t& rhs) {
	return lhs.txid == rhs.txid && lhs.index == rhs.index;
}

inline
bool operator<(const txio_key_t& lhs, const txio_key_t& rhs) {
	if(lhs.txid == rhs.txid) {
		return lhs.index < rhs.index;
	}
	return lhs.txid < rhs.txid;
}

} // mmx


namespace std {
	template<>
	struct hash<mmx::txio_key_t> {
		size_t operator()(const mmx::txio_key_t& x) const {
			return std::hash<mmx::hash_t>{}(x.txid) ^ x.index;
		}
	};
} // std

#endif /* INCLUDE_MMX_TXIO_KEY_T_HPP_ */
