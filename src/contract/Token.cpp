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
	if(time_factor && !time_factor->inverse) {
		return false;
	}
	for(const auto& entry : stake_factors) {
		const auto& factor = entry.second;
		if(!factor.value || !factor.inverse) {
			return false;
		}
		if(uint128_t(factor.value) * 10000 > factor.inverse) {
			return false;
		}
	}
	return Contract::is_valid() && name.size() <= 128
			&& symbol.size() >= 2 && symbol.size() <= 6 && symbol != "MMX"
			&& decimals >= 0 && decimals <= 12;
}

hash_t Token::calc_hash() const
{
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);

	write_bytes(out, get_type_hash());
	write_bytes(out, version);
	write_bytes(out, name);
	write_bytes(out, symbol);
	write_bytes(out, web_url);
	write_bytes(out, icon_url);
	write_bytes(out, decimals);
	write_bytes(out, owner);
	write_bytes(out, time_factor);
	for(const auto& entry : stake_factors) {
		write_bytes(out, entry.first);
		write_bytes(out, entry.second);
	}
	out.flush();

	return hash_t(buffer);
}

uint64_t Token::calc_cost(std::shared_ptr<const ChainParams> params) const
{
	return (8 + 4 + name.size() + symbol.size() + web_url.size() + icon_url.size()
			+ 4 + 32 + 16 + (32 + 16) * stake_factors.size()) * params->min_txfee_byte;
}

std::vector<addr_t> Token::get_dependency() const {
	if(owner) {
		return {*owner};
	}
	return {};
}

std::vector<addr_t> Token::get_parties() const {
	return get_dependency();
}

vnx::optional<addr_t> Token::get_owner() const {
	return owner;
}

std::vector<tx_out_t> Token::validate(std::shared_ptr<const Operation> operation, std::shared_ptr<const Context> context) const
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
		if(!is_mintable) {
			throw std::logic_error("token not mintable");
		}
		tx_out_t out;
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

void Token::set_time_factor(const vnx::optional<ulong_fraction_t>& factor)
{
	if(!is_adjustable) {
		throw std::logic_error("token not adjustable");
	}
	time_factor = factor;
}

void Token::set_stake_factor(const addr_t& currency, const vnx::optional<ulong_fraction_t>& factor)
{
	if(!is_adjustable) {
		throw std::logic_error("token not adjustable");
	}
	if(factor) {
		stake_factors[currency] = *factor;
	} else {
		stake_factors.erase(currency);
	}
}


} // contract
} // mmx
