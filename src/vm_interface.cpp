/*
 * vm_interface.cpp
 *
 *  Created on: May 12, 2022
 *      Author: mad
 */

#include <mmx/vm/Engine.h>
#include <mmx/vm_interface.h>
#include <mmx/uint128.hpp>

#include <vnx/vnx.h>


namespace mmx {
namespace vm {

const contract::method_t* find_method(std::shared_ptr<const contract::Binary> binary, const std::string& method_name)
{
	auto iter = binary->methods.find(method_name);
	if(iter != binary->methods.end()) {
		return &iter->second;
	}
	return nullptr;
}

void set_balance(std::shared_ptr<vm::Engine> engine, const std::map<addr_t, uint128>& balance)
{
	const auto addr = vm::MEM_EXTERN + vm::EXTERN_BALANCE;
	engine->assign(addr, std::make_unique<vm::map_t>());
	for(const auto& entry : balance) {
		engine->write_key(addr, to_binary(entry.first), std::make_unique<vm::uint_t>(entry.second));
	}
}

void set_deposit(std::shared_ptr<vm::Engine> engine, const addr_t& currency, const uint64_t amount)
{
	const auto addr = vm::MEM_EXTERN + vm::EXTERN_DEPOSIT;
	engine->assign(addr, std::make_unique<vm::array_t>());
	engine->push_back(addr, to_binary(currency));
	engine->push_back(addr, vm::uint_t(amount));
}

std::vector<std::unique_ptr<vm::var_t>> read_constants(const void* constant, const size_t constant_size)
{
	size_t offset = 0;
	std::vector<std::unique_ptr<vm::var_t>> out;
	while(offset < constant_size) {
		std::unique_ptr<vm::var_t> var;
		offset += vm::deserialize(var, ((const uint8_t*)constant) + offset, constant_size - offset, false, false);
		out.push_back(std::move(var));
	}
	return out;
}

std::vector<std::unique_ptr<vm::var_t>> read_constants(std::shared_ptr<const contract::Binary> binary)
{
	return read_constants(binary->constant.data(), binary->constant.size());
}

void load(	std::shared_ptr<vm::Engine> engine,
			std::shared_ptr<const contract::Binary> binary)
{
	uint64_t dst = 0;
	for(auto& var : read_constants(binary)) {
		if(dst < vm::MEM_EXTERN) {
			engine->assign(dst++, std::move(var));
		}
	}
	if(dst >= vm::MEM_EXTERN) {
		throw std::runtime_error("constant memory overflow");
	}
	vm::deserialize(engine->code, binary->binary.data(), binary->binary.size());

	engine->init();
	engine->check_gas();
}

void copy(std::shared_ptr<vm::Engine> dst, std::shared_ptr<vm::Engine> src, const uint64_t dst_addr, const uint64_t src_addr, size_t call_depth);

void copy(	std::shared_ptr<vm::Engine> dst, std::shared_ptr<vm::Engine> src,
			const uint64_t dst_addr, const uint64_t* dst_key, const vm::var_t* var, size_t call_depth)
{
	if(++call_depth > vm::MAX_COPY_RECURSION) {
		throw std::runtime_error("copy recursion overflow");
	}
	const vm::var_t* out = nullptr;
	std::unique_ptr<vm::var_t> tmp;
	if(var) {
		switch(var->type) {
			case vm::TYPE_REF: {
				const auto heap = dst->alloc();
				copy(dst, src, heap, ((const vm::ref_t*)var)->address, call_depth);
				tmp = std::make_unique<vm::ref_t>(heap);
				break;
			}
			case vm::TYPE_ARRAY: {
				if(dst_key) {
					throw std::logic_error("cannot assign array here");
				}
				const auto array = (const vm::array_t*)var;
				dst->assign(dst_addr, std::make_unique<vm::array_t>(array->size));

				for(uint64_t i = 0; i < array->size; ++i) {
					copy(dst, src, dst_addr, &i, src->read_entry(array->address, i), call_depth);
				}
				break;
			}
			case vm::TYPE_MAP: {
				if(dst_key) {
					throw std::logic_error("cannot assign map here");
				}
				const auto map = (const vm::map_t*)var;
				if(map->flags & FLAG_STORED) {
					throw std::logic_error("cannot copy map from storage at " + to_hex(map->address));
				}
				dst->assign(dst_addr, std::make_unique<vm::map_t>());

				for(const auto& entry : src->find_entries(map->address)) {
					const auto key = dst->lookup(src->read(entry.first), false);
					copy(dst, src, dst_addr, &key, entry.second, call_depth);
				}
				break;
			}
			default:
				out = var;
		}
	}
	if(!out) {
		out = tmp.get();
	}
	if(out) {
		if(dst_key) {
			dst->write_entry(dst_addr, *dst_key, *out);
		} else {
			dst->write(dst_addr, *out);
		}
	}
	dst->check_gas();
}

void copy(std::shared_ptr<vm::Engine> dst, std::shared_ptr<vm::Engine> src, const uint64_t dst_addr, const uint64_t src_addr, size_t call_depth)
{
	copy(dst, src, dst_addr, nullptr, src->read(src_addr), call_depth);
}

void copy(std::shared_ptr<vm::Engine> dst, std::shared_ptr<vm::Engine> src, const uint64_t dst_addr, const uint64_t src_addr)
{
	copy(dst, src, dst_addr, src_addr, 0);
}

class AssignTo : public vnx::Visitor {
public:
	// Note: This class is consensus critical.

