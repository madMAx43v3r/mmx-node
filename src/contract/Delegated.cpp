/*
 * Delegated.cpp
 *
 *  Created on: Dec 10, 2021
 *      Author: mad
 */

#include <mmx/contract/Delegated.hxx>


namespace mmx {
namespace contract {

vnx::bool_t Delegated::validate(std::shared_ptr<const Solution> solution, const hash_t& txid) const
{
	if(owner) {
		return owner->validate(solution, txid);
	}
	return false;
}


} // contract
} // mmx
