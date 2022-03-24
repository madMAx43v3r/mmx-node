/*
 * Server.h
 *
 *  Created on: Jan 22, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_EXCHANGE_SERVER_H_
#define INCLUDE_MMX_EXCHANGE_SERVER_H_

#include <mmx/exchange/ServerBase.hxx>
#include <mmx/exchange/trade_pair_t.hpp>
#include <mmx/NodeAsyncClient.hxx>
#include <mmx/Request.hxx>
#include <mmx/Return.hxx>
#include <mmx/txio_key_t.hpp>
#include <mmx/utils.h>

#include <vnx/GenericAsyncClient.h>
#include <vnx/rocksdb/multi_table.h>


namespace mmx {
namespace exchange {

class Server : public ServerBase {
public:
	Server(const std::string& _vnx_name);

protected:
	struct peer_t : Super::peer_t {
		bool is_blocked = false;
		bool is_outbound = false;
		std::string address;
		std::unordered_map<txio_key_t, trade_pair_t> order_map;
	};

	struct order_book_t {
		std::multimap<double, order_t> orders;
		std::unordered_map<txio_key_t, double> key_map;
		vnx::optional<trade_entry_t> last_trade;
	};

	struct trade_job_t {
		int64_t start_time_ms = 0;
		vnx::request_id_t request_id;
		std::shared_ptr<Transaction> tx;
		std::vector<txio_key_t> locked_keys;
		std::unordered_set<uint64_t> pending_clients;
	};

	void init() override;

	void main() override;

	void ping(const uint64_t& client) const override;

	void cancel(const uint64_t& client, const std::vector<txio_key_t>& orders) override;

	void reject(const uint64_t& client, const hash_t& txid) override;

	void approve(const uint64_t& client, std::shared_ptr<const Transaction> tx) override;

	void place_async(const uint64_t& client, const trade_pair_t& pair, const limit_order_t& order, const vnx::request_id_t& request_id) const override;

	void execute_async(std::shared_ptr<const Transaction> tx, const vnx::request_id_t& request_id) override;

	void match_async(const trade_order_t& order, const vnx::request_id_t& request_id) const override;

	std::vector<trade_pair_t> get_trade_pairs() const override;

	std::vector<order_t> get_orders(const trade_pair_t& pair, const int32_t& limit) const override;

	std::vector<trade_entry_t> get_history(const trade_pair_t& pair, const int32_t& limit) const override;

	ulong_fraction_t get_price(const addr_t& want, const amount_t& have) const override;

	ulong_fraction_t get_min_trade(const trade_pair_t& pair) const override;

	void handle(std::shared_ptr<const Block> block) override;

private:
	void update();

	bool is_open(const txio_key_t& bid_key) const;

	void finish_trade(std::shared_ptr<trade_job_t> job);

	void cancel_trade(std::shared_ptr<trade_job_t> job);

	void cancel_order(const trade_pair_t& pair, const txio_key_t& key);

	std::shared_ptr<order_book_t> find_pair(const trade_pair_t& pair) const;

	std::shared_ptr<order_book_t> get_pair(const trade_pair_t& pair) const;

	void send_to(uint64_t client, std::shared_ptr<const vnx::Value> msg, bool reliable = true);

	void send_to(std::shared_ptr<peer_t> peer, std::shared_ptr<const vnx::Value> msg, bool reliable = true);

	void on_error(uint64_t client, uint32_t id, const vnx::exception& ex);

	void on_request(uint64_t client, std::shared_ptr<const Request> msg);

	void on_return(uint64_t client, std::shared_ptr<const Return> msg);

	void on_msg(uint64_t client, std::shared_ptr<const vnx::Value> msg);

	void on_pause(uint64_t client) override;

	void on_resume(uint64_t client) override;

	void on_connect(uint64_t client, const std::string& address) override;

	void on_disconnect(uint64_t client) override;

	std::shared_ptr<Super::peer_t> get_peer_base(uint64_t client) const override;

	std::shared_ptr<peer_t> get_peer(uint64_t client) const;

	std::shared_ptr<peer_t> find_peer(uint64_t client) const;

private:
	std::shared_ptr<NodeAsyncClient> node;
	std::shared_ptr<vnx::GenericAsyncClient> server;
	std::shared_ptr<const ChainParams> params;

	mutable std::unordered_map<txio_key_t, utxo_t> utxo_map;
	mutable std::unordered_map<txio_key_t, uint64_t> owner_map;					// [addr => client]

	std::unordered_map<txio_key_t, hash_t> lock_map;							// [key => txid]

	mutable std::map<trade_pair_t, std::shared_ptr<order_book_t>> trade_map;

	vnx::rocksdb::multi_table<trade_pair_t, trade_entry_t, uint64_t> trade_history;

	std::unordered_map<uint64_t, std::shared_ptr<peer_t>> peer_map;
	std::unordered_map<hash_t, std::shared_ptr<trade_job_t>> pending_trades;

};


} // exchange
} // mmx

#endif /* INCLUDE_MMX_EXCHANGE_SERVER_H_ */
