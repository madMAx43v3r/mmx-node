/*
 * PubKey.cpp
 *
 *  Created on: Dec 10, 2021
 *      Author: mad
 */

#include <mmx/contract/PubKey.hxx>
#include <mmx/solution/PubKey.hxx>


namespace mmx {
namespace contract {

vnx::bool_t PubKey::validate(std::shared_ptr<const Solution> solution, const hash_t& txid) const
{
	if(auto sol = std::dynamic_pointer_cast<const solution::PubKey>(solution))
	{
		if(sol->pubkey.get_addr() != pubkey_hash) {
			return false;
		}
		return sol->signature.verify(sol->pubkey, txid);
	}
	return false;
}


} // contract
} // mmx
