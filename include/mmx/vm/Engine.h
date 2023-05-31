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

static constexpr uint64_t INSTR_COST = 20;
static constexpr uint64_t INSTR_CALL_COST = 30;
static constexpr uint64_t INSTR_MUL_128_COST = 50;
static constexpr uint64_t INSTR_MUL_256_COST = 200;
static constexpr uint64_t INSTR_DIV_64_COST = 50;
static constexpr uint64_t INSTR_DIV_128_COST = 1000;
static constexpr uint64_t INSTR_DIV_256_COST = 10000;

static constexpr uint64_t WRITE_COST = 50;
static constexpr uint64_t WRITE_32_BYTE_COST = 30;
static constexpr uint64_t STOR_READ_COST = 2000;
static constexpr uint64_t STOR_WRITE_COST = 2000;
static constexpr uint64_t STOR_READ_BYTE_COST = 2;
static constexpr uint64_t STOR_WRITE_BYTE_COST = 50;

static constexpr uint64_t CONV_STRING_UINT_CHAR_COST = 200;
static constexpr uint64_t CONV_UINT_32_STRING_COST = 500;
static constexpr uint64_t CONV_UINT_64_STRING_COST = 1000;
static constexpr uint64_t CONV_UINT_128_STRING_COST = 25000;
static constexpr uint64_t CONV_UINT_256_STRING_COST = 300000;
static constexpr uint64_t CONV_STRING_HEX_BINARY_BYTE_COST = 10;
static constexpr uint64_t CONV_STRING_BECH32_COST = 1000;
static constexpr uint64_t CONV_BECH32_STRING_COST = 1000;

static constexpr uint64_t SHA256_BLOCK_COST = 2000;
static constexpr uint64_t ECDSA_VERIFY_COST = 20000;

static constexpr uint64_t MAX_KEY_BYTES = 256;
static constexpr uint64_t MAX_VALUE_BYTES = 16 * 1024;
static constexpr uint64_t MAX_CALL_RECURSION = 1000;
static constexpr uint64_t MAX_ERASE_RECURSION = 100;
static constexpr uint64_t MAX_COPY_RECURSION = 100;

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

	uint64_t gas_used = 0;
	uint64_t gas_limit = 0;

	uint32_t error_code = 0;
	uint32_t error_addr = -1;

	const addr_t contract;

	bool is_debug = false;

	std::function<void(uint32_t level, const std::string& msg)> log_func;
	std::function<void(const std::string& name, const uint64_t data)> event_func;
	std::function<void(const std::string& name, const std::string& method, const uint32_t nargs)> remote_call;
	std::function<void(const addr_t& address, const std::string& field, const uint64_t dst)> read_contract;

	Engine(const addr_t& contract, std::shared_ptr<Storage> backend, bool read_only);

	virtual ~Engine();

	void addref(const uint64_t dst);
	void unref(const uint64_t dst);

	var_t* assign(const uint64_t dst, std::unique_ptr<var_t> value);

	uint64_t lookup(const uint64_t src, const bool read_only);
	uint64_t lookup(const var_t* var, const bool read_only);
	uint64_t lookup(const var_t& var, const bool read_only);
	uint64_t lookup(const varptr_t& var, const bool read_only);

	var_t* write(const uint64_t dst, const var_t* src);
	var_t* write(const uint64_t dst, const var_t& src);
	var_t* write(const uint64_t dst, const varptr_t& var);

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

	var_t* read(const uint64_t src, const bool mem_only = false);
	var_t& read_fail(const uint64_t src);

	var_t* read_entry(const uint64_t src, const uint64_t key);
	var_t& read_entry_fail(const uint64_t src, const uint64_t key);

	var_t* read_key(const uint64_t src, const uint64_t key);
	var_t* read_key(const uint64_t src, const var_t& key);
	var_t* read_key(const uint64_t src, const varptr_t& key);
	var_t& read_key_fail(const uint64_t src, const uint64_t key);

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
	void send(const uint64_t address, const uint64_t amount, const uint64_t currency, const uint64_t memo);
	void mint(const uint64_t address, const uint64_t amount, const uint64_t memo);
	void rcall(const uint64_t name, const uint64_t method, const uint64_t stack_ptr, const uint64_t nargs);
	void cread(const uint64_t dst, const uint64_t address, const uint64_t field);

	frame_t& get_frame();
	uint64_t deref(const uint64_t src);
	uint64_t alloc();

	void init();
	void begin(const uint64_t instr_ptr);
	void run();
	void step();
	void check_gas();
	void jump(const uint64_t instr_ptr);
	void call(const uint64_t instr_ptr, const uint64_t stack_ptr);
	bool ret();
	void exec(const instr_t& instr);

	void reset();
	void commit();

	void clear_stack(const uint64_t offset = 0);

	vnx::optional<memo_t> parse_memo(const uint64_t addr);

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
	std::shared_ptr<StorageProxy> storage;
	std::map<uint64_t, std::unique_ptr<var_t>> memory;
	std::map<std::pair<uint64_t, uint64_t>, std::unique_ptr<var_t>> entries;
	std::map<const var_t*, uint64_t, varptr_less_t> key_map;

	size_t erase_call_depth = 0;

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