	std::shared_ptr<vm::Engine> engine;

	AssignTo(std::shared_ptr<vm::Engine> engine, const uint64_t dst)
		:	engine(engine)
	{
		enable_binary = true;

		frame_t frame;
		frame.dst = dst;
		stack.push_back(frame);
	}

	void visit_null() override {
		handle(std::make_unique<vm::var_t>());
	}

	void visit(const bool& value) override {
		handle(std::make_unique<vm::var_t>(value ? vm::TYPE_TRUE : vm::TYPE_FALSE));
	}

	void visit(const uint8_t& value) override {
		visit(uint256_t(value));
	}
	void visit(const uint16_t& value) override {
		visit(uint256_t(value));
	}
	void visit(const uint32_t& value) override {
		visit(uint256_t(value));
	}
	void visit(const uint64_t& value) override {
		visit(uint256_t(value));
	}

	void visit(const int8_t& value) override {
		if(value < 0) {
			throw std::logic_error("negative values not supported");
		}
		visit(uint64_t(value));
	}
	void visit(const int16_t& value) override {
		if(value < 0) {
			throw std::logic_error("negative values not supported");
		}
		visit(uint64_t(value));
	}
	void visit(const int32_t& value) override {
		if(value < 0) {
			throw std::logic_error("negative values not supported");
		}
		visit(uint64_t(value));
	}
	void visit(const int64_t& value) override {
		if(value < 0) {
			throw std::logic_error("negative values not supported");
		}
		visit(uint64_t(value));
	}

	void visit(const vnx::float32_t& value) override {
		throw std::logic_error("type float not supported");
	}
	void visit(const vnx::float64_t& value) override {
		throw std::logic_error("type double not supported");
	}

	void visit(const std::string& value) override {
		handle(vm::binary_t::alloc(value));
	}

	void visit(const std::vector<uint8_t>& value) override {
		handle(vm::binary_t::alloc(value.data(), value.size()));
	}

	void visit(const uint256_t& value) {
		handle(std::make_unique<vm::uint_t>(value));
	}

	void list_begin(size_t size) override {
		const auto addr = engine->alloc();
		engine->assign(addr, std::make_unique<vm::array_t>(size));
		stack.emplace_back(addr);
	}
	void list_element(size_t index) override {
		stack.back().key = index;
	}
	void list_end(size_t size) override {
		const auto addr = stack.back().dst;
		stack.pop_back();
		handle(std::make_unique<vm::ref_t>(addr));
	}

