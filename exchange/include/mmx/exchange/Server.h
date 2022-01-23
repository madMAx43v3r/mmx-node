/*
 * Server.h
 *
 *  Created on: Jan 22, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_EXCHANGE_SERVER_H_
#define INCLUDE_MMX_EXCHANGE_SERVER_H_

#include <mmx/exchange/ServerBase.hxx>
#include <mmx/exchange/ServerAsyncClient.hxx>
#include <mmx/exchange/order_t.hpp>
#include <mmx/exchange/trade_pair_t.hpp>
#include <mmx/NodeAsyncClient.hxx>
#include <mmx/Request.hxx>
#include <mmx/Return.hxx>
#include <mmx/utils.h>


namespace mmx {
namespace exchange {

class Server : ServerBase {
public:
	Server(const std::string& _vnx_name);

protected:
	struct peer_t : Super::peer_t {
		std::string address;
		std::unordered_set<addr_t> addr_set;
	};

	struct order_book_t {
		std::multimap<double, order_t> orders;
	};

	struct trade_job_t {
		int64_t start_time_ms = 0;
		vnx::request_id_t request_id;
		std::shared_ptr<Transaction> tx;
	};

	void main() override;

	void approve(const uint64_t& client, std::shared_ptr<const Transaction> tx) override;

	void place_async(const uint64_t& client, const trade_pair_t& pair, const limit_order_t& order, const vnx::request_id_t& request_id) override;

	void execute_async(std::shared_ptr<const Transaction> tx, const vnx::request_id_t& request_id) override;

	void match_async(const trade_pair_t& pair, const trade_order_t& order, const vnx::request_id_t& request_id) const override;

	std::vector<order_t> get_orders(const trade_pair_t& pair) const override;

	ulong_fraction_t get_price(const addr_t& want, const amount_t& have) const override;

private:
	std::shared_ptr<order_book_t> find_pair(const trade_pair_t& pair) const;

	void send_to(uint64_t client, std::shared_ptr<const vnx::Value> msg, bool reliable = true);

	void send_to(std::shared_ptr<peer_t> peer, std::shared_ptr<const vnx::Value> msg, bool reliable = true);

	void on_error(uint64_t client, uint32_t id, const vnx::exception& ex);

	void on_request(uint64_t client, std::shared_ptr<const Request> msg);

	void on_return(uint64_t client, std::shared_ptr<const Return> msg);

	void on_msg(uint64_t client, std::shared_ptr<const vnx::Value> msg);

	void on_connect(uint64_t client, const std::string& address) override;

	void on_disconnect(uint64_t client) override;

	std::shared_ptr<Super::peer_t> get_peer_base(uint64_t client) override;

	std::shared_ptr<peer_t> get_peer(uint64_t client);

	std::shared_ptr<peer_t> find_peer(uint64_t client);

private:
	std::shared_ptr<NodeAsyncClient> node;
	std::shared_ptr<ServerAsyncClient> server;
	std::shared_ptr<const ChainParams> params;

	std::unordered_map<addr_t, uint64_t> addr_map;								// [addr => client]
	std::unordered_map<txio_key_t, utxo_t> utxo_map;
	std::unordered_map<txio_key_t, hash_t> lock_map;							// [key => txid]
	std::map<trade_pair_t, std::shared_ptr<order_book_t>> trade_map;

	std::unordered_map<uint64_t, std::shared_ptr<peer_t>> peer_map;
	std::unordered_map<hash_t, std::shared_ptr<trade_job_t>> pending;

};


} // exchange
} // mmx

#endif /* INCLUDE_MMX_EXCHANGE_SERVER_H_ */
