/*
 * Farmer.h
 *
 *  Created on: Dec 12, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_FARMER_H_
#define INCLUDE_MMX_FARMER_H_

#include <mmx/FarmerBase.hxx>
#include <mmx/WalletAsyncClient.hxx>


namespace mmx {

class Farmer : public FarmerBase {
public:
	Farmer(const std::string& _vnx_name);

protected:
	void init() override;

	void main() override;

	vnx::Hash64 get_mac_addr() const override;

	std::vector<pubkey_t> get_farmer_keys() const override;

	std::shared_ptr<const FarmInfo> get_farm_info() const override;

	std::shared_ptr<const BlockHeader> sign_block(std::shared_ptr<const BlockHeader> block) const override;

	void handle(std::shared_ptr<const FarmInfo> value) override;

	void handle(std::shared_ptr<const ProofResponse> value) override;

private:
	void update();

	skey_t get_skey(const pubkey_t& pubkey) const;

private:
	std::shared_ptr<vnx::Pipe> pipe;
	std::shared_ptr<const ChainParams> params;
	std::shared_ptr<WalletAsyncClient> wallet;

	mutable std::unordered_map<pubkey_t, skey_t> key_map;
	std::map<hash_t, std::shared_ptr<const vnx::Sample>> info_map;

};


} // mmx

#endif /* INCLUDE_MMX_FARMER_H_ */
