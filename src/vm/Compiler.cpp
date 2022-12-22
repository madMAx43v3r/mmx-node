/*
 * Compiler.cpp
 *
 *  Created on: Dec 10, 2022
 *      Author: mad
 */

#include <mmx/vm/Compiler.h>
#include <mmx/vm/varptr_t.hpp>
#include <mmx/vm_interface.h>

#include <lexy/dsl.hpp>
#include <lexy/callback.hpp>
#include <lexy/action/parse_as_tree.hpp>
#include <lexy/action/trace.hpp>
#include <lexy/input/string_input.hpp>
#include <lexy_ext/parse_tree_algorithm.hpp>
#include <lexy_ext/report_error.hpp>
#include <lexy_ext/shell.hpp>

#include <uint256_t.h>

#include <memory>
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
static constexpr auto kw_this = LEXY_KEYWORD("this", kw_id);
static constexpr auto kw_for = LEXY_KEYWORD("for", kw_id);
static constexpr auto kw_else = LEXY_KEYWORD("else", kw_id);
static constexpr auto kw_while = LEXY_KEYWORD("while", kw_id);
static constexpr auto kw_var = LEXY_KEYWORD("var", kw_id);
static constexpr auto kw_let = LEXY_KEYWORD("let", kw_id);
static constexpr auto kw_null = LEXY_KEYWORD("null", kw_id);
static constexpr auto kw_true = LEXY_KEYWORD("true", kw_id);
static constexpr auto kw_false = LEXY_KEYWORD("false", kw_id);
static constexpr auto kw_const = LEXY_KEYWORD("const", kw_id);
static constexpr auto kw_public = LEXY_KEYWORD("public", kw_id);
static constexpr auto kw_payable = LEXY_KEYWORD("payable", kw_id);
static constexpr auto kw_return = LEXY_KEYWORD("return", kw_id);
static constexpr auto kw_function = LEXY_KEYWORD("function", kw_id);
static constexpr auto kw_namespace = LEXY_KEYWORD("namespace", kw_id);

struct reserved {
	static constexpr auto name = "mmx.lang.reserved";
	static constexpr auto rule = dsl::literal_set(
			kw_if, kw_do, kw_in, kw_of, kw_for, kw_else, kw_while, kw_var, kw_let,
			kw_null, kw_true, kw_false, kw_const, kw_public, kw_payable, kw_return,
			kw_function, kw_namespace, kw_this);
};

struct identifier : lexy::token_production {
	struct invalid_reserved {
		static constexpr auto name() { return "invalid identifier: reserved keyword"; }
	};
	static constexpr auto name = "mmx.lang.identifier";
	static constexpr auto rule = dsl::peek(dsl::p<reserved>) >> dsl::error<invalid_reserved>
			| dsl::identifier(dsl::ascii::alpha_underscore, dsl::ascii::alpha_digit_underscore);
};

struct primitive : lexy::token_production {
	static constexpr auto name = "mmx.lang.primitive";
	static constexpr auto rule = dsl::literal_set(kw_null, kw_true, kw_false);
};

struct integer {
	struct hex : lexy::token_production {
		static constexpr auto name = "mmx.lang.integer.hex";
		static constexpr auto rule = dsl::digits<dsl::hex>;
	};
	struct binary : lexy::token_production {
		static constexpr auto name = "mmx.lang.integer.binary";
		static constexpr auto rule = dsl::digits<dsl::binary>;
	};
	struct decimal : lexy::token_production {
		static constexpr auto name = "mmx.lang.integer.decimal";
		static constexpr auto rule = dsl::digits<dsl::decimal>;
	};
	static constexpr auto name = "mmx.lang.integer";
	static constexpr auto rule =
			(LEXY_LIT("0b") >> dsl::p<binary>) | (LEXY_LIT("0x") >> dsl::p<hex>) | dsl::p<decimal>;
};

struct string : lexy::token_production {
	static constexpr auto name = "mmx.lang.string";
	static constexpr auto rule = dsl::quoted(-dsl::ascii::control, dsl::backslash_escape);
};

struct address : lexy::token_production {
	static constexpr auto name = "mmx.lang.address";
	static constexpr auto rule =
			dsl::peek(LEXY_LIT("mmx1")) >> dsl::identifier(dsl::ascii::alpha_digit);
};

struct constant {
	static constexpr auto name = "mmx.lang.constant";
	static constexpr auto rule =
			dsl::p<primitive> | dsl::p<address> | dsl::p<string> | dsl::p<integer>;
};

