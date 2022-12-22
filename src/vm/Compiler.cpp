/*
 * Compiler.cpp
 *
 *  Created on: Dec 10, 2022
 *      Author: mad
 */

#include <mmx/vm/Compiler.h>
#include <mmx/vm/instr_t.h>
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

static constexpr auto ws = dsl::whitespace(dsl::ascii::space);
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
		static constexpr auto name = "invalid identifier: reserved keyword";
	};
	static constexpr auto name = "mmx.lang.identifier";
	static constexpr auto rule = dsl::peek(dsl::p<reserved>) >> dsl::error<invalid_reserved>
			| dsl::identifier(dsl::ascii::alpha_underscore, dsl::ascii::alpha_digit_underscore);
};

struct primitive : lexy::token_production {
	static constexpr auto name = "mmx.lang.primitive";
	static constexpr auto rule = dsl::literal_set(kw_null, kw_true, kw_false);
};

struct integer : lexy::transparent_production {
	struct invalid_integer {
		static constexpr auto name = "invalid integer";
	};
	struct hex : lexy::token_production {
		static constexpr auto name = "mmx.lang.integer.hex";
		static constexpr auto rule =
				dsl::digits<dsl::hex> >> dsl::if_(dsl::peek(dsl::ascii::alpha_underscore) >> dsl::error<invalid_integer>);
	};
	struct binary : lexy::token_production {
		static constexpr auto name = "mmx.lang.integer.binary";
		static constexpr auto rule =
				dsl::digits<dsl::binary> >> dsl::if_(dsl::peek(dsl::ascii::alpha_digit_underscore) >> dsl::error<invalid_integer>);
	};
	struct decimal : lexy::token_production {
		static constexpr auto name = "mmx.lang.integer.decimal";
		static constexpr auto rule =
				dsl::digits<dsl::decimal> >> dsl::if_(dsl::peek(dsl::ascii::alpha_underscore) >> dsl::error<invalid_integer>);
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

struct expected_constant {
	static constexpr auto name = "expected a constant";
};

struct function {
	struct arguments {
		struct item {
			static constexpr auto name = "mmx.lang.function.arguments.item";
			static constexpr auto rule = dsl::p<identifier> +
					dsl::opt(dsl::equal_sign >> (dsl::p<constant> | dsl::error<expected_constant>));
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

struct call_ex {
	static constexpr auto name = "mmx.lang.call";
	static constexpr auto rule = dsl::peek(dsl::p<identifier> + ws + dsl::lit_c<'('>) >>
			dsl::p<identifier> + dsl::parenthesized.opt_list(dsl::recurse<expression>, dsl::sep(dsl::comma));
};

struct operator_ex : lexy::token_production {
	static constexpr auto name = "mmx.lang.operator";
	static constexpr auto rule = dsl::literal_set(
			dsl::lit_c<'='>, dsl::lit_c<'+'>, dsl::lit_c<'-'>, dsl::lit_c<'*'>, dsl::lit_c<'/'>, dsl::lit_c<'!'>,
			dsl::lit_c<'>'>, dsl::lit_c<'<'>, LEXY_LIT(">="), LEXY_LIT("<="), LEXY_LIT("=="), LEXY_LIT("!="),
			LEXY_LIT("++"), LEXY_LIT("--"), LEXY_LIT(">>"), LEXY_LIT("<<"),
			kw_return, kw_do, kw_else, kw_of, kw_in);
};

struct qualifier : lexy::token_production {
	static constexpr auto name = "mmx.lang.qualifier";
	static constexpr auto rule = dsl::literal_set(kw_let, kw_var, kw_const);
};

struct variable {
	static constexpr auto name = "mmx.lang.variable";
	static constexpr auto rule = dsl::p<qualifier> >>
			dsl::p<identifier> + dsl::opt(dsl::equal_sign >> (dsl::p<constant> | dsl::else_ >> dsl::recurse<expression>));
};

struct list {
	static constexpr auto name = "mmx.lang.list";
	static constexpr auto rule = dsl::parenthesized.opt_list(dsl::recurse<expression>, dsl::sep(dsl::comma));
};

struct array {
	static constexpr auto name = "mmx.lang.array";
	static constexpr auto rule = dsl::square_bracketed.opt_list(dsl::recurse<expression>, dsl::trailing_sep(dsl::comma));
};

struct object {
	struct entry {
		static constexpr auto name = "mmx.lang.object.entry";
		static constexpr auto rule = (dsl::p<string> | dsl::p<identifier>) + dsl::lit_c<':'> + dsl::recurse<expression>;
	};
	static constexpr auto name = "mmx.lang.object";
	static constexpr auto rule = dsl::curly_bracketed.opt_list(dsl::p<entry>, dsl::trailing_sep(dsl::comma));
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
	static constexpr auto rule = dsl::loop(
			dsl::peek_not(dsl::comma | dsl::semicolon | dsl::lit_c<')'> | dsl::lit_c<']'> | dsl::lit_c<'}'> | dsl::eof) >> (
					dsl::p<list> | dsl::p<array> | dsl::p<object> | dsl::p<operator_ex> |
					dsl::p<if_ex> | dsl::p<for_loop> | dsl::p<while_loop> | dsl::p<constant> |
					dsl::p<call_ex> | dsl::p<identifier>
			) | dsl::break_);
};

struct statement {
	static constexpr auto name = "mmx.lang.statement";
	static constexpr auto rule = (dsl::p<variable> | dsl::else_ >> dsl::p<expression>) + dsl::semicolon;
};

struct source {
	static constexpr auto name = "mmx.lang.source";
	static constexpr auto whitespace = dsl::ascii::space;
	static constexpr auto rule = dsl::loop(
			dsl::peek_not(dsl::lit_c<'}'> | dsl::eof) >> (
					dsl::p<namespace_ex> | dsl::p<scope> | dsl::p<function> | dsl::else_ >> dsl::p<statement>
			) | dsl::break_);
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
	static constexpr uint32_t max_init_args = 0x100;

