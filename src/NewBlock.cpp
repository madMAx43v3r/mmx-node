/*
 * NewBlock.cpp
 *
 *  Created on: Dec 18, 2024
 *      Author: mad
 */

#include <mmx/NewBlock.hxx>
#include <mmx/write_bytes.h>


namespace mmx {

bool NewBlock::is_valid() const
{
	for(auto vdf : vdf_chain) {
		if(!vdf || !vdf->is_valid()) {
			return false;
		}
	}
	return block
		&& block->is_valid()
		&& hash == calc_hash()
		&& content_hash == calc_content_hash();
}

hash_t NewBlock::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "block", block ? block->content_hash : hash_t());

	std::vector<hash_t> vdf_hash;
	for(auto vdf : vdf_chain) {
		vdf_hash.push_back(vdf ? vdf->content_hash : hash_t());
	}
	write_field(out, "vdf_chain", vdf_hash);
	out.flush();

	return hash_t(buffer);
}

hash_t NewBlock::calc_content_hash() const
{
	return hash_t(hash + farmer_sig);
}

void NewBlock::validate() const
{
	if(!block || !block->proof) {
		throw std::logic_error("missing proof");
	}
	if(!farmer_sig.verify(block->proof->farmer_key, hash)) {
		throw std::logic_error("invalid farmer signature");
	}
}


} // mmx
