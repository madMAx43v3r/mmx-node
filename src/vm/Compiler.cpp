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
#include <lexy/action/parse_as_tree.hpp>
#include <lexy/action/trace.hpp>
#include <lexy/input/string_input.hpp>
#include <lexy_ext/report_error.hpp>
#include <lexy_ext/shell.hpp>

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

struct source;
struct expression;
struct statement;

static constexpr auto kw_id = dsl::identifier(dsl::ascii::alpha);

static constexpr auto kw_var = LEXY_KEYWORD("var", kw_id);
static constexpr auto kw_null = LEXY_KEYWORD("null", kw_id);
static constexpr auto kw_true = LEXY_KEYWORD("true", kw_id);
static constexpr auto kw_false = LEXY_KEYWORD("false", kw_id);
static constexpr auto kw_const = LEXY_KEYWORD("const", kw_id);
static constexpr auto kw_return = LEXY_KEYWORD("return", kw_id);
static constexpr auto kw_function = LEXY_KEYWORD("function", kw_id);

struct identifier {
	static constexpr auto name = "vm.lang.identifier";
	static constexpr auto rule = dsl::identifier(dsl::ascii::alpha_underscore, dsl::ascii::alpha_digit_underscore);
	static constexpr auto value = lexy::as_string<std::string>;
};

struct primitive : lexy::token_production {
	static constexpr auto name = "vm.lang.primitive";
	static constexpr auto rule = dsl::literal_set(kw_null, kw_true, kw_false);
	static constexpr auto value = lexy::as_string<std::string>;
};

struct integer : lexy::token_production {
	struct hex {
		static constexpr auto name = "vm.lang.integer.hex";
		static constexpr auto rule = LEXY_LIT("0x") >> dsl::integer<uint256_t, dsl::hex>;
	};
	struct binary {
		static constexpr auto name = "vm.lang.integer.binary";
		static constexpr auto rule = LEXY_LIT("0b") >> dsl::integer<uint256_t, dsl::binary>;
	};
	struct decimal {
		static constexpr auto name = "vm.lang.integer.decimal";
		static constexpr auto rule = dsl::integer<uint256_t, dsl::decimal>;
	};
	static constexpr auto name = "vm.lang.integer";
	static constexpr auto rule = dsl::p<binary> | dsl::p<hex> | dsl::p<decimal>;
	static constexpr auto value = lexy::as_integer<uint256_t>;
};

struct string : lexy::token_production {
	static constexpr auto name = "vm.lang.string";
	static constexpr auto rule = dsl::quoted(-dsl::ascii::control, dsl::backslash_escape);
	static constexpr auto value = lexy::as_string<std::string>;
};

struct address : lexy::token_production {
	static constexpr auto name = "vm.lang.address";
	static constexpr auto rule =
			dsl::peek(LEXY_LIT("mmx1")) >> dsl::identifier(dsl::ascii::alpha_digit);
	static constexpr auto value = lexy::as_string<std::string>;
};

struct literal {
	static constexpr auto name = "vm.lang.literal";
	static constexpr auto rule =
			dsl::p<primitive> | dsl::p<address> | dsl::p<string> | dsl::p<integer>;
};

struct scope_ex {
	static constexpr auto name = "vm.lang.scope_ex";
	static constexpr auto rule = dsl::curly_bracketed(dsl::recurse<source>);
};

struct function {
	struct arguments {
		struct item {
			struct value {
				static constexpr auto name = "vm.lang.function.arguments.item.value";
				static constexpr auto rule = dsl::recurse<expression>;
			};
			static constexpr auto name = "vm.lang.function.arguments.item";
			static constexpr auto rule = dsl::p<identifier> + dsl::opt(dsl::equal_sign >> dsl::p<value>);
		};
		static constexpr auto name = "vm.lang.function.arguments";
		static constexpr auto rule = dsl::parenthesized.opt_list(dsl::p<item>, dsl::sep(dsl::comma));
	};
	static constexpr auto name = "vm.lang.function";
	static constexpr auto rule = kw_function >> dsl::p<identifier> + dsl::p<arguments> + dsl::p<scope_ex>;
};