	std::shared_ptr<const contract::Binary> compile(const std::string& source);

protected:
	typedef lexy::parse_tree<lexy::input_reader<lexy::string_input<lexy::utf8_encoding>>> parse_tree_t;
	typedef typename parse_tree_t::node node_t;

	struct variable_t {
		bool is_const = false;
		bool is_static = false;
		uint32_t address = 0;
		std::string name;
		varptr_t value;
	};

	struct function_t {
		bool is_const = false;
		bool is_public = false;
		bool is_payable = false;
		uint32_t address = 0;
		std::string name;
		std::vector<variable_t> args;
		vnx::optional<node_t> root;
	};

	struct frame_t {
		uint32_t addr_offset = 0;
		std::vector<variable_t> var_list;
		std::map<std::string, uint32_t> var_map;
	};

	void parse(parse_tree_t& tree, const std::string& source);

	void recurse(const node_t& node, const uint32_t dst_addr = 0);

	const variable_t* find_variable(const std::string& name) const;

	variable_t get_variable(const std::string& name);

	uint32_t get_const_address(const varptr_t& value);

	std::string get_namespace(const bool concat = false) const;

	std::ostream& debug(bool ident = false) const;

	void print_debug_info(const node_t& node);

	static std::vector<node_t> get_children(const node_t& node);

	static std::string get_literal(const node_t& node);
	static varptr_t parse_constant(const node_t& node);

private:
	int depth = 0;
	int curr_pass = 0;
	bool is_debug = true;
	vnx::optional<node_t> curr_node;
	vnx::optional<function_t> curr_function;

	frame_t global;
	std::vector<instr_t> code;
	std::vector<frame_t> frame;
	std::vector<variable_t> const_vars;
	std::vector<std::string> name_space;

	std::map<varptr_t, uint32_t> const_table;
	std::map<std::string, function_t> function_map;

	std::shared_ptr<contract::Binary> binary;

