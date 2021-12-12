/*
 * ECDSA_Wallet.h
 *
 *  Created on: Dec 11, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_ECDSA_WALLET_H_
#define INCLUDE_MMX_ECDSA_WALLET_H_

#include <mmx/KeyFile.hxx>
#include <mmx/skey_t.hpp>
#include <mmx/addr_t.hpp>
#include <mmx/pubkey_t.hpp>


namespace mmx {

class ECDSA_Wallet {
public:
	ECDSA_Wallet(std::shared_ptr<const KeyFile> key_file)
	{
		master_sk = hash_t(key_file->seed_value);
	}

	skey_t get_skey(const std::vector<uint32_t>& path) const
	{
		skey_t key = master_sk;
		for(const uint32_t i : path) {
			key = hash_t(hash_t(key + hash_t(&i, sizeof(i))) + hash_t(master_sk.bytes));
		}
		return key;
	}

	pubkey_t get_pubkey(const std::vector<uint32_t>& path) const
	{
		return pubkey_t::from_skey(get_skey(path));
	}

	addr_t get_address(const std::vector<uint32_t>& path) const
	{
		return get_pubkey(path);
	}

	skey_t get_skey(const uint32_t offset) const
	{
		return get_skey({0, offset});
	}

	pubkey_t get_pubkey(const uint32_t offset) const
	{
		return get_pubkey({0, offset});
	}

	addr_t get_address(const uint32_t offset) const
	{
		return get_pubkey({0, offset});
	}

private:
	skey_t master_sk;

};


} // mmx

#endif /* INCLUDE_MMX_ECDSA_WALLET_H_ */