struct scope {
	static constexpr auto name = "mmx.lang.scope";
	static constexpr auto rule = dsl::curly_bracketed(dsl::recurse<source>);
};

struct namespace_ex {
	static constexpr auto name = "mmx.lang.namespace";
	static constexpr auto rule = kw_namespace >> dsl::p<identifier> + dsl::p<scope>;
};

struct function {
	struct arguments {
		struct item {
			static constexpr auto name = "mmx.lang.function.arguments.item";
			static constexpr auto rule = dsl::p<identifier> + dsl::opt(dsl::equal_sign >> dsl::p<constant>);
		};
		static constexpr auto name = "mmx.lang.function.arguments";
		static constexpr auto rule = dsl::parenthesized.opt_list(dsl::p<item>, dsl::sep(dsl::comma));
	};
	struct qualifier {
		static constexpr auto name = "mmx.lang.function.qualifier";
		static constexpr auto rule = dsl::literal_set(kw_const, kw_public, kw_payable);
	};
	static constexpr auto name = "mmx.lang.function";
	static constexpr auto rule = kw_function >>
			dsl::p<identifier> + dsl::p<arguments> + dsl::while_(dsl::p<qualifier>) + dsl::p<scope>;
};

struct qualifier : lexy::token_production {
	static constexpr auto name = "mmx.lang.qualifier";
	static constexpr auto rule = dsl::literal_set(kw_let, kw_var, kw_const);
};

struct operator_ex : lexy::token_production {
	static constexpr auto name = "mmx.lang.operator";
	static constexpr auto rule = dsl::literal_set(
			dsl::lit_c<'='>, dsl::lit_c<'+'>, dsl::lit_c<'-'>, dsl::lit_c<'*'>, dsl::lit_c<'/'>, dsl::lit_c<'!'>, dsl::lit_c<':'>,
			dsl::lit_c<'>'>, dsl::lit_c<'<'>, LEXY_LIT(">="), LEXY_LIT("<="), LEXY_LIT("=="), LEXY_LIT("!="),
			LEXY_LIT("++"), LEXY_LIT("--"), LEXY_LIT(">>"), LEXY_LIT("<<"),
			kw_return, kw_do, kw_else, kw_of, kw_in);
};

struct variable {
	static constexpr auto name = "mmx.lang.variable";
	static constexpr auto rule = dsl::p<qualifier> >>
			dsl::p<identifier> + dsl::opt(dsl::equal_sign >> dsl::recurse<expression>);
};

struct list {
	static constexpr auto name = "mmx.lang.list";
	static constexpr auto rule = dsl::parenthesized.opt_list(dsl::recurse<expression>, dsl::sep(dsl::comma));
};

struct array {
	static constexpr auto name = "mmx.lang.array";
	static constexpr auto rule = dsl::square_bracketed.opt_list(dsl::recurse<expression>, dsl::trailing_sep(dsl::comma));
};

struct if_ex {
	static constexpr auto name = "mmx.lang.if";
	static constexpr auto rule = kw_if >> dsl::parenthesized(dsl::recurse<expression>);
};

struct for_loop {
	static constexpr auto name = "mmx.lang.for";
	static constexpr auto rule = kw_for >> dsl::parenthesized.opt_list(dsl::recurse<expression>, dsl::sep(dsl::comma | dsl::semicolon));
};

struct while_loop {
	static constexpr auto name = "mmx.lang.while";
	static constexpr auto rule = kw_while >> dsl::parenthesized(dsl::recurse<expression>);
};

