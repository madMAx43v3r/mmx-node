/*
 * Engine.h
 *
 *  Created on: Apr 21, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_VM_ENGINE_H_
#define INCLUDE_MMX_VM_ENGINE_H_

#include <mmx/addr_t.hpp>
#include <mmx/txout_t.hxx>
#include <mmx/vm/var_t.h>
#include <mmx/vm/varptr_t.hpp>
#include <mmx/vm/instr_t.h>
#include <mmx/vm/Storage.h>

#include <set>
#include <map>
#include <limits>
#include <memory>
#include <string>
#include <sstream>


namespace mmx {
namespace vm {

class StorageProxy;

static constexpr uint64_t INSTR_COST = 100;
static constexpr uint64_t INSTR_READ_COST = 5;
static constexpr uint64_t SEND_COST = 20000;
static constexpr uint64_t MINT_COST = 10000;
static constexpr uint64_t WRITE_COST = 20;
static constexpr uint64_t WRITE_BYTE_COST = 1;
static constexpr uint64_t STOR_READ_COST = 5000;
static constexpr uint64_t STOR_WRITE_COST = 10000;
static constexpr uint64_t STOR_READ_BYTE_COST = 1;
static constexpr uint64_t STOR_WRITE_BYTE_COST = 100;
static constexpr uint64_t SHA256_BLOCK_COST = 10000;
static constexpr uint64_t ECDSA_VERIFY_COST = 50000;

enum externvar_e : uint32_t {

	EXTERN_HEIGHT,
	EXTERN_TXID,
	EXTERN_USER,
	EXTERN_BALANCE,
	EXTERN_DEPOSIT,		// [currency, amount]
	EXTERN_ADDRESS,

};

enum globalvar_e : uint32_t {

	GLOBAL_NEXT_ALLOC = 1,
	GLOBAL_LOG_HISTORY,
	GLOBAL_EVENT_HISTORY,
	GLOBAL_DYNAMIC_START = 0x1000

};


class Engine {
public:
	struct frame_t {
		uint64_t instr_ptr = 0;
		uint64_t stack_ptr = 0;
	};

	std::vector<instr_t> code;
	std::vector<frame_t> call_stack;

	std::vector<txout_t> outputs;
	std::vector<txout_t> mint_outputs;

	uint64_t total_gas = 0;
	uint64_t total_cost = 0;

	uint32_t error_code = 0;
	uint32_t error_addr = -1;

	const addr_t contract;
	const std::shared_ptr<StorageProxy> storage;

	std::function<void(const std::string& name, const std::string& method, const uint32_t nargs)> remote;

	Engine(const addr_t& contract, std::shared_ptr<Storage> backend, bool read_only);

	virtual ~Engine();

	void addref(const uint64_t dst);
	void unref(const uint64_t dst);

	var_t* assign(const uint64_t dst, std::unique_ptr<var_t> value);
	var_t* assign_entry(const uint64_t dst, const uint64_t key, std::unique_ptr<var_t> value);
	var_t* assign_key(const uint64_t dst, const uint64_t key, std::unique_ptr<var_t> value);

	uint64_t lookup(const uint64_t src, const bool read_only);
	uint64_t lookup(const var_t* var, const bool read_only);
	uint64_t lookup(const var_t& var, const bool read_only);
	uint64_t lookup(const varptr_t& var, const bool read_only);

	var_t* write(const uint64_t dst, const var_t* src);
	var_t* write(const uint64_t dst, const var_t& src);
	var_t* write(const uint64_t dst, const varptr_t& var);
	var_t* write(const uint64_t dst, const std::vector<varptr_t>& var);
	var_t* write(const uint64_t dst, const std::map<varptr_t, varptr_t>& var);

	var_t* write_entry(const uint64_t dst, const uint64_t key, const var_t& src);
	var_t* write_entry(const uint64_t dst, const uint64_t key, const varptr_t& var);
	void erase_entry(const uint64_t dst, const uint64_t key);

	var_t* write_key(const uint64_t dst, const uint64_t key, const var_t& var);
	var_t* write_key(const uint64_t dst, const var_t& key, const var_t& var);
	var_t* write_key(const uint64_t dst, const varptr_t& key, const varptr_t& var);
	void erase_key(const uint64_t dst, const uint64_t key);

	void push_back(const uint64_t dst, const var_t& var);
	void push_back(const uint64_t dst, const varptr_t& var);
	void push_back(const uint64_t dst, const uint64_t src);
	void pop_back(const uint64_t dst, const uint64_t& src);

	void erase(const uint64_t dst);

	var_t* read(const uint64_t src);
	var_t& read_fail(const uint64_t src);

	var_t* read_entry(const uint64_t src, const uint64_t key);
	var_t& read_entry_fail(const uint64_t src, const uint64_t key);

	var_t* read_key(const uint64_t src, const uint64_t key);
	var_t& read_key_fail(const uint64_t src, const uint64_t key);

	std::unique_ptr<array_t> clone_array(const uint64_t dst, const array_t& src);
	std::unique_ptr<map_t> clone_map(const uint64_t dst, const map_t& src);

	void copy(const uint64_t dst, const uint64_t src);
	void clone(const uint64_t dst, const uint64_t src);
	void get(const uint64_t dst, const uint64_t addr, const uint64_t key, const uint8_t flags);
	void set(const uint64_t addr, const uint64_t key, const uint64_t src, const uint8_t flags);
	void erase(const uint64_t addr, const uint64_t key, const uint8_t flags);
	void concat(const uint64_t dst, const uint64_t lhs, const uint64_t rhs);
	void memcpy(const uint64_t dst, const uint64_t src, const uint64_t count, const uint64_t offset);
	void conv(const uint64_t dst, const uint64_t src, const uint64_t dflags, const uint64_t sflags);
	void sha256(const uint64_t dst, const uint64_t src);
	void verify(const uint64_t dst, const uint64_t msg, const uint64_t pubkey, const uint64_t signature);
	void log(const uint64_t level, const uint64_t msg);
	void event(const uint64_t name, const uint64_t data);
	void send(const uint64_t address, const uint64_t amount, const uint64_t currency);
	void mint(const uint64_t address, const uint64_t amount);
	void rcall(const uint64_t name, const uint64_t method, const uint64_t stack_ptr, const uint64_t nargs);

	frame_t& get_frame();
	uint64_t deref(const uint64_t src);
	uint64_t alloc();

	void init();
	void begin(const uint64_t instr_ptr);
	void run();
	void step();
	void check_gas() const;
	void jump(const uint64_t instr_ptr);
	void call(const uint64_t instr_ptr, const uint64_t stack_ptr);
	bool ret();
	void exec(const instr_t& instr);

	void reset();
	void commit();

	void clear_stack(const uint64_t offset = 0);

	std::map<uint64_t, const var_t*> find_entries(const uint64_t dst) const;

	void dump_memory(const uint64_t begin = 0, const uint64_t end = -1);

	template<typename T>
	T* read(const uint64_t src, const vartype_e& type);
	template<typename T>
	T& read_fail(const uint64_t src, const vartype_e& type);
	template<typename T>
	T* read_key(const uint64_t src, const uint64_t key, const vartype_e& type);
	template<typename T>
	T& read_key_fail(const uint64_t src, const uint64_t key, const vartype_e& type);

private:
	var_t* assign(std::unique_ptr<var_t>& var, std::unique_ptr<var_t> value);
	var_t* write(std::unique_ptr<var_t>& var, const uint64_t* dst, const var_t& src);

	void clear(var_t* var);
	void erase(std::unique_ptr<var_t>& var);
	void erase_entries(const uint64_t dst);

	uint64_t deref_addr(uint32_t src, const bool flag);
	uint64_t deref_value(uint32_t src, const bool flag);

private:
	bool have_init = false;
	std::map<uint64_t, std::unique_ptr<var_t>> memory;
	std::map<std::pair<uint64_t, uint64_t>, std::unique_ptr<var_t>> entries;
	std::map<const var_t*, uint64_t, varptr_less_t> key_map;

};


template<typename T>
T* Engine::read(const uint64_t src, const vartype_e& type)
{
	if(auto var = read(src)) {
		if(var->type != type) {
			throw std::logic_error("read type mismatch: expected "
				+ to_string(type) + ", got " + to_string(var->type));
		}
		return (T*)var;
	}
	return nullptr;
}

template<typename T>
T& Engine::read_fail(const uint64_t src, const vartype_e& type)
{
	auto& var = read_fail(src);
	if(var.type != type) {
		throw std::logic_error("read type mismatch: expected "
				+ to_string(type) + ", got " + to_string(var.type));
	}
	return (T&)var;
}

template<typename T>
T* Engine::read_key(const uint64_t src, const uint64_t key, const vartype_e& type)
{
	if(auto var = read_key(src, key)) {
		if(var->type != type) {
			throw std::logic_error("read type mismatch: expected "
				+ to_string(type) + ", got " + to_string(var->type));
		}
		return (T*)var;
	}
	return nullptr;
}

template<typename T>
T& Engine::read_key_fail(const uint64_t src, const uint64_t key, const vartype_e& type)
{
	auto& var = read_key_fail(src, key);
	if(var.type != type) {
		throw std::logic_error("read type mismatch: expected "
				+ to_string(type) + ", got " + to_string(var.type));
	}
	return (T&)var;
}


} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_ENGINE_H_ */
