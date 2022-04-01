/*
 * ProofOfSpace.cpp
 *
 *  Created on: Nov 30, 2021
 *      Author: mad
 */

#include <mmx/ProofOfSpace.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

vnx::bool_t ProofOfSpace::is_valid() const
{
	return ksize > 0 && proof_bytes.size() <= 512;
}

mmx::hash_t ProofOfSpace::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	buffer.reserve(4 * 1024);

	write_bytes(out, get_type_hash());
	write_bytes(out, version);
	write_bytes(out, ksize);
	write_bytes(out, plot_id);
	write_bytes(out, proof_bytes);
	write_bytes(out, local_key);
	write_bytes(out, farmer_key);
	write_bytes(out, pool_key);
	out.flush();

	return hash_t(buffer);
}

void ProofOfSpace::validate() const
{
	const bls_pubkey_t plot_key = local_key.to_bls() + farmer_key.to_bls();

	const uint32_t port = 11337;
	if(hash_t(hash_t(pool_key + plot_key) + bytes_t<4>(&port, 4)) != plot_id) {
		throw std::logic_error("invalid proof keys or port");
	}
	if(!local_sig.verify(local_key, calc_hash())) {
		throw std::logic_error("invalid proof signature");
	}
}


} // mmx
