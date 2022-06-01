/*
 * Token.cpp
 *
 *  Created on: Jan 16, 2022
 *      Author: mad
 */

#include <mmx/contract/Token.hxx>
#include <mmx/operation/Mint.hxx>
#include <mmx/write_bytes.h>


namespace mmx {
namespace contract {

vnx::bool_t Token::is_valid() const
{
	return Super::is_valid();
}

hash_t Token::calc_hash(const vnx::bool_t& full_hash) const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_field(out, "version", 	version);
	write_field(out, "name", 		name);
	write_field(out, "symbol", 		symbol);
	write_field(out, "web_url", 	web_url);
	write_field(out, "icon_url", 	icon_url);
	write_field(out, "decimals", 	decimals);
	write_field(out, "owner", 		owner);
	out.flush();

	return hash_t(buffer);
}

std::vector<addr_t> Token::get_dependency() const {
	if(owner) {
		return {*owner};
	}
	return {};
}

vnx::optional<addr_t> Token::get_owner() const {
	return owner;
}

std::vector<txout_t> Token::validate(std::shared_ptr<const Operation> operation, std::shared_ptr<const Context> context) const
{
	if(!owner) {
		throw std::logic_error("!owner");
	}
	{
		auto contract = context->get_contract(*owner);
		if(!contract) {
			throw std::logic_error("missing dependency");
		}
		contract->validate(operation, context);
	}
	if(auto mint = std::dynamic_pointer_cast<const operation::Mint>(operation))
	{
		txout_t out;
		out.contract = mint->address;
		out.address = mint->target;
		out.amount = mint->amount;
		return {out};
	}
	return {};
}

void Token::transfer(const vnx::optional<addr_t>& new_owner)
{
	owner = new_owner;
}


} // contract
} // mmx
