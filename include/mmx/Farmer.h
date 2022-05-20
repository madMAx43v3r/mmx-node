/*
 * Farmer.h
 *
 *  Created on: Dec 12, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_FARMER_H_
#define INCLUDE_MMX_FARMER_H_

#include <mmx/FarmerBase.hxx>
#include <mmx/FarmerKeys.hxx>
#include <mmx/WalletAsyncClient.hxx>


namespace mmx {

class Farmer : public FarmerBase {
public:
	Farmer(const std::string& _vnx_name);

protected:
	void init() override;

	void main() override;

	vnx::Hash64 get_mac_addr() const override;

	std::vector<bls_pubkey_t> get_farmer_keys() const override;

	std::shared_ptr<const FarmInfo> get_farm_info() const override;

	bls_signature_t sign_proof(std::shared_ptr<const ProofOfSpace> proof) const override;

	std::shared_ptr<const BlockHeader>
	sign_block(std::shared_ptr<const BlockHeader> block, const uint64_t& reward_amount) const override;

	void handle(std::shared_ptr<const FarmInfo> value) override;

private:
	void update();

	skey_t find_skey(const bls_pubkey_t& pubkey) const;

private:
	std::shared_ptr<vnx::Pipe> pipe;
	std::shared_ptr<WalletAsyncClient> wallet;

	std::unordered_map<bls_pubkey_t, skey_t> key_map;
	std::unordered_map<vnx::Hash64, std::shared_ptr<const vnx::Sample>> info_map;

};


} // mmx

#endif /* INCLUDE_MMX_FARMER_H_ */
