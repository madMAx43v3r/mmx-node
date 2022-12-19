/*
 * Compiler.cpp
 *
 *  Created on: Dec 10, 2022
 *      Author: mad
 */

#include <mmx/vm/Compiler.h>

#include <lexy/dsl.hpp>
#include <lexy/callback.hpp>
#include <lexy/action/parse.hpp>
#include <lexy/input/string_input.hpp>
#include <lexy_ext/report_error.hpp>

#include <uint256_t.h>

#include <iostream>


namespace lexy {

template<>
struct integer_traits<uint256_t>
{
	typedef uint256_t type;

	static constexpr auto is_bounded = true;

	static constexpr auto _max = uint256_max;

	template<int Radix>
	static constexpr std::size_t max_digit_count = 78;

	template<int Radix>
	static constexpr void add_digit_unchecked(type& result, unsigned digit)
	{
		result *= Radix;
		result += digit;
	}

	template<int Radix>
	static constexpr bool add_digit_checked(type& result, unsigned digit)
	{
		const auto prev = result;
		add_digit_unchecked<Radix>(result, digit);
		return result >= prev;
	}
};

} // lexy


namespace mmx {
namespace vm {
namespace lang {

namespace dsl = lexy::dsl;

struct integer_literal {
//	static constexpr auto rule = dsl::integer<uint256_t, dsl::decimal>;
	static constexpr auto rule =
			  (LEXY_LIT("0b") >> dsl::integer<uint256_t, dsl::binary>)
			| (LEXY_LIT("0x") >> dsl::integer<uint256_t, dsl::hex>)
			| dsl::else_ >> dsl::integer<uint256_t, dsl::decimal>;
	static constexpr auto value = lexy::as_integer<uint256_t>;
};

struct string_literal {
	static constexpr auto rule = dsl::quoted(-dsl::ascii::control, dsl::backslash_escape);
	static constexpr auto value = lexy::as_string<std::string>;
};

} // lang


std::shared_ptr<const contract::Binary> compile(const std::string& source)
{
	auto result = lexy::parse<lang::integer_literal>(
			lexy::string_input<lexy::utf8_encoding>(source), lexy_ext::report_error);
	if(result.has_value()) {
		std::cout << result.value() << std::endl;
	}
	return nullptr;
}


} // vm
} // mmx