	mutable std::stringstream debug_out;

};

const std::string Compiler::version = "0.0.0";

std::shared_ptr<const contract::Binary> Compiler::compile(const std::string& source)
{
	if(curr_pass) {
		return nullptr;
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
	{
		frame_t scope;
		scope.addr_offset = max_init_args;
		frame.push_back(scope);
	}

	try {
		debug() << "First pass ..." << std::endl;
		recurse(tree.root());

		curr_pass++;
		debug() << std::endl << "Second pass ..." << std::endl;

		for(const auto& entry : function_map) {
			recurse(*entry.second.root);
		}
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
	if(node.kind().is_token()) {
		const auto token = node.lexeme();
		return std::string(token.begin(), token.end());
	} else {
		const auto list = get_children(node);
		if(list.size() != 1) {
			throw std::logic_error("not a literal");
		}
		return get_literal(list[0]);
	}
}

varptr_t Compiler::parse_constant(const node_t& node)
{
	const std::string name(node.kind().name());
	if(name != lang::constant::name) {
		throw std::logic_error("not a constant");
	}
	for(const auto& node : get_children(node))
	{
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
		else if(name == lang::integer::hex::name) {
			return std::make_unique<uint_t>(uint256_t(get_literal(list[0]), 16));
		}
		else if(name == lang::integer::binary::name) {
			return std::make_unique<uint_t>(uint256_t(get_literal(list[0]), 2));
		}
		else if(name == lang::integer::decimal::name) {
			return std::make_unique<uint_t>(uint256_t(get_literal(list[0]), 10));
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

void Compiler::recurse(const node_t& node, const uint32_t dst_addr)
{
	curr_node = node;
	print_debug_info(node);
	depth++;

	bool is_scope = false;

	const std::string name(node.kind().name());
	const std::string p_name(node.parent().kind().name());
	const auto list = get_children(node);

	if(name == lang::namespace_ex::name) {
		if(list.size() < 2) {
			throw std::logic_error("invalid namespace declaration");
		}
		name_space.push_back(get_literal(list[1]));
	}
	else if(name == lang::scope::name) {
		if(list.size() < 2) {
			throw std::logic_error("invalid scope");
		}
		if(dst_addr) {
			recurse(list[1], dst_addr);
		} else {
			frame_t scope;
			if(frame.size() > 1) {
				scope.addr_offset = frame.back().addr_offset;
			}
			frame.push_back(scope);
			is_scope = true;
		}
	}
	else if(name == lang::source::name) {
		for(const auto& node : list) {
			recurse(node, dst_addr);
		}
	}
	else if(name == lang::function::name) {
		if(list.size() < 3) {
			throw std::logic_error("invalid function declaration");
		}
		if(curr_pass == 0) {
			function_t func;
			func.root = node;
			func.name = get_namespace(true) + get_literal(list[1]);
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
					var.name = get_literal(list[0]);
					var.address = MEM_STACK + 1 + func.args.size();

					debug(true) << "[" << func.args.size() << "] " << var.name;
					if(list.size() == 3) {
						var.value = parse_constant(list[2]);
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
		} else {
			// second pass
			const auto func_name = get_namespace(true) + get_literal(list[1]);
			debug(true) << "name = \"" << func_name << "\"" << std::endl;

			if(curr_function) {
				throw std::logic_error("cannot define function inside a function");
			}
			frame_t scope;
			{
				auto& func = function_map[func_name];
				func.address = code.size();

				scope.addr_offset = 1 + func.args.size();
				scope.var_list.emplace_back();
				scope.var_list.insert(scope.var_list.end(), func.args.begin(), func.args.end());
				curr_function = func;
			}
			frame.push_back(scope);
			recurse(list.back());
			frame.pop_back();

			if(code.empty() || code.back().code != OP_RET) {
				code.emplace_back(OP_RET);
			}
			curr_function = nullptr;
		}
	}
	else if(name == lang::variable::name) {
		if(list.size() < 2) {
			throw std::logic_error("invalid variable declaration");
		}
		if(curr_pass == 0 || frame.size() > 1)
		{
			const auto qualifier = get_literal(list[0]);

			variable_t var;
			var.is_const = qualifier == "const";
			var.is_static = frame.size() == 1;
			var.name = get_literal(list[1]);

			bool is_constant = false;
			bool is_expression = false;
			debug(true) << qualifier << " " << var.name;

			if(list.size() == 4) {
				const auto node = list[3];
				const std::string name(node.kind().name());
				if(name == lang::constant::name) {
					is_constant = true;
					var.value = parse_constant(node);
					debug() << " = " << to_string(var.value);
				}
				else if(name == lang::expression::name) {
					is_expression = true;
					debug() << " = <expression>";
				}
			}
			auto& scope = frame.size() > 1 ? frame.back() : global;

			if(is_constant) {
				var.is_static = false;
				var.address = get_const_address(var.value);
			} else {
				if(var.is_static) {
					var.address = MEM_STATIC + scope.addr_offset;
				} else {
					var.address = MEM_STACK + scope.addr_offset;
				}
				scope.addr_offset++;
			}
			debug() << " (0x" << std::hex << var.address << std::dec << ")" << std::endl;

			if(scope.var_map.count(var.name)) {
				throw std::logic_error("duplicate variable name: " + var.name);
			}
			scope.var_map[var.name] = scope.var_list.size();
			scope.var_list.push_back(var);

			if(is_expression) {
				recurse(list.back(), var.address);
			}
		}
	}
	else if(name == lang::statement::name) {
		if(list.size() < 1 || list.size() > 2) {
			throw std::logic_error("invalid statement");
		}
		recurse(list[0], dst_addr);
	}
	else if(name == lang::expression::name) {
		if(dst_addr) {
			//
		}
		// TODO
	}
	else if(name == lang::object::name) {
		// TODO
	}
	else if(name == lang::call_ex::name) {
		// TODO
	}

	if(name == lang::namespace_ex::name) {
		name_space.pop_back();
	}
	if(is_scope) {
		frame.pop_back();
	}
	depth--;
}

const Compiler::variable_t* Compiler::find_variable(const std::string& name) const
{
	for(auto iter = frame.rbegin(); iter != frame.rend(); ++iter) {
		auto iter2 = iter->var_map.find(name);
		if(iter2 != iter->var_map.end()) {
			return &iter->var_list[iter2->second];
		}
	}
	{
		auto iter = global.var_map.find(name);
		if(iter != global.var_map.end()) {
			return &global.var_list[iter->second];
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

uint32_t Compiler::get_const_address(const varptr_t& value)
{
	if(!value) {
		throw std::logic_error("get_const_addr(): !value");
	}
	{
		auto iter = const_table.find(value);
		if(iter != const_table.end()) {
			return MEM_CONST + iter->second;
		}
	}
	const auto addr = const_table[value] = const_vars.size();

	variable_t var;
	var.value = value;
	var.address = MEM_CONST + addr;
	const_vars.push_back(var);
	return var.address;
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
