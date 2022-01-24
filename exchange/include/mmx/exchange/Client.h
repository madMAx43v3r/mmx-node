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
#include <mmx/NodeClient.hxx>
#include <mmx/WalletClient.hxx>


namespace mmx {
namespace exchange {

class Client : public ClientBase {
public:
	Client(const std::string& _vnx_name);

protected:
	struct peer_t : Super::peer_t {
		std::string address;
		std::unordered_set<txio_key_t> order_set;
	};

	void init() override;

	void main() override;

	vnx::optional<open_order_t> get_order(const txio_key_t& key) const override;

	std::shared_ptr<const OrderBundle> get_offer(const uint64_t& id) const override;

	std::vector<std::shared_ptr<const OrderBundle>> get_all_offers() const override;

	std::shared_ptr<const OrderBundle> make_offer(const uint32_t& wallet, const trade_pair_t& pair, const uint64_t& bid, const uint64_t& ask) const override;

	void place(std::shared_ptr<const OrderBundle> offer) override;

	std::shared_ptr<const Transaction> approve(std::shared_ptr<const Transaction> tx) const override;

	void handle(std::shared_ptr<const Block> block) override;

private:
	void send_offer(uint64_t server, std::shared_ptr<const OrderBundle> offer);

	void send_to(uint64_t client, std::shared_ptr<const vnx::Value> msg, bool reliable = true);

	void send_to(std::shared_ptr<peer_t> peer, std::shared_ptr<const vnx::Value> msg, bool reliable = true);

	void on_error(uint64_t client, uint32_t id, const vnx::exception& ex);

	void on_request(uint64_t client, std::shared_ptr<const Request> msg);

	void on_return(uint64_t client, std::shared_ptr<const Return> msg);

	void on_msg(uint64_t client, std::shared_ptr<const vnx::Value> msg);

	void on_connect(uint64_t client, const std::string& address) override;

	void on_disconnect(uint64_t client) override;

	std::shared_ptr<Super::peer_t> get_peer_base(uint64_t client) const override;

	std::shared_ptr<peer_t> get_peer(uint64_t client) const;

	std::shared_ptr<peer_t> find_peer(uint64_t client) const;

private:
	std::shared_ptr<NodeClient> node;
	std::shared_ptr<WalletClient> wallet;

	std::set<uint64_t> server_set;

	std::unordered_map<txio_key_t, open_order_t> order_map;
	std::unordered_map<uint64_t, std::shared_ptr<OrderBundle>> offer_map;

	uint32_t next_request_id = 0;
	uint64_t next_offer_id = 0;

};


} // exchange
} // mmx

#endif /* INCLUDE_MMX_EXCHANGE_CLIENT_H_ */