struct expression {
	static constexpr auto name = "mmx.lang.expression";
	static constexpr auto whitespace = dsl::ascii::space;
	static constexpr auto rule = dsl::loop(
			dsl::peek_not(dsl::comma | dsl::semicolon | dsl::lit_c<')'> | dsl::lit_c<']'> | dsl::lit_c<'}'> | dsl::eof) >> (
				(dsl::p<scope> >> dsl::break_) | (dsl::p<function> >> dsl::break_) | (dsl::p<namespace_ex> >> dsl::break_) |
				dsl::p<variable> | dsl::p<list> | dsl::p<array> | dsl::p<operator_ex> |
				dsl::p<if_ex> | dsl::p<for_loop> | dsl::p<while_loop> | dsl::p<constant> | dsl::p<identifier>
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
void dump_parse_tree(const Node& node, std::ostream& out, int depth = 0, int offset = 0)
{
	out << (depth < 10 ? " " : "") << depth << ":" << (offset < 10 ? " " : "") << offset << " ";

	for(int i = 0; i < depth; ++i) {
		out << "  ";
	}
	const std::string name(node.kind().name());
	out << name;

	if(node.kind().is_token()) {
		const auto token = node.lexeme();
		out << ": " << vnx::to_string(std::string(token.begin(), token.end()));
	}
	out << std::endl;

	int i = 0;
	for(const auto& child : node.children()) {
		const std::string name(child.kind().name());
		if(name != "whitespace") {
			dump_parse_tree(child, out, depth + 1, i++);
		}
	}
}

class Compiler {
public:
	static const std::string version;

	std::shared_ptr<const contract::Binary> compile(const std::string& source);

protected:
	struct variable_t {
		bool is_const = false;
		bool is_static = false;
		uint64_t address = 0;
		std::string name;
		varptr_t value;
	};

	struct function_t {
		bool is_const = false;
		bool is_public = false;
		bool is_payable = false;
		uint64_t address = 0;
		std::string name;
		std::vector<variable_t> args;
	};

	struct scope_t {
		std::vector<variable_t> stack;
		std::map<std::string, uint32_t> local_map;
	};

	typedef lexy::parse_tree<lexy::input_reader<lexy::string_input<lexy::utf8_encoding>>> parse_tree_t;
	typedef typename parse_tree_t::node node_t;

	void parse(parse_tree_t& tree, const std::string& source);

	void first_pass(const node_t& node);

	void second_pass(const node_t& node);

	const variable_t* find_variable(const std::string& name) const;

	variable_t get_variable(const std::string& name);

	uint32_t get_const_addr(const varptr_t& value, const std::string* name = nullptr);

	std::string get_namespace(const bool concat = false) const;

	std::ostream& debug(bool ident = false) const;

	void print_debug_info(const node_t& node);

	static std::vector<node_t> get_children(const node_t& node);

	static std::string get_literal(const node_t& node);
	static std::string get_identifier(const node_t& node);
	static varptr_t get_constant(const node_t& node);

private:
	int depth = 0;
	bool is_debug = true;
	vnx::optional<node_t> curr_node;
	vnx::optional<function_t> curr_function;

	std::vector<scope_t> frame;
	std::vector<variable_t> const_vars;
	std::vector<std::string> name_space;

	std::map<varptr_t, uint32_t> const_table;
	std::map<std::string, uint32_t> const_map;
	std::map<std::string, function_t> function_map;

	std::shared_ptr<contract::Binary> binary;

	mutable std::stringstream debug_out;

};

const std::string Compiler::version = "0.0.0";

std::shared_ptr<const contract::Binary> Compiler::compile(const std::string& source)
{
	if(binary) {
		throw std::logic_error("compile() called twice");
	}
	binary = std::make_shared<contract::Binary>();
	binary->source = source;
	binary->compiler = std::string("mmx") + "-" + version;

	parse_tree_t tree;
	parse(tree, source);

	std::cout << std::endl;
	dump_parse_tree(tree.root(), std::cout);
	std::cout << std::endl;

	{
		variable_t var;
		var.is_const = true;
		var.value = std::make_unique<var_t>();
		const_table[var.value] = const_vars.size();
		const_vars.push_back(var);
	}

	try {
		if(is_debug) {
			std::cout << "First pass ..." << std::endl;
		}
		first_pass(tree.root());

		name_space.clear();
		frame.emplace_back();

		curr_node = nullptr;
		curr_function = nullptr;

		if(is_debug) {
			std::cout << "Second pass ..." << std::endl;
		}
//		second_pass(tree.root());
	}
	catch(const std::exception& ex) {
//		lexy_ext::node_position(tree, curr_node);
		throw std::logic_error(std::string("error: ") + ex.what());
	}

	return binary;
}

void Compiler::parse(parse_tree_t& tree, const std::string& source)
{
	const auto result = lexy::parse_as_tree<lang::source>(
			tree, lexy::string_input<lexy::utf8_encoding>(source), lexy_ext::report_error);

	if(!result.is_success()) {
		throw std::runtime_error("parse() failed");
	}
}

std::vector<Compiler::node_t> Compiler::get_children(const node_t& node)
{
	std::vector<Compiler::node_t> list;
	for(const auto& child : node.children()) {
		const std::string name(child.kind().name());
		if(name != "whitespace") {
			list.push_back(child);
		}
	}
	return list;
}

std::string Compiler::get_literal(const node_t& node)
{
	if(!node.kind().is_token()) {
		throw std::logic_error("not a token");
	}
	const auto token = node.lexeme();
	return std::string(token.begin(), token.end());
}

std::string Compiler::get_identifier(const node_t& node)
{
	const auto list = get_children(node);
	const std::string name(node.kind().name());

	if(name == "identifier") {
		return get_literal(node);
	}
	else if(name == lang::identifier::name) {
		if(list.size() != 1) {
			throw std::logic_error("invalid identifier");
		}
		return get_literal(list[0]);
	}
	throw std::logic_error("not an identifier");
}

varptr_t Compiler::get_constant(const node_t& node)
{
	const std::string name(node.kind().name());
	if(name != lang::constant::name) {
		throw std::logic_error("not a constant");
	}
	const auto list = get_children(node);
	if(list.size() != 1) {
		throw std::logic_error("invalid constant");
	}
	{
		const auto node = list[0];
		const auto list = get_children(node);
		const std::string name(node.kind().name());

		if(name == lang::primitive::name) {
			if(list.size() != 1) {
				throw std::logic_error("invalid primitive");
			}
			const auto value = get_literal(list[0]);
			if(value == "null") {
				return std::make_unique<var_t>();
			} else if(value == "true") {
				return std::make_unique<var_t>(TYPE_TRUE);
			} else if(value == "false") {
				return std::make_unique<var_t>(TYPE_FALSE);
			}
		}
		else if(name == lang::integer::name) {
			if(list.size() != 1) {
				throw std::logic_error("invalid integer");
			}
			const auto node = list[0];
			const std::string name(node.kind().name());
			const auto list = get_children(node);
			if(list.size() != 1) {
				throw std::logic_error("invalid integer");
			}
			const auto value = get_literal(list[0]);

			if(name == lang::integer::decimal::name) {
				return std::make_unique<uint_t>(uint256_t(value, 10));
			} else if(name == lang::integer::hex::name) {
				return std::make_unique<uint_t>(uint256_t(value, 16));
			} else if(name == lang::integer::binary::name) {
				return std::make_unique<uint_t>(uint256_t(value, 2));
			}  else {
				throw std::logic_error("invalid integer");
			}
		}
		else if(name == lang::string::name) {
			if(list.size() != 3) {
				throw std::logic_error("invalid string");
			}
			return binary_t::alloc(get_literal(list[1]));
		}
		else if(name == lang::address::name) {
			if(list.size() != 1) {
				throw std::logic_error("invalid address");
			}
			return std::make_unique<uint_t>(addr_t(get_literal(list[0])).to_uint256());
		}
	}
	throw std::logic_error("invalid constant");
}

std::string Compiler::get_namespace(const bool concat) const
{
	std::string res;
	for(const auto& name : name_space) {
		if(!res.empty()) {
			res += ".";
		}
		res += name;
	}
	if(concat && !res.empty()) {
		res += ".";
	}
	return res;
}

std::ostream& Compiler::debug(bool ident) const
{
	auto& out = is_debug ? std::cout : debug_out;
	for(int i = 0; ident && i < depth; ++i) {
		out << "  ";
	}
	return out;
}

void Compiler::print_debug_info(const node_t& node)
{
	debug(true) << node.kind().name();

	if(node.kind().is_token()) {
		const auto token = node.lexeme();
		debug() << ": " << vnx::to_string(std::string(token.begin(), token.end()));
	}
	debug() << std::endl;
}

void Compiler::first_pass(const node_t& node)
{
	curr_node = node;
	print_debug_info(node);
	depth++;

	const std::string name(node.kind().name());
	const std::string p_name(node.parent().kind().name());
	const auto list = get_children(node);

	if(name == lang::function::name) {
		if(list.size() < 3) {
			throw std::logic_error("invalid function declaration");
		}
		function_t func;
		func.name = get_namespace(true) + get_identifier(list[1]);
		debug(true) << "name = \"" << func.name << "\"" << std::endl;

		if(function_map.count(func.name)) {
			throw std::logic_error("duplicate function name");
		}
		for(const auto& arg : get_children(list[2]))
		{
			const std::string name(arg.kind().name());
			if(name == lang::function::arguments::item::name) {
				const auto list = get_children(arg);
				if(list.size() < 1) {
					throw std::logic_error("invalid function argument");
				}
				const std::string name(list[0].kind().name());
				if(name != lang::identifier::name) {
					throw std::logic_error("expected argument identifier");
				}
				variable_t var;
				var.is_const = true;
				var.name = get_identifier(list[0]);
				var.address = MEM_STACK + 1 + func.args.size();

				debug(true) << "[" << func.args.size() << "] " << var.name;
				if(list.size() == 3) {
					var.value = get_constant(list[2]);
					debug() << " = " << to_string(var.value);
				} else if(list.size() != 1) {
					throw std::logic_error("invalid function argument");
				}
				debug() << std::endl;

				func.args.push_back(var);
			}
		}
		for(size_t i = 3; i < list.size(); ++i)
		{
			const auto node = list[i];
			const std::string name(node.kind().name());
			if(name == lang::function::qualifier::name) {
				const auto list = get_children(node);
				if(list.size() != 1) {
					throw std::logic_error("invalid function qualifier");
				}
				const auto value = get_literal(list[0]);
				if(value == "const") {
					func.is_const = true;
					debug(true) << "is_const = true" << std::endl;
				}
				else if(value == "public") {
					func.is_public = true;
					debug(true) << "is_public = true" << std::endl;
				}
				else if(value == "payable") {
					func.is_payable = true;
					debug(true) << "is_payable = true" << std::endl;
				}
				else {
					throw std::logic_error("invalid function qualifier");
				}
			}
		}
		function_map[func.name] = func;
		return;
	}
	else if(name == lang::namespace_ex::name) {
		if(list.size() < 2) {
			throw std::logic_error("invalid namespace declaration");
		}
		name_space.push_back(get_identifier(list[1]));
	}
	else if(p_name == lang::expression::name) {
		return;
	}

	for(const auto& node : list) {
		first_pass(node);
	}
	depth--;
}

void Compiler::second_pass(const node_t& node)
{
	curr_node = node;
	print_debug_info(node);
	depth++;

	const std::string name(node.kind().name());
	const auto list = get_children(node);

	if(name == lang::variable::name) {
		if(list.size() < 2) {
			throw std::logic_error("invalid variable declaration");
		}
		variable_t var;
		var.name = get_identifier(list[1]);

		const auto qualifier = get_literal(list[0]);
		if(qualifier == "const") {
			var.is_const = true;
			var.address = MEM_CONST + const_vars.size();
			const_map[var.name] = const_vars.size();
			const_vars.push_back(var);
		}
		else if(qualifier == "var") {
			auto& scope = frame[0];
			var.address = MEM_STATIC + scope.stack.size();
			scope.local_map[var.name] = scope.stack.size();
			scope.stack.push_back(var);
		}
		else {
			throw std::logic_error("invalid variable qualifier");
		}
		// TODO
	}

	for(const auto& node : list) {
		first_pass(node);
	}
	depth--;
}

const Compiler::variable_t* Compiler::find_variable(const std::string& name) const
{
	for(auto iter = frame.rbegin(); iter != frame.rend(); ++iter) {
		auto iter2 = iter->local_map.find(name);
		if(iter2 != iter->local_map.end()) {
			return &iter->stack[iter2->second];
		}
	}
	{
		auto iter = const_map.find(name);
		if(iter != const_map.end()) {
			return &const_vars[iter->second];
		}
	}
	return nullptr;
}

Compiler::variable_t Compiler::get_variable(const std::string& name)
{
	if(auto var = find_variable(name)) {
		return *var;
	}
	throw std::logic_error("no such variable: " + name);
}

uint32_t Compiler::get_const_addr(const varptr_t& value, const std::string* name)
{
	if(!value) {
		throw std::logic_error("get_const_addr(): !value");
	}
	{
		auto iter = const_table.find(value);
		if(iter != const_table.end()) {
			if(name) {
				const_map[*name] = iter->second;
			}
			return iter->second;
		}
	}
	const auto addr = const_table[value] = const_vars.size();

	variable_t var;
	var.value = value;
	var.address = MEM_CONST + addr;
	if(name) {
		var.name = *name;
		const_map[*name] = addr;
	}
	const_vars.push_back(var);
	return addr;
}


std::shared_ptr<const contract::Binary> compile(const std::string& source)
{
	typedef lang::source test_t;

	lexy_ext::shell<lexy_ext::default_prompt<lexy::utf8_encoding>> shell;

	lexy::trace_to<test_t>(shell.write_message().output_iterator(),
			lexy::string_input<lexy::utf8_encoding>(source), {lexy::visualize_fancy});

	Compiler compiler;
	return compiler.compile(source);
}


} // vm
} // mmx
