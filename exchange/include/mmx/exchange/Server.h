/*
 * Server.h
 *
 *  Created on: Jan 22, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_EXCHANGE_SERVER_H_
#define INCLUDE_MMX_EXCHANGE_SERVER_H_

#include <mmx/exchange/ServerBase.hxx>
#include <mmx/exchange/order_t.hpp>
#include <mmx/exchange/trade_pair_t.hpp>
#include <mmx/NodeAsyncClient.hxx>


namespace mmx {
namespace exchange {

class Server : ServerBase {
public:
	Server(const std::string& _vnx_name);

protected:
	struct order_book_t {
		std::multimap<double, order_t> orders;
	};

	void approve(std::shared_ptr<const Transaction> tx) override;

	void place_async(const trade_pair_t& pair, const limit_order_t& order, const vnx::request_id_t& request_id) override;

	void execute_async(const trade_pair_t& pair, const trade_order_t& order, const vnx::request_id_t& request_id) const override;

	std::vector<order_t> get_orders(const trade_pair_t& pair) const override;

	ulong_fraction_t get_price(const addr_t& want, const amount_t& have) const override;

private:
	std::shared_ptr<order_book_t> find_pair(const trade_pair_t& pair) const;

private:
	std::shared_ptr<NodeAsyncClient> node;

	std::unordered_map<txio_key_t, utxo_t> utxo_map;
	std::map<trade_pair_t, std::shared_ptr<order_book_t>> trade_map;

};


} // exchange
} // mmx

#endif /* INCLUDE_MMX_EXCHANGE_SERVER_H_ */
