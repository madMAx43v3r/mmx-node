/*
 * StorageRAM.h
 *
 *  Created on: Apr 30, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_VM_STORAGERAM_H_
#define INCLUDE_MMX_VM_STORAGERAM_H_

#include <mmx/vm/Storage.h>

#include <map>
#include <tuple>
#include <mutex>
#include <unordered_map>


namespace mmx {
namespace vm {

class StorageRAM : public Storage {
public:
	~StorageRAM();

	// all methods below are thread-safe

	std::unique_ptr<var_t> read(const addr_t& contract, const uint64_t src) const override;

	std::unique_ptr<var_t> read(const addr_t& contract, const uint64_t src, const uint64_t key) const override;

	void write(const addr_t& contract, const uint64_t dst, const var_t& value) override;

	void write(const addr_t& contract, const uint64_t dst, const uint64_t key, const var_t& value) override;

	uint64_t lookup(const addr_t& contract, const var_t& value) const override;

	void clear();

	bool has_balances(const addr_t& contract) const;

	void set_balances(const addr_t& contract, const std::map<addr_t, uint128>& values);

	void set_balance(const addr_t& contract, const addr_t& currency, const uint128& amount) override;

	std::unique_ptr<uint128> get_balance(const addr_t& contract, const addr_t& currency) const override;

	std::map<addr_t, uint128> get_balances(const addr_t& contract) const override;

protected:
	mutable std::mutex mutex;
	std::map<std::pair<addr_t, uint64_t>, std::unique_ptr<var_t>> memory;
	std::map<std::tuple<addr_t, uint64_t, uint64_t>, std::unique_ptr<var_t>> entries;
	std::map<addr_t, std::map<const var_t*, uint64_t, varptr_less_t>> key_map;

	std::map<addr_t, std::map<addr_t, uint128>> balance_map;

};


} // vm
} // mmx

#endif /* INCLUDE_MMX_VM_STORAGERAM_H_ */
