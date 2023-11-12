/*
 * ProofOfSpaceOG.cpp
 *
 *  Created on: Nov 30, 2021
 *      Author: mad
 */

#include <mmx/ProofOfSpaceOG.hxx>
#include <mmx/write_bytes.h>

#include <bls.hpp>


namespace mmx {

vnx::bool_t ProofOfSpaceOG::is_valid() const
{
	return Super::is_valid() && ksize > 0 && proof_bytes.size() <= 512;
}

mmx::hash_t ProofOfSpaceOG::calc_hash(const vnx::bool_t& full_hash) const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	buffer.reserve(4 * 1024);

	write_bytes(out, get_type_hash());
	write_field(out, "version", 	version);
	write_field(out, "score", 		score);
	write_field(out, "plot_id", 	plot_id);
	write_field(out, "plot_key", 	plot_key);
	write_field(out, "farmer_key", 	farmer_key);
	write_field(out, "ksize", 		ksize);
	write_field(out, "proof_bytes", proof_bytes);
	write_field(out, "pool_key", 	pool_key);
	write_field(out, "local_key", 	local_key);
	out.flush();

	return hash_t(buffer);
}

void ProofOfSpaceOG::validate() const
{
	bls::G1Element local_key_bls;
	bls::G1Element farmer_key_bls;

	local_key.to(local_key_bls);
	farmer_key.to(farmer_key_bls);

	const bls_pubkey_t plot_key_ = local_key_bls + farmer_key_bls;

	if(plot_key_ != plot_key) {
		throw std::logic_error("invalid plot_key");
	}
	if(hash_t(hash_t(pool_key + plot_key_) + bytes_t<4>().from_uint(11337u)) != plot_id) {
		throw std::logic_error("invalid proof keys or port");
	}
}


} // mmx
