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

static constexpr auto kw_if = LEXY_KEYWORD("if", kw_id);
static constexpr auto kw_do = LEXY_KEYWORD("do", kw_id);
static constexpr auto kw_in = LEXY_KEYWORD("in", kw_id);
static constexpr auto kw_of = LEXY_KEYWORD("of", kw_id);
static constexpr auto kw_for = LEXY_KEYWORD("for", kw_id);
static constexpr auto kw_else = LEXY_KEYWORD("else", kw_id);
static constexpr auto kw_while = LEXY_KEYWORD("while", kw_id);
static constexpr auto kw_var = LEXY_KEYWORD("var", kw_id);
static constexpr auto kw_let = LEXY_KEYWORD("let", kw_id);
static constexpr auto kw_null = LEXY_KEYWORD("null", kw_id);
static constexpr auto kw_true = LEXY_KEYWORD("true", kw_id);
static constexpr auto kw_false = LEXY_KEYWORD("false", kw_id);
static constexpr auto kw_const = LEXY_KEYWORD("const", kw_id);
static constexpr auto kw_return = LEXY_KEYWORD("return", kw_id);
static constexpr auto kw_function = LEXY_KEYWORD("function", kw_id);

struct identifier {
	static constexpr auto name = "mmx.lang.identifier";
	static constexpr auto rule = dsl::identifier(dsl::ascii::alpha_underscore, dsl::ascii::alpha_digit_underscore);
	static constexpr auto value = lexy::as_string<std::string>;
};

struct primitive : lexy::token_production {
	static constexpr auto name = "mmx.lang.primitive";
	static constexpr auto rule = dsl::literal_set(kw_null, kw_true, kw_false);
	static constexpr auto value = lexy::as_string<std::string>;
};

struct integer : lexy::token_production {
	struct hex {
		static constexpr auto name = "mmx.lang.integer.hex";
		static constexpr auto rule = LEXY_LIT("0x") >> dsl::integer<uint256_t, dsl::hex>;
	};
	struct binary {
		static constexpr auto name = "mmx.lang.integer.binary";
		static constexpr auto rule = LEXY_LIT("0b") >> dsl::integer<uint256_t, dsl::binary>;
	};
	struct decimal {
		static constexpr auto name = "mmx.lang.integer.decimal";
		static constexpr auto rule = dsl::integer<uint256_t, dsl::decimal>;
	};
	static constexpr auto name = "mmx.lang.integer";
	static constexpr auto rule = dsl::p<binary> | dsl::p<hex> | dsl::p<decimal>;
	static constexpr auto value = lexy::as_integer<uint256_t>;
};

struct string : lexy::token_production {
	static constexpr auto name = "mmx.lang.string";
	static constexpr auto rule = dsl::quoted(-dsl::ascii::control, dsl::backslash_escape);
	static constexpr auto value = lexy::as_string<std::string>;
};

struct address : lexy::token_production {
	static constexpr auto name = "mmx.lang.address";
	static constexpr auto rule =
			dsl::peek(LEXY_LIT("mmx1")) >> dsl::identifier(dsl::ascii::alpha_digit);
	static constexpr auto value = lexy::as_string<std::string>;
};

struct literal {
	static constexpr auto name = "mmx.lang.literal";
	static constexpr auto rule =
			dsl::p<primitive> | dsl::p<address> | dsl::p<string> | dsl::p<integer>;
};

struct scope_ex {
	static constexpr auto name = "mmx.lang.scope_ex";
	static constexpr auto rule = dsl::curly_bracketed(dsl::recurse<source>);
};

struct function {
	struct arguments {
		struct item {
			struct value {
				static constexpr auto name = "mmx.lang.function.arguments.item.value";
				static constexpr auto rule = dsl::recurse<expression>;
			};
			static constexpr auto name = "mmx.lang.function.arguments.item";
			static constexpr auto rule = dsl::p<identifier> + dsl::opt(dsl::equal_sign >> dsl::p<value>);
		};
		static constexpr auto name = "mmx.lang.function.arguments";
		static constexpr auto rule = dsl::parenthesized.opt_list(dsl::p<item>, dsl::sep(dsl::comma));
	};
	static constexpr auto name = "mmx.lang.function";
	static constexpr auto rule = kw_function >> dsl::p<identifier> + dsl::p<arguments> + dsl::p<scope_ex>;
};

struct qualifier : lexy::token_production {
	static constexpr auto name = "mmx.lang.qualifier";
	static constexpr auto rule = dsl::literal_set(kw_let, kw_var, kw_const);
	static constexpr auto value = lexy::as_string<std::string>;
};

struct op_not : lexy::token_production {
	static constexpr auto name = "mmx.lang.op_not";
	static constexpr auto rule = dsl::lit_c<'!'>;
};