	void map_begin(size_t size) override {
		const auto addr = engine->alloc();
		engine->assign(addr, std::make_unique<vm::map_t>());
		stack.emplace_back(addr);
	}
	void map_key(size_t index) override {
		stack.back().lookup = true;
	}
	void map_value(size_t index) override {
		stack.back().lookup = false;
	}
	void map_end(size_t size) override {
		const auto addr = stack.back().dst;
		stack.pop_back();
		handle(std::make_unique<vm::ref_t>(addr));
	}

private:
	void handle(std::unique_ptr<var_t> var) {
		auto& frame = stack.back();
		if(frame.lookup) {
			switch(var->type) {
				case TYPE_UINT:
				case TYPE_STRING:
				case TYPE_BINARY:
					break;
				default:
					throw std::logic_error("invalid key type: " + to_string(var->type));
			}
			frame.key = engine->lookup(*var, false);
		} else if(frame.key) {
			engine->write_entry(frame.dst, *frame.key, *var);
		} else {
			engine->assign(frame.dst, std::move(var));
		}
	}

private:
	struct frame_t {
		uint64_t dst = 0;
		vnx::optional<uint64_t> key;
		bool lookup = false;
		frame_t() = default;
		frame_t(const uint64_t& dst) : dst(dst) {}
	};
	std::vector<frame_t> stack;

};

void assign(std::shared_ptr<vm::Engine> engine, const uint64_t dst, const vnx::Variant& value)
{
	AssignTo visitor(engine, dst);
	vnx::accept(visitor, value);
}

vnx::Variant convert(std::shared_ptr<vm::Engine> engine, const vm::var_t* var)
{
	if(var) {
		switch(var->type) {
			case vm::TYPE_NIL:
				return vnx::Variant(nullptr);
			case vm::TYPE_TRUE:
				return vnx::Variant(true);
			case vm::TYPE_FALSE:
				return vnx::Variant(false);
			case vm::TYPE_REF:
				return convert(engine, engine->read(((const vm::ref_t*)var)->address));
			case vm::TYPE_UINT: {
				const auto& value = ((const vm::uint_t*)var)->value;
				if(value >> 64 == 0) {
					return vnx::Variant(value.lower().lower());
				}
				return vnx::Variant("0x" + value.str(16));
			}
			case vm::TYPE_STRING:
				return vnx::Variant(((const vm::binary_t*)var)->to_string());
			case vm::TYPE_BINARY:
				return vnx::Variant(((const vm::binary_t*)var)->to_vector());
			case vm::TYPE_ARRAY: {
				const auto array = (const vm::array_t*)var;
				std::vector<vnx::Variant> tmp;
				for(uint64_t i = 0; i < array->size; ++i) {
					tmp.push_back(convert(engine, engine->read_entry(array->address, i)));
				}
				return vnx::Variant(tmp);
			}
			case vm::TYPE_MAP: {
				const auto map = (const vm::map_t*)var;
				if(map->flags & FLAG_STORED) {
					throw std::logic_error("cannot read map from storage at " + to_hex(map->address));
				}
				std::map<vnx::Variant, vnx::Variant> tmp;
				for(const auto& entry : engine->find_entries(map->address)) {
					tmp[convert(engine, engine->read(entry.first))] = convert(engine, entry.second);
				}
				bool is_object = true;
				for(const auto& entry : tmp) {
					if(!entry.first.is_string()) {
						is_object = false;
						break;
					}
				}
				if(is_object) {
					vnx::Object obj;
					for(const auto& entry : tmp) {
						obj[entry.first.to_string_value()] = entry.second;
					}
					return vnx::Variant(obj);
				}
				return vnx::Variant(tmp);
			}
			default:
				break;
		}
	}
	return vnx::Variant();
}

vnx::Variant read(std::shared_ptr<vm::Engine> engine, const uint64_t address)
{
	return convert(engine, engine->read(address));
}

void set_args(std::shared_ptr<vm::Engine> engine, const std::vector<vnx::Variant>& args)
{
	for(size_t i = 0; i < args.size(); ++i) {
		const auto dst = vm::MEM_STACK + 1 + i;
		if(dst >= vm::MEM_STATIC) {
			throw std::runtime_error("stack overflow");
		}
		assign(engine, dst, args[i]);
	}
}

void execute(std::shared_ptr<vm::Engine> engine, const contract::method_t& method, const bool read_only)
{
	engine->begin(method.entry_point);
	engine->run();

	if(!method.is_const && !read_only) {
		engine->commit();
	}
	engine->check_gas();
}

void dump_code(std::ostream& out, std::shared_ptr<const contract::Binary> bin, const vnx::optional<std::string>& method)
{
	{
		size_t i = 0;
		out << "constants:" << std::endl;
		for(const auto& var : mmx::vm::read_constants(bin)) {
			out << "  [0x" << vnx::to_hex_string(i++) << "] " << mmx::vm::to_string(var.get()) << std::endl;
		}
	}
	out << "fields:" << std::endl;
	for(const auto& entry : bin->fields) {
		out << "  [0x" << vnx::to_hex_string(entry.second) << "] " << entry.first << std::endl;
	}
	std::map<size_t, mmx::contract::method_t> method_table;
	for(const auto& entry : bin->methods) {
		method_table[entry.second.entry_point] = entry.second;
	}
	std::vector<mmx::vm::instr_t> code;
	const auto length = mmx::vm::deserialize(code, bin->binary.data(), bin->binary.size());
	size_t i = 0;
	if(method) {
		auto iter = bin->methods.find(*method);
		if(iter == bin->methods.end()) {
			out << "No such method: " << *method << std::endl;
			return;
		}
		i = iter->second.entry_point;
	}
	out << "code:" << std::endl;

	for(; i < code.size(); ++i) {
		auto iter = method_table.find(i);
		if(iter != method_table.end()) {
			out << iter->second.name << " (";
			int k = 0;
			for(const auto& arg : iter->second.args) {
				if(k++) {
					out << ", ";
				}
				out << arg;
			}
			out << ")" << std::endl;
		}
		out << "  [0x" << vnx::to_hex_string(i) << "] " << to_string(code[i]) << std::endl;

		if(method && code[i].code == mmx::vm::OP_RET) {
			break;
		}
	}
	if(!method) {
		out << "Total size: " << length << " bytes" << std::endl;
		out << "Total instructions: " << code.size() << std::endl;
	}
}


} // vm
} // mmx
