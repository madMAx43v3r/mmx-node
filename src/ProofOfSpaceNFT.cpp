/*
 * ProofOfSpaceNFT.cpp
 *
 *  Created on: Apr 13, 2022
 *      Author: mad
 */

#include <mmx/ProofOfSpaceNFT.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

vnx::bool_t ProofOfSpaceNFT::is_valid() const
{
	return Super::is_valid() && ksize > 0 && proof_bytes.size() <= 512;
}

mmx::hash_t ProofOfSpaceNFT::calc_hash(const vnx::bool_t& full_hash) const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	buffer.reserve(4 * 1024);

	write_bytes(out, get_type_hash());
	write_field(out, "version", 	version);
	write_field(out, "ksize", 		ksize);
	write_field(out, "score", 		score);
	write_field(out, "plot_id", 	plot_id);
	write_field(out, "proof_bytes", proof_bytes);
	write_field(out, "local_key", 	local_key);
	write_field(out, "farmer_key", 	farmer_key);
	write_field(out, "contract", 	contract);

	if(full_hash) {
		write_field(out, "local_sig", local_sig);
	}
	out.flush();

	return hash_t(buffer);
}

void ProofOfSpaceNFT::validate() const
{
	const auto local_key_bls = local_key.to_bls();
	const auto farmer_key_bls = farmer_key.to_bls();

	bls::AugSchemeMPL MPL;
	const auto taproot_sk = MPL.KeyGen(
			hash_t(bls_pubkey_t(local_key_bls + farmer_key_bls) + local_key + farmer_key).to_vector());

	const bls_pubkey_t plot_key = local_key_bls + farmer_key_bls + taproot_sk.GetG1Element();

	const uint32_t port = 11337;
	if(hash_t(hash_t(contract + plot_key) + bytes_t<4>(&port, 4)) != plot_id) {
		throw std::logic_error("invalid proof keys or port");
	}
	if(!local_sig.verify(local_key, calc_hash())) {
		throw std::logic_error("invalid proof signature");
	}
}


} // mmx
