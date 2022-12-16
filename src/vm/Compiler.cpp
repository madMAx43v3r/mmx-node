/*
 * Compiler.cpp
 *
 *  Created on: Dec 10, 2022
 *      Author: mad
 */

#include <uint256_t.h>

#include <lexy/dsl.hpp>


namespace {
namespace lang {

namespace dsl = lexy::dsl;

template<>
struct integer_traits<uint256_t>
{
	using T = uint256_t;

	static constexpr auto is_bounded = true;

	static constexpr auto _max = []{
		return uint256_max;
	}();

	template <int Radix>
	static constexpr std::size_t max_digit_count = _digit_count(Radix, _max);

	template <int Radix>
	static constexpr void add_digit_unchecked(T& result, unsigned digit)
	{
		return integer_traits<T>::add_digit_unchecked(result, digit);
	}

	template <int Radix>
	static constexpr bool add_digit_checked(T& result, unsigned digit)
	{
		return integer_traits<T>::add_digit_checked(result, digit);
	}
};

template<typename Base>
struct integer_literal {
	static constexpr auto rule = [] {
		return dsl::integer<uint256_t>(dsl::digits<Base>());
    }();
	static constexpr auto value = lexy::as_integer<uint256_t>;
};

typedef integer_literal<dsl::hex> integer_literal_hex;
typedef integer_literal<dsl::decimal> integer_literal_dec;


} // lang
}


namespace mmx {
namespace vm {



} // vm
} // mmx
