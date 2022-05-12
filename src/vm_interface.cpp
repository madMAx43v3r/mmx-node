/*
 * vm_interface.cpp
 *
 *  Created on: May 12, 2022
 *      Author: mad
 */

#include <mmx/vm_interface.h>
#include <mmx/uint128.hpp>


namespace mmx {

const contract::method_t* find_method(std::shared_ptr<const contract::Executable> executable, const std::string& method_name)
{
	auto iter = executable->methods.find(method_name);
	if(iter != executable->methods.end()) {
		return &iter->second;
	}
	return nullptr;
}

void set_balance(std::shared_ptr<vm::Engine> engine, const std::map<addr_t, uint128>& balance)
{
	const auto addr = vm::MEM_EXTERN + vm::EXTERN_BALANCE;
	engine->assign(addr, new vm::map_t());
	for(const auto& entry : balance) {
		engine->write_key(addr, vm::uint_t(entry.first), vm::uint_t(entry.second));
	}
}

void set_deposit(std::shared_ptr<vm::Engine> engine, const txout_t& deposit)
{
	const auto addr = vm::MEM_EXTERN + vm::EXTERN_DEPOSIT;
	engine->assign(addr, new vm::array_t());
	engine->push_back(addr, vm::uint_t(deposit.contract));
	engine->push_back(addr, vm::uint_t(deposit.amount));
	if(deposit.sender) {
		engine->push_back(addr, vm::uint_t(*deposit.sender));
	} else {
		engine->push_back(addr, vm::var_t());
	}
}

std::vector<vm::varptr_t> get_constants(const void* constant, const size_t constant_size)
{
	size_t offset = 0;
	std::vector<vm::varptr_t> out;
	while(offset < constant_size) {
		vm::var_t* var = nullptr;
		offset += vm::deserialize(var,
				((const uint8_t*)constant) + offset, constant_size - offset, false, false);
		if(out.size() >= vm::MEM_EXTERN) {
			throw std::runtime_error("constant memory overflow");
		}
		out.push_back(var);
	}
	return out;
}

std::vector<vm::varptr_t> get_constants(std::shared_ptr<const contract::Executable> exec)
{
	return get_constants(exec->constant.data(), exec->constant.size());
}

void load(	std::shared_ptr<vm::Engine> engine,
			std::shared_ptr<const contract::Executable> exec)
{
	uint64_t dst = 0;
	for(auto& var : get_constants(exec)) {
		engine->assign(dst++, var.release());
	}
	vm::deserialize(engine->code, exec->binary.data(), exec->binary.size());
	engine->init();
}

class AssignTo : public vnx::Visitor {
public:
	std::shared_ptr<vm::Engine> engine;

	AssignTo(std::shared_ptr<vm::Engine> engine, const uint64_t dst)
		:	engine(engine)
	{
		frame_t frame;
		frame.dst = dst;
		stack.push_back(frame);
	}

	void visit_null() override {
		auto& frame = stack.back();
		if(frame.lookup) {
			frame.key = engine->lookup(vm::var_t(), false);
		} else if(frame.key) {
			engine->write_entry(frame.dst, *frame.key, vm::var_t());
		} else {
			engine->write(frame.dst, vm::var_t());
		}
	}
	void visit(const bool& value) override {
		const auto var = vm::var_t(value ? vm::TYPE_TRUE : vm::TYPE_FALSE);
		auto& frame = stack.back();
		if(frame.lookup) {
			frame.key = engine->lookup(var, false);
		} else if(frame.key) {
			engine->write_entry(frame.dst, *frame.key, var);
		} else {
			engine->write(frame.dst, var);
		}
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
		visit(uint256_t(value));
	}
	void visit(const int16_t& value) override {
		visit(uint256_t(value));
	}
	void visit(const int32_t& value) override {
		visit(uint256_t(value));
	}
	void visit(const int64_t& value) override {
		visit(uint256_t(value));
	}
	void visit(const vnx::float32_t& value) override {
		visit(uint256_t(int64_t(value)));
	}
	void visit(const vnx::float64_t& value) override {
		visit(uint256_t(int64_t(value)));
	}
	void visit(const std::string& value) override {
		const auto var = vm::binary_t::alloc(value);
		auto& frame = stack.back();
		if(frame.lookup) {
			frame.key = engine->lookup(var, false);
			delete var;
		} else if(frame.key) {
			engine->assign_entry(frame.dst, *frame.key, var);
		} else {
			engine->assign(frame.dst, var);
		}
	}
	void visit(const uint256_t& value) {
		const auto var = vm::uint_t(value);
		auto& frame = stack.back();
		if(frame.lookup) {
			frame.key = engine->lookup(var, false);
		} else if(frame.key) {
			engine->write_entry(frame.dst, *frame.key, var);
		} else {
			engine->write(frame.dst, var);
		}
	}

	void list_begin(size_t size) override {
		const auto addr = engine->alloc();
		engine->assign(addr, new vm::array_t());
		stack.emplace_back(addr);
	}
	void list_element(size_t index) override {
		stack.back().key = index;
	}
	void list_end(size_t size) override {
		const auto addr = stack.back().dst;
		stack.pop_back();
		const auto& frame = stack.back();
		if(frame.lookup) {
			throw std::logic_error("key type not supported");
		}
		if(frame.key) {
			engine->write_entry(frame.dst, *frame.key, vm::ref_t(addr));
		} else {
			engine->write(frame.dst, vm::ref_t(addr));
		}
	}

	void map_begin(size_t size) override {
		const auto addr = engine->alloc();
		engine->assign(addr, new vm::map_t());
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
		const auto& frame = stack.back();
		if(frame.lookup) {
			throw std::logic_error("key type not supported");
		}
		if(frame.key) {
			engine->write_entry(frame.dst, *frame.key, vm::ref_t(addr));
		} else {
			engine->write(frame.dst, vm::ref_t(addr));
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
				if(!value.upper()) {
					if(!value.lower().upper()) {
						return vnx::Variant(value.lower().lower());
					}
					return vnx::Variant(mmx::uint128(value.lower()));
				}
				return vnx::Variant(hash_t::from_bytes(value));
			}
			case vm::TYPE_STRING:
				return vnx::Variant(((const vm::binary_t*)var)->to_string());
			case vm::TYPE_BINARY:
				return vnx::Variant(((const vm::binary_t*)var)->to_hex_string());
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
				std::map<vnx::Variant, vnx::Variant> tmp;
				for(const auto& entry : engine->find_entries(map->address)) {
					tmp[convert(engine, engine->read(entry.first))] = convert(engine, entry.second);
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

void execute(	std::shared_ptr<vm::Engine> engine,
				const contract::method_t& method, const std::vector<vnx::Variant>& args, const uint64_t total_gas)
{
	for(size_t i = 0; i < args.size(); ++i) {
		assign(engine, vm::MEM_STACK + 1 + i, args[i]);
	}
	engine->total_gas = total_gas;
	engine->begin(method.entry_point);
	engine->run();

	if(!method.is_const) {
		engine->commit();
	}
	if(engine->total_cost > engine->total_gas) {
		throw std::logic_error("insufficient gas: " + std::to_string(engine->total_cost) + " > " + std::to_string(engine->total_gas));
	}
}


} // mmx
