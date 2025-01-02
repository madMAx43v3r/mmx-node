/*
 * ValidatorVote.cpp
 *
 *  Created on: Dec 29, 2024
 *      Author: mad
 */

#include <mmx/ValidatorVote.hxx>


namespace mmx {

bool ValidatorVote::is_valid() const
{
	return content_hash == calc_content_hash()
		&& farmer_sig.verify(farmer_key, hash_t("MMX/validator/vote/" + hash + farmer_key));
}

hash_t ValidatorVote::calc_content_hash() const
{
	return hash_t(hash + farmer_key + farmer_sig);
}


} // mmx