struct qualifier : lexy::token_production {
	static constexpr auto name = "vm.lang.qualifier";
	static constexpr auto rule = dsl::literal_set(kw_var, kw_const);
	static constexpr auto value = lexy::as_string<std::string>;
};

struct op_not : lexy::token_production {
	static constexpr auto name = "vm.lang.op_not";
	static constexpr auto rule = dsl::lit_c<'!'>;
};

struct op_literal : lexy::token_production {
	static constexpr auto name = "vm.lang.op_literal";
	static constexpr auto rule = dsl::literal_set(
			dsl::lit_c<'='>, dsl::lit_c<'+'>, dsl::lit_c<'-'>, dsl::lit_c<'*'>, dsl::lit_c<'/'>, dsl::lit_c<'!'>, dsl::lit_c<':'>,
			dsl::lit_c<'>'>, dsl::lit_c<'<'>, LEXY_LIT(">="), LEXY_LIT("<="), LEXY_LIT("=="), LEXY_LIT("!="),
			LEXY_LIT("++"), LEXY_LIT("--"), LEXY_LIT(">>"), LEXY_LIT("<<"), kw_return);
	static constexpr auto value = lexy::as_string<std::string>;
};

struct variable {
	static constexpr auto name = "vm.lang.variable";
	static constexpr auto rule = dsl::p<qualifier> >>
			dsl::p<identifier> + dsl::opt(dsl::equal_sign >> dsl::recurse<expression>);
};

struct list_ex {
	static constexpr auto name = "vm.lang.list_ex";
	static constexpr auto rule = dsl::parenthesized.opt_list(dsl::recurse<expression>, dsl::sep(dsl::comma));
};

struct array_ex {
	static constexpr auto name = "vm.lang.array_ex";
	static constexpr auto rule = dsl::square_bracketed.opt_list(dsl::recurse<expression>, dsl::trailing_sep(dsl::comma));
};

struct expression {
	static constexpr auto name = "vm.lang.expression";
	static constexpr auto whitespace = dsl::ascii::space;
	static constexpr auto rule = dsl::loop(
			dsl::peek_not(dsl::comma | dsl::semicolon | dsl::lit_c<')'> | dsl::lit_c<']'> | dsl::lit_c<'}'> | dsl::eof) >> (
				dsl::p<function> | dsl::p<variable> | dsl::p<list_ex> | dsl::p<array_ex> | dsl::p<scope_ex> |
				dsl::p<op_literal> | dsl::p<literal> | dsl::p<identifier>
			) | dsl::break_);
};

struct statement {
	static constexpr auto name = "vm.lang.statement";
	static constexpr auto whitespace = dsl::ascii::space;
	static constexpr auto rule = dsl::p<expression>
			+ dsl::if_(dsl::peek_not(dsl::lit_c<'}'> | dsl::eof) >> (dsl::comma | dsl::semicolon));
};

struct source {
	static constexpr auto name = "vm.lang.source";
	static constexpr auto whitespace = dsl::ascii::space;
	static constexpr auto rule = dsl::loop((dsl::peek_not(dsl::lit_c<'}'> | dsl::eof) >> dsl::p<statement>) | dsl::break_);
};

} // lang


std::shared_ptr<const contract::Binary> compile(const std::string& source)
{
	typedef lang::source test_t;

	lexy_ext::shell<lexy_ext::default_prompt<lexy::utf8_encoding>> shell;

	lexy::trace_to<test_t>(shell.write_message().output_iterator(),
			lexy::string_input<lexy::utf8_encoding>(source), {lexy::visualize_fancy});

	lexy::parse_tree<lexy::input_reader<lexy::string_input<lexy::utf8_encoding>>> tree;
	auto result = lexy::parse_as_tree<test_t>(
			tree, lexy::string_input<lexy::utf8_encoding>(source), lexy_ext::report_error);

	if(result.is_success()) {
		std::cout << "Success" << std::endl;
	} else {
		std::cout << "Failed" << std::endl;
	}
	return nullptr;
}


} // vm
} // mmx
