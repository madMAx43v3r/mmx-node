/*
 * Wallet.cpp
 *
 *  Created on: Dec 11, 2021
 *      Author: mad
 */

#include <mmx/Wallet.h>

#include <vnx/vnx.h>


namespace mmx {

Wallet::Wallet(const std::string& _vnx_name)
	:	WalletBase(_vnx_name)
{
}

void Wallet::init()
{
	vnx::open_pipe(vnx_name, this, 1000);
}

void Wallet::main()
{
	for(const auto& file_path : key_files)
	{
		if(auto key_file = vnx::read_from_file<KeyFile>(file_path)) {
			bls_wallets.push_back(std::make_shared<BLS_Wallet>(key_file, 11337));
			log(INFO) << "Loaded wallet: " << file_path;
		} else {
			bls_wallets.push_back(nullptr);
			log(ERROR) << "Failed to read wallet: " << file_path;
		}
	}

	Super::main();
}

void Wallet::show_farmer_keys(const uint32_t& index) const
{
	if(auto wallet = bls_wallets.at(index)) {
		if(auto keys = wallet->get_farmer_keys()) {
			std::stringstream ss;
			vnx::PrettyPrinter printer(ss);
			keys->accept(printer);
			log(INFO) << ss.str();
		}
	} else {
		log(ERROR) << "No such wallet";
	}
}

std::shared_ptr<const FarmerKeys> Wallet::get_farmer_keys(const uint32_t& index) const
{
	if(auto wallet = bls_wallets.at(index)) {
		return wallet->get_farmer_keys();
	}
	return nullptr;
}

std::vector<std::shared_ptr<const FarmerKeys>> Wallet::get_all_farmer_keys() const
{
	std::vector<std::shared_ptr<const FarmerKeys>> res;
	for(auto wallet : bls_wallets) {
		if(wallet) {
			res.push_back(wallet->get_farmer_keys());
		}
	}
	return res;
}


} // mmx
