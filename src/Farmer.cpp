/*
 * Farmer.cpp
 *
 *  Created on: Dec 12, 2021
 *      Author: mad
 */

#include <mmx/Farmer.h>
#include <mmx/WalletClient.hxx>


namespace mmx {

Farmer::Farmer(const std::string& _vnx_name)
	:	FarmerBase(_vnx_name)
{
}

void Farmer::init()
{
	vnx::open_pipe(vnx_name, this, 1000);
	vnx::open_pipe(vnx_get_id(), this, 1000);
}

void Farmer::main()
{
	WalletClient wallet(wallet_server);

	for(auto keys : wallet.get_all_farmer_keys()) {
		if(keys) {
			key_map[keys->farmer_public_key] = keys->farmer_private_key;
			log(INFO) << "Got farmer key: " << keys->farmer_public_key;
		}
	}

	Super::main();
}

vnx::Hash64 Farmer::get_mac_addr() const
{
	return vnx_get_id();
}

bls_signature_t Farmer::sign_block(std::shared_ptr<const BlockHeader> block) const
{
	if(auto proof = block->proof) {
		auto iter = key_map.find(proof->farmer_key);
		if(iter != key_map.end()) {
			return bls_signature_t::sign(iter->second, block->hash);
		}
		throw std::logic_error("unknown farmer key");
	}
	throw std::logic_error("invalid proof");
}


} // mmx