struct op_literal : lexy::token_production {
	static constexpr auto name = "mmx.lang.op_literal";
	static constexpr auto rule = dsl::literal_set(
			dsl::lit_c<'='>, dsl::lit_c<'+'>, dsl::lit_c<'-'>, dsl::lit_c<'*'>, dsl::lit_c<'/'>, dsl::lit_c<'!'>, dsl::lit_c<':'>,
			dsl::lit_c<'>'>, dsl::lit_c<'<'>, LEXY_LIT(">="), LEXY_LIT("<="), LEXY_LIT("=="), LEXY_LIT("!="),
			LEXY_LIT("++"), LEXY_LIT("--"), LEXY_LIT(">>"), LEXY_LIT("<<"),
			kw_return, kw_do, kw_else, kw_of, kw_in);
	static constexpr auto value = lexy::as_string<std::string>;
};

struct variable {
	static constexpr auto name = "mmx.lang.variable";
	static constexpr auto rule = dsl::p<qualifier> >>
			dsl::p<identifier> + dsl::opt(dsl::equal_sign >> dsl::recurse<expression>);
};

struct list_ex {
	static constexpr auto name = "mmx.lang.list_ex";
	static constexpr auto rule = dsl::parenthesized.opt_list(dsl::recurse<expression>, dsl::sep(dsl::comma));
};

struct array_ex {
	static constexpr auto name = "mmx.lang.array_ex";
	static constexpr auto rule = dsl::square_bracketed.opt_list(dsl::recurse<expression>, dsl::trailing_sep(dsl::comma));
};

struct if_ex {
	static constexpr auto name = "mmx.lang.if_ex";
	static constexpr auto rule = kw_if >> dsl::parenthesized(dsl::recurse<expression>);
};

struct for_loop {
	static constexpr auto name = "mmx.lang.for_loop";
	static constexpr auto rule = kw_for >> dsl::parenthesized.opt_list(dsl::recurse<expression>, dsl::sep(dsl::comma | dsl::semicolon));
};

struct while_loop {
	static constexpr auto name = "mmx.lang.while_loop";
	static constexpr auto rule = kw_while >> dsl::parenthesized(dsl::recurse<expression>);
};

struct expression {
	static constexpr auto name = "mmx.lang.expression";
	static constexpr auto whitespace = dsl::ascii::space;
	static constexpr auto rule = dsl::loop(
			dsl::peek_not(dsl::comma | dsl::semicolon | dsl::lit_c<')'> | dsl::lit_c<']'> | dsl::lit_c<'}'> | dsl::eof) >> (
				(dsl::p<scope_ex> >> dsl::break_) | (dsl::p<function> >> dsl::break_) |
				dsl::p<variable> | dsl::p<list_ex> | dsl::p<array_ex> | dsl::p<op_literal> |
				dsl::p<if_ex> | dsl::p<for_loop> | dsl::p<while_loop> | dsl::p<literal> | dsl::p<identifier>
			) | dsl::break_);
};

struct statement {
	static constexpr auto name = "mmx.lang.statement";
	static constexpr auto whitespace = dsl::ascii::space;
	static constexpr auto rule = dsl::p<expression> + dsl::if_(dsl::comma | dsl::semicolon);
};

struct source {
	static constexpr auto name = "mmx.lang.source";
	static constexpr auto whitespace = dsl::ascii::space;
	static constexpr auto rule = dsl::loop((dsl::peek_not(dsl::lit_c<'}'> | dsl::eof) >> dsl::p<statement>) | dsl::break_);
};

} // lang


template<typename Node>
void dump_parse_tree(const Node& node, std::ostream& out, int depth = 0)
{
	const std::string name(node.kind().name());
	if(name == "whitespace") {
		return;
	}
	out << depth << (depth < 10 ? " " : "");

	for(int i = 0; i < depth; ++i) {
		out << "  ";
	}
	out << name;

	if(node.kind().is_token()) {
		const auto token = node.lexeme();
		out << ": " << vnx::to_string(std::string(token.begin(), token.end()));
	}
	out << std::endl;

	for(const auto& child : node.children()) {
		dump_parse_tree(child, out, depth + 1);
	}
}

std::shared_ptr<const contract::Binary> compile(const std::string& source)
{
	typedef lang::source test_t;

	lexy_ext::shell<lexy_ext::default_prompt<lexy::utf8_encoding>> shell;

	lexy::trace_to<test_t>(shell.write_message().output_iterator(),
			lexy::string_input<lexy::utf8_encoding>(source), {lexy::visualize_fancy});

	lexy::parse_tree<lexy::input_reader<lexy::string_input<lexy::utf8_encoding>>> tree;
	auto result = lexy::parse_as_tree<test_t>(
			tree, lexy::string_input<lexy::utf8_encoding>(source), lexy_ext::report_error);

	std::cout << std::endl;
	dump_parse_tree(tree.root(), std::cout);
	std::cout << std::endl;

	if(result.is_success()) {
		std::cout << "Success" << std::endl;
	} else {
		std::cout << "Failed" << std::endl;
	}
	return nullptr;
}


} // vm
} // mmx
