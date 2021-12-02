/*
 * utxo_key_t.hpp
 *
 *  Created on: Nov 30, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_UTXO_KEY_T_HPP_
#define INCLUDE_MMX_UTXO_KEY_T_HPP_

#include <mmx/utxo_key_t.hxx>


namespace mmx {

inline
bool operator==(const utxo_key_t& lhs, const utxo_key_t& rhs) {
	return lhs.txid == rhs.txid && lhs.index == rhs.index;
}

} // mmx


namespace std {
	template<>
	struct hash<mmx::utxo_key_t> {
		size_t operator()(const mmx::utxo_key_t& x) const {
			return std::hash<mmx::hash_t>{}(x.txid) xor x.index;
		}
	};
} // std

#endif /* INCLUDE_MMX_UTXO_KEY_T_HPP_ */
