/*
 * Client.h
 *
 *  Created on: Jan 23, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_EXCHANGE_CLIENT_H_
#define INCLUDE_MMX_EXCHANGE_CLIENT_H_

#include <mmx/exchange/ClientBase.hxx>
#include <mmx/exchange/amount_t.hxx>
#include <mmx/exchange/matched_order_t.hxx>
#include <mmx/Request.hxx>
#include <mmx/Return.hxx>
#include <mmx/NodeClient.hxx>
#include <mmx/WalletClient.hxx>
#include <mmx/txio_key_t.hpp>

#include <vnx/ThreadPool.h>


namespace mmx {
namespace exchange {

class Client : public ClientBase {
public:
	Client(const std::string& _vnx_name);

protected:
	struct peer_t : Super::peer_t {
		std::string address;
		std::unordered_set<uint32_t> pending;
		std::unordered_set<txio_key_t> order_set;
	};

	struct match_job_t {
		size_t num_returns = 0;
		size_t num_requests = 0;
		vnx::request_id_t request_id;
		std::vector<matched_order_t> result;
	};

	void init() override;

	void main() override;

	std::vector<std::string> get_servers() const override;

	vnx::optional<open_order_t> get_order(const txio_key_t& key) const override;

	std::shared_ptr<const OfferBundle> get_offer(const uint64_t& id) const override;

	std::vector<std::shared_ptr<const OfferBundle>> get_all_offers() const override;

	void cancel_offer(const uint64_t& id) override;

	void cancel_all() override;

	std::shared_ptr<const OfferBundle> make_offer(const uint32_t& wallet, const trade_pair_t& pair, const uint64_t& bid, const uint64_t& ask) const override;

	std::vector<trade_order_t> make_trade(const uint32_t& wallet, const trade_pair_t& pair, const uint64_t& bid, const vnx::optional<uint64_t>& ask) const override;

	void place(std::shared_ptr<const OfferBundle> offer) override;

	std::shared_ptr<const Transaction> approve(std::shared_ptr<const Transaction> tx) const override;

	void execute_async(const std::string& server, const uint32_t& wallet, const matched_order_t& order, const vnx::request_id_t& request_id) const override;

	void match_async(const std::string& server, const trade_pair_t& pair, const std::vector<trade_order_t>& orders, const vnx::request_id_t& request_id) const override;

	void get_orders_async(const std::string& server, const trade_pair_t& pair, const vnx::request_id_t& request_id) const override;

	void get_price_async(const std::string& server, const addr_t& want, const amount_t& have, const vnx::request_id_t& request_id) const override;

	void handle(std::shared_ptr<const Block> block) override;

private:
	std::shared_ptr<OfferBundle> find_offer(const uint64_t& id) const;

	void send_offer(uint64_t server, std::shared_ptr<const OfferBundle> offer);

	void send_to(uint64_t client, std::shared_ptr<const vnx::Value> msg, bool reliable = true);

	void send_to(std::shared_ptr<peer_t> peer, std::shared_ptr<const vnx::Value> msg, bool reliable = true);

	uint32_t send_request(	std::shared_ptr<peer_t> peer, std::shared_ptr<const vnx::Value> method,
							const std::function<void(std::shared_ptr<const vnx::Value>)>& callback = {}) const;

	void connect();

	void add_peer(const std::string& address, const int sock);

	void connect_task(const std::string& address) noexcept;

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

	std::shared_ptr<peer_t> get_server(const std::string& name) const;

private:
	std::shared_ptr<NodeClient> node;
	std::shared_ptr<WalletClient> wallet;

	std::set<std::string> connecting;										// [address]
	std::map<std::string, uint64_t> avail_server_map;						// [address => client]
	std::unordered_map<uint64_t, std::shared_ptr<peer_t>> peer_map;

	std::unordered_map<txio_key_t, open_order_t> order_map;
	std::map<uint64_t, std::shared_ptr<OfferBundle>> offer_map;

	mutable std::unordered_map<uint32_t, std::function<void(std::shared_ptr<const vnx::Value>)>> return_map;

	vnx::ThreadPool* threads = nullptr;

	mutable uint32_t next_request_id = 0;
	mutable uint64_t next_offer_id = 0;

};


} // exchange
} // mmx

#endif /* INCLUDE_MMX_EXCHANGE_CLIENT_H_ */
