/*
 * chiapos.cpp
 *
 *  Created on: Dec 9, 2021
 *      Author: mad
 */

#include <picosha2.hpp>
#include <verifier.hpp>
#include <prover_disk.hpp>

#include <mmx/chiapos.h>


namespace mmx {
namespace chiapos {

DiskProver::DiskProver(const std::string& file_path)
{
	impl = new ::DiskProver(file_path);
}

DiskProver::~DiskProver()
{
	delete (::DiskProver*)impl;
}

uint8_t DiskProver::get_ksize() const
{
	auto* prover = (::DiskProver*)impl;
	return prover->GetSize();
}

std::string DiskProver::get_file_path() const {
	auto* prover = (::DiskProver*)impl;
	return prover->GetFilename();
}

std::vector<uint8_t> DiskProver::get_plot_id() const
{
	auto* prover = (::DiskProver*)impl;
	return prover->GetId();
}

std::vector<uint8_t> DiskProver::get_farmer_key() const
{
	auto* prover = (::DiskProver*)impl;
	const auto memo = prover->GetMemo();
	if(memo.size() == 128) {
		return std::vector<uint8_t>(memo.begin() + 48, memo.begin() + 96);
	}
	throw std::logic_error("invalid plot memo");
}

std::vector<std::array<uint8_t, 32>> DiskProver::get_qualities(const std::array<uint8_t, 32>& challenge) const
{
	auto* prover = (::DiskProver*)impl;
	const auto tmp = prover->GetQualitiesForChallenge(challenge.data());

	std::vector<std::array<uint8_t, 32>> res;
	for(const auto& entry : tmp) {
		if(entry.GetSize() == 256) {
			std::array<uint8_t, 32> buffer;
			entry.ToBytes(buffer.data());
			res.push_back(buffer);
		}
	}
	return res;
}

std::shared_ptr<Proof> DiskProver::get_full_proof(const std::array<uint8_t, 32>& challenge, const size_t index) const
{
	auto* prover = (::DiskProver*)impl;
	const auto tmp = prover->GetFullProof(challenge.data(), index, true);
	if(tmp.GetSize() == 0) {
		return nullptr;
	}
	auto proof = std::make_shared<Proof>();
	proof->k = get_ksize();
	proof->id = get_plot_id();
	proof->proof.resize((tmp.GetSize() + 7) / 8);
	tmp.ToBytes(proof->proof.data());

	const auto memo = prover->GetMemo();
	if(memo.size() != 128) {
		throw std::logic_error("invalid plot memo");
	}
	proof->pool_key = std::vector<uint8_t>(memo.begin(), memo.begin() + 48);
	proof->farmer_key = std::vector<uint8_t>(memo.begin() + 48, memo.begin() + 96);
	proof->master_sk = std::vector<uint8_t>(memo.begin() + 96, memo.end());
	return proof;
}


std::array<uint8_t, 32> verify(	const uint8_t ksize,
								const std::array<uint8_t, 32>& plot_id,
								const std::array<uint8_t, 32>& challenge,
								const void* proof_bytes,
								const size_t proof_size)
{
	Verifier verifier;
	const auto bits = verifier.ValidateProof(
			plot_id.data(), ksize, challenge.data(),
			(const uint8_t*)proof_bytes, proof_size);

	if(bits.GetSize() == 0) {
		throw std::logic_error("invalid proof");
	}
	std::array<uint8_t, 32> quality;
	if(bits.GetSize() != quality.size() * 8) {
		throw std::logic_error("quality size mismatch");
	}
	bits.ToBytes(quality.data());
	return quality;
}


} // chiapos
} // mmx
