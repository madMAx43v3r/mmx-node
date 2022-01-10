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


namespace mmx {

class Farmer : public FarmerBase {
public:
	Farmer(const std::string& _vnx_name);

protected:
	void init() override;

	void main() override;

	vnx::Hash64 get_mac_addr() const override;

	std::shared_ptr<const FarmInfo> get_farm_info() const override;

	std::shared_ptr<const BlockHeader>
	sign_block(std::shared_ptr<const BlockHeader> block, const uint64_t& reward_amount) const override;

protected:
	void update();

	void handle(std::shared_ptr<const FarmInfo> value);

private:
	std::unordered_map<bls_pubkey_t, skey_t> key_map;

	std::unordered_map<vnx::Hash64, std::shared_ptr<const vnx::Sample>> info_map;

};


} // mmx

#endif /* INCLUDE_MMX_FARMER_H_ */
