/*
 * Partial.cpp
 *
 *  Created on: Sep 18, 2024
 *      Author: mad
 */

#include <mmx/Partial.hxx>
#include <mmx/ProofOfStake.hxx>
#include <mmx/ProofOfSpaceNFT.hxx>

#include <vnx/vnx.h>


namespace mmx {

hash_t Partial::calc_hash() const
{
	std::string tmp = get_type_name() + "/";
	tmp += "height:" + std::to_string(height);
	tmp += "contract:" + contract.to_string();
	tmp += "account:" + account.to_string();
	tmp += "pool_url:" + pool_url;
	tmp += "harvester:" + harvester;
	tmp += "difficulty:" + std::to_string(difficulty);
	tmp += "lookup_time_ms:" + std::to_string(lookup_time_ms);
	tmp += "proof:";

	if(proof) {
		tmp += proof->get_type_name() + "/";
		tmp += "score:" + std::to_string(proof->score);
		tmp += "plot_id:" + proof->plot_id.to_string();
		tmp += "challenge:" + proof->challenge.to_string();
		tmp += "farmer_key:" + proof->farmer_key.to_string();
	}
	if(auto nft = std::dynamic_pointer_cast<const ProofOfSpaceNFT>(proof))
	{
		tmp += "ksize:" + std::to_string(nft->ksize);
		tmp += "seed:" + nft->seed.to_string();
		tmp += "proof_xs:";
		for(const auto& x : nft->proof_xs) {
			tmp += std::to_string(x) + ",";
		}
		tmp += "contract:" + nft->contract.to_string();
	}
	return hash_t(tmp);
}


} // mmx
